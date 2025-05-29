#include "Logger.h"
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace AISecurityVision {

// 静态成员初始化
const std::map<LogLevel, std::string> Logger::s_colorCodes = {
    {LogLevel::TRACE, "\033[37m"},   // 白色
    {LogLevel::DEBUG, "\033[36m"},   // 青色
    {LogLevel::INFO, "\033[32m"},    // 绿色
    {LogLevel::WARN, "\033[33m"},    // 黄色
    {LogLevel::ERROR, "\033[31m"},   // 红色
    {LogLevel::FATAL, "\033[35m"}    // 紫色
};

const std::string Logger::s_resetColor = "\033[0m";

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_logLevel(LogLevel::INFO)
    , m_logTarget(LogTarget::CONSOLE)
    , m_colorOutput(true)
    , m_showTimestamp(true)
    , m_showThreadId(false)
    , m_maxFileSize(10 * 1024 * 1024)  // 10MB
    , m_maxFileCount(5) {
}

Logger::~Logger() {
    flush();
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logLevel = level;
}

void Logger::setLogTarget(LogTarget target) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logTarget = target;
}

void Logger::setLogFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logFilePath = filePath;

    if (!filePath.empty()) {
        // 创建目录（如果不存在）
        std::filesystem::path path(filePath);
        auto parentPath = path.parent_path();
        if (!parentPath.empty()) {
            try {
                std::filesystem::create_directories(parentPath);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "[Logger] Failed to create directories: " << e.what() << std::endl;
            }
        }

        // 重新打开文件
        m_logFile = std::make_unique<std::ofstream>(filePath, std::ios::app);
        if (!m_logFile->is_open()) {
            std::cerr << "[Logger] Failed to open log file: " << filePath << std::endl;
        }
    }
}

void Logger::setColorOutput(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_colorOutput = enable;
}

void Logger::setTimestamp(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_showTimestamp = enable;
}

void Logger::setThreadId(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_showThreadId = enable;
}

void Logger::setMaxFileSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxFileSize = maxSize;
}

void Logger::setMaxFileCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxFileCount = count;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile && m_logFile->is_open()) {
        m_logFile->flush();
    }
    std::cout.flush();
    std::cerr.flush();
}

void Logger::log(LogLevel level, const char* file, int line, const char* func, const std::string& message) {
    // 检查日志级别
    if (level < m_logLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string formattedMessage = formatMessage(level, file, line, func, message);

    // 根据目标输出
    if (m_logTarget == LogTarget::CONSOLE || m_logTarget == LogTarget::BOTH) {
        writeToConsole(formattedMessage, level);
    }

    if (m_logTarget == LogTarget::FILE || m_logTarget == LogTarget::BOTH) {
        writeToFile(formattedMessage);
    }
}

Logger::LogStream Logger::stream(LogLevel level, const char* file, int line, const char* func) {
    return LogStream(*this, level, file, line, func);
}

std::string Logger::formatMessage(LogLevel level, const char* file, int line, const char* func, const std::string& message) {
    std::ostringstream oss;

    // 时间戳
    if (m_showTimestamp) {
        oss << "[" << getCurrentTimestamp() << "] ";
    }

    // 线程ID
    if (m_showThreadId) {
        oss << "[T:" << std::this_thread::get_id() << "] ";
    }

    // 日志级别
    oss << "[" << getLevelString(level) << "] ";

    // 文件名:行号:函数名
    oss << "[" << extractFileName(file) << ":" << line << ":" << func << "] ";

    // 消息内容
    oss << message;

    return oss.str();
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::getLevelColor(LogLevel level) {
    auto it = s_colorCodes.find(level);
    return (it != s_colorCodes.end()) ? it->second : "";
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::extractFileName(const char* filePath) {
    std::string path(filePath);
    size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

void Logger::writeToConsole(const std::string& message, LogLevel level) {
    std::ostream& stream = (level >= LogLevel::ERROR) ? std::cerr : std::cout;

    if (m_colorOutput) {
        stream << getLevelColor(level) << message << s_resetColor << std::endl;
    } else {
        stream << message << std::endl;
    }
}

void Logger::writeToFile(const std::string& message) {
    if (!m_logFile || !m_logFile->is_open()) {
        return;
    }

    // 检查文件大小并轮转
    rotateLogFile();

    *m_logFile << message << std::endl;
    m_logFile->flush();
}

void Logger::rotateLogFile() {
    if (m_logFilePath.empty() || !m_logFile || !m_logFile->is_open()) {
        return;
    }

    size_t currentSize = getFileSize(m_logFilePath);
    if (currentSize < m_maxFileSize) {
        return;
    }

    // 关闭当前文件
    m_logFile->close();

    // 轮转文件
    for (int i = m_maxFileCount - 1; i > 0; --i) {
        std::string oldFile = m_logFilePath + "." + std::to_string(i);
        std::string newFile = m_logFilePath + "." + std::to_string(i + 1);

        if (std::filesystem::exists(oldFile)) {
            if (i == m_maxFileCount - 1) {
                std::filesystem::remove(newFile);  // 删除最老的文件
            }
            std::filesystem::rename(oldFile, newFile);
        }
    }

    // 重命名当前文件
    std::string backupFile = m_logFilePath + ".1";
    std::filesystem::rename(m_logFilePath, backupFile);

    // 重新打开新文件
    m_logFile = std::make_unique<std::ofstream>(m_logFilePath, std::ios::app);
}

size_t Logger::getFileSize(const std::string& filePath) {
    try {
        return std::filesystem::file_size(filePath);
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

// LogStream 实现
Logger::LogStream::LogStream(Logger& logger, LogLevel level, const char* file, int line, const char* func)
    : m_logger(logger), m_level(level), m_file(file), m_line(line), m_func(func) {
}

Logger::LogStream::~LogStream() {
    m_logger.log(m_level, m_file, m_line, m_func, m_stream.str());
}

Logger::LogStream& Logger::LogStream::operator<<(std::ostream& (*manip)(std::ostream&)) {
    m_stream << manip;
    return *this;
}

} // namespace AISecurityVision
