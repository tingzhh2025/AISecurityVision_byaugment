#include "AccessLogger.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <filesystem>

using namespace AISecurityVision;

AccessLogger::AccessLogger(const LogConfig& config)
    : m_config(config), m_currentFileSize(0), m_lastFlush(std::chrono::steady_clock::now()) {
    
    // 创建日志目录
    if (!std::filesystem::exists(m_config.logDirectory)) {
        std::filesystem::create_directories(m_config.logDirectory);
    }
    
    LOG_INFO() << "[AccessLogger] Initialized with config: " 
               << "dir=" << m_config.logDirectory 
               << ", prefix=" << m_config.logFilePrefix
               << ", maxSize=" << m_config.maxFileSize;
}

AccessLogger::~AccessLogger() {
    stop();
}

void AccessLogger::logAccess(const AccessLogEntry& entry) {
    if (!m_running) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logQueue.push(entry);
    m_condition.notify_one();
    
    // 更新统计信息
    updateStats(entry);
    
    // 保存到最近日志
    m_recentLogs.push(entry);
    if (m_recentLogs.size() > MAX_RECENT_LOGS) {
        m_recentLogs.pop();
    }
}

void AccessLogger::logApiAccess(const std::string& clientIp, const std::string& method, 
                               const std::string& endpoint, int statusCode, 
                               std::chrono::milliseconds responseTime,
                               const std::string& userId) {
    AccessLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.clientIp = clientIp;
    entry.userId = userId;
    entry.method = method;
    entry.endpoint = endpoint;
    entry.statusCode = statusCode;
    entry.responseTime = responseTime;
    
    logAccess(entry);
}

void AccessLogger::logSecurityEvent(const std::string& clientIp, const std::string& eventType,
                                   const std::string& description, int severity) {
    AccessLogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.clientIp = clientIp;
    entry.method = "SECURITY";
    entry.endpoint = eventType;
    entry.errorMessage = description;
    entry.statusCode = 900 + severity; // 特殊状态码表示安全事件
    
    logAccess(entry);
    
    LOG_WARN() << "[AccessLogger] Security event: " << eventType 
               << " from " << clientIp << " - " << description;
}

AccessLogger::AccessStats AccessLogger::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

std::string AccessLogger::getStatsJson() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    nlohmann::json stats;
    stats["total_requests"] = m_stats.totalRequests;
    stats["successful_requests"] = m_stats.successfulRequests;
    stats["failed_requests"] = m_stats.failedRequests;
    stats["rate_limited_requests"] = m_stats.rateLimitedRequests;
    stats["auth_failed_requests"] = m_stats.authFailedRequests;
    stats["average_response_time"] = m_stats.averageResponseTime;
    stats["total_data_transferred"] = m_stats.totalDataTransferred;
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_stats.startTime).count();
    stats["uptime_seconds"] = uptime;
    
    return stats.dump();
}

std::vector<AccessLogger::AccessLogEntry> AccessLogger::getRecentLogs(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AccessLogEntry> logs;
    auto tempQueue = m_recentLogs;
    
    while (!tempQueue.empty() && logs.size() < count) {
        logs.push_back(tempQueue.front());
        tempQueue.pop();
    }
    
    return logs;
}

void AccessLogger::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = AccessStats();
    LOG_INFO() << "[AccessLogger] Statistics reset";
}

void AccessLogger::flush() {
    if (m_logFile && m_logFile->is_open()) {
        m_logFile->flush();
        m_lastFlush = std::chrono::steady_clock::now();
    }
}

void AccessLogger::setConfig(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    LOG_INFO() << "[AccessLogger] Configuration updated";
}

void AccessLogger::start() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_writerThread = std::make_unique<std::thread>(&AccessLogger::logWriterThread, this);
    
    LOG_INFO() << "[AccessLogger] Started";
}

void AccessLogger::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_condition.notify_all();
    
    if (m_writerThread && m_writerThread->joinable()) {
        m_writerThread->join();
    }
    
    if (m_logFile && m_logFile->is_open()) {
        m_logFile->close();
    }
    
    LOG_INFO() << "[AccessLogger] Stopped";
}

bool AccessLogger::isRunning() const {
    return m_running;
}

void AccessLogger::logWriterThread() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        // 等待日志条目或停止信号
        m_condition.wait(lock, [this] { return !m_logQueue.empty() || !m_running; });
        
        if (!m_running && m_logQueue.empty()) {
            break;
        }
        
        // 处理所有待写入的日志
        while (!m_logQueue.empty()) {
            auto entry = m_logQueue.front();
            m_logQueue.pop();
            
            lock.unlock();
            
            // 检查是否需要轮转日志文件
            if (needsRotation()) {
                rotateLogFile();
            }
            
            // 确保日志文件已打开
            if (!m_logFile || !m_logFile->is_open()) {
                m_currentLogFilePath = getCurrentLogFilePath();
                m_logFile = std::make_unique<std::ofstream>(m_currentLogFilePath, std::ios::app);
                m_currentFileSize = std::filesystem::file_size(m_currentLogFilePath);
            }
            
            // 写入日志
            if (m_logFile && m_logFile->is_open()) {
                std::string logLine = formatLogEntry(entry);
                *m_logFile << logLine << std::endl;
                m_currentFileSize += logLine.length() + 1;
                
                // 控制台输出（如果启用）
                if (m_config.enableConsoleOutput) {
                    std::cout << logLine << std::endl;
                }
            }
            
            lock.lock();
        }
        
        // 定期刷新
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastFlush).count() >= m_config.flushIntervalSeconds) {
            lock.unlock();
            flush();
            lock.lock();
        }
    }
}

std::string AccessLogger::formatLogEntry(const AccessLogEntry& entry) const {
    if (m_config.enableJsonFormat) {
        return formatAsJson(entry);
    } else {
        return formatAsText(entry);
    }
}

std::string AccessLogger::formatAsJson(const AccessLogEntry& entry) const {
    nlohmann::json log;
    
    log["timestamp"] = getTimestampString(entry.timestamp);
    log["client_ip"] = entry.clientIp;
    log["user_id"] = entry.userId;
    log["method"] = entry.method;
    log["endpoint"] = entry.endpoint;
    log["user_agent"] = entry.userAgent;
    log["referer"] = entry.referer;
    log["status_code"] = entry.statusCode;
    log["request_size"] = entry.requestSize;
    log["response_size"] = entry.responseSize;
    log["response_time_ms"] = entry.responseTime.count();
    log["error_message"] = entry.errorMessage;
    log["rate_limited"] = entry.rateLimited;
    log["auth_failed"] = entry.authFailed;
    
    return log.dump();
}

std::string AccessLogger::formatAsText(const AccessLogEntry& entry) const {
    std::ostringstream oss;
    
    oss << getTimestampString(entry.timestamp) << " "
        << entry.clientIp << " "
        << "\"" << entry.method << " " << entry.endpoint << "\" "
        << entry.statusCode << " "
        << entry.responseSize << " "
        << entry.responseTime.count() << "ms";
    
    if (!entry.userId.empty()) {
        oss << " user=" << entry.userId;
    }
    
    if (entry.rateLimited) {
        oss << " [RATE_LIMITED]";
    }
    
    if (entry.authFailed) {
        oss << " [AUTH_FAILED]";
    }
    
    if (!entry.errorMessage.empty()) {
        oss << " error=\"" << entry.errorMessage << "\"";
    }
    
    return oss.str();
}

bool AccessLogger::needsRotation() const {
    return m_currentFileSize >= m_config.maxFileSize;
}

void AccessLogger::rotateLogFile() {
    if (m_logFile && m_logFile->is_open()) {
        m_logFile->close();
    }
    
    // 重命名当前文件
    auto timestamp = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    
    std::ostringstream oss;
    oss << m_config.logDirectory << "/" << m_config.logFilePrefix 
        << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".log";
    
    std::string rotatedPath = oss.str();
    
    try {
        std::filesystem::rename(m_currentLogFilePath, rotatedPath);
        LOG_INFO() << "[AccessLogger] Rotated log file to: " << rotatedPath;
    } catch (const std::exception& e) {
        LOG_ERROR() << "[AccessLogger] Failed to rotate log file: " << e.what();
    }
    
    // 清理旧文件
    cleanupOldLogFiles();
    
    // 重置文件大小
    m_currentFileSize = 0;
}

std::string AccessLogger::getCurrentLogFilePath() const {
    return m_config.logDirectory + "/" + m_config.logFilePrefix + ".log";
}

std::string AccessLogger::getTimestampString(const std::chrono::system_clock::time_point& timePoint) const {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

void AccessLogger::updateStats(const AccessLogEntry& entry) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_stats.totalRequests++;
    
    if (entry.statusCode >= 200 && entry.statusCode < 300) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }
    
    if (entry.rateLimited) {
        m_stats.rateLimitedRequests++;
    }
    
    if (entry.authFailed) {
        m_stats.authFailedRequests++;
    }
    
    // 更新平均响应时间
    double totalTime = m_stats.averageResponseTime * (m_stats.totalRequests - 1) + entry.responseTime.count();
    m_stats.averageResponseTime = totalTime / m_stats.totalRequests;
    
    // 更新数据传输量
    m_stats.totalDataTransferred += entry.requestSize + entry.responseSize;
}

void AccessLogger::cleanupOldLogFiles() {
    try {
        std::vector<std::filesystem::path> logFiles;
        
        // 收集所有日志文件
        for (const auto& entry : std::filesystem::directory_iterator(m_config.logDirectory)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find(m_config.logFilePrefix) == 0 &&
                    filename.length() >= 4 &&
                    filename.substr(filename.length() - 4) == ".log") {
                    logFiles.push_back(entry.path());
                }
            }
        }
        
        // 按修改时间排序
        std::sort(logFiles.begin(), logFiles.end(), [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        });
        
        // 删除超过限制的文件
        if (logFiles.size() > static_cast<size_t>(m_config.maxFiles)) {
            for (size_t i = m_config.maxFiles; i < logFiles.size(); ++i) {
                std::filesystem::remove(logFiles[i]);
                LOG_INFO() << "[AccessLogger] Removed old log file: " << logFiles[i];
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "[AccessLogger] Failed to cleanup old log files: " << e.what();
    }
}
