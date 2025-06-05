#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <mutex>
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief API访问日志记录器
 * 
 * 提供高性能的API访问日志记录功能：
 * - 异步日志写入
 * - 结构化日志格式
 * - 自动日志轮转
 * - 安全事件检测
 * - 统计信息收集
 */
class AccessLogger {
public:
    /**
     * @brief 访问日志条目
     */
    struct AccessLogEntry {
        std::chrono::system_clock::time_point timestamp;    // 时间戳
        std::string clientIp;                               // 客户端IP
        std::string userId;                                 // 用户ID
        std::string method;                                 // HTTP方法
        std::string endpoint;                               // API端点
        std::string userAgent;                              // User-Agent
        std::string referer;                                // Referer
        int statusCode;                                     // 响应状态码
        size_t requestSize;                                 // 请求大小
        size_t responseSize;                                // 响应大小
        std::chrono::milliseconds responseTime;             // 响应时间
        std::string errorMessage;                           // 错误消息
        bool rateLimited;                                   // 是否被限流
        bool authFailed;                                    // 认证是否失败
        
        AccessLogEntry() : statusCode(0), requestSize(0), responseSize(0), 
                          responseTime(0), rateLimited(false), authFailed(false) {}
    };

    /**
     * @brief 日志配置
     */
    struct LogConfig {
        std::string logDirectory;          // 日志目录
        std::string logFilePrefix;         // 日志文件前缀
        size_t maxFileSize;                // 最大文件大小(字节)
        int maxFiles;                      // 最大文件数量
        bool enableConsoleOutput;          // 是否输出到控制台
        bool enableJsonFormat;             // 是否使用JSON格式
        int flushIntervalSeconds;          // 刷新间隔(秒)
        
        LogConfig() : logDirectory("logs"), logFilePrefix("access"), 
                     maxFileSize(100 * 1024 * 1024), maxFiles(10),
                     enableConsoleOutput(false), enableJsonFormat(true),
                     flushIntervalSeconds(5) {}
    };

    /**
     * @brief 访问统计信息
     */
    struct AccessStats {
        uint64_t totalRequests;             // 总请求数
        uint64_t successfulRequests;        // 成功请求数
        uint64_t failedRequests;            // 失败请求数
        uint64_t rateLimitedRequests;       // 被限流的请求数
        uint64_t authFailedRequests;        // 认证失败的请求数
        double averageResponseTime;         // 平均响应时间(ms)
        uint64_t totalDataTransferred;      // 总传输数据量(字节)
        std::chrono::system_clock::time_point startTime; // 统计开始时间
        
        AccessStats() : totalRequests(0), successfulRequests(0), failedRequests(0),
                       rateLimitedRequests(0), authFailedRequests(0), 
                       averageResponseTime(0.0), totalDataTransferred(0),
                       startTime(std::chrono::system_clock::now()) {}
    };

public:
    /**
     * @brief 构造函数
     * @param config 日志配置
     */
    explicit AccessLogger(const LogConfig& config = LogConfig());

    /**
     * @brief 析构函数
     */
    ~AccessLogger();

    /**
     * @brief 记录访问日志
     * @param entry 访问日志条目
     */
    void logAccess(const AccessLogEntry& entry);

    /**
     * @brief 记录API访问（简化版本）
     * @param clientIp 客户端IP
     * @param method HTTP方法
     * @param endpoint API端点
     * @param statusCode 响应状态码
     * @param responseTime 响应时间(ms)
     * @param userId 用户ID（可选）
     */
    void logApiAccess(const std::string& clientIp, const std::string& method, 
                     const std::string& endpoint, int statusCode, 
                     std::chrono::milliseconds responseTime,
                     const std::string& userId = "");

    /**
     * @brief 记录安全事件
     * @param clientIp 客户端IP
     * @param eventType 事件类型
     * @param description 事件描述
     * @param severity 严重程度(1-5)
     */
    void logSecurityEvent(const std::string& clientIp, const std::string& eventType,
                         const std::string& description, int severity = 3);

    /**
     * @brief 获取访问统计信息
     * @return 统计信息
     */
    AccessStats getStats() const;

    /**
     * @brief 获取统计信息JSON字符串
     * @return JSON格式的统计信息
     */
    std::string getStatsJson() const;

    /**
     * @brief 获取最近的访问日志
     * @param count 获取数量
     * @return 最近的访问日志条目
     */
    std::vector<AccessLogEntry> getRecentLogs(size_t count = 100) const;

    /**
     * @brief 重置统计信息
     */
    void resetStats();

    /**
     * @brief 强制刷新日志到文件
     */
    void flush();

    /**
     * @brief 设置日志配置
     * @param config 新的配置
     */
    void setConfig(const LogConfig& config);

    /**
     * @brief 启动日志记录器
     */
    void start();

    /**
     * @brief 停止日志记录器
     */
    void stop();

    /**
     * @brief 检查是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;

private:
    /**
     * @brief 日志写入线程函数
     */
    void logWriterThread();

    /**
     * @brief 格式化日志条目
     * @param entry 日志条目
     * @return 格式化后的日志字符串
     */
    std::string formatLogEntry(const AccessLogEntry& entry) const;

    /**
     * @brief 格式化为JSON格式
     * @param entry 日志条目
     * @return JSON格式的日志字符串
     */
    std::string formatAsJson(const AccessLogEntry& entry) const;

    /**
     * @brief 格式化为文本格式
     * @param entry 日志条目
     * @return 文本格式的日志字符串
     */
    std::string formatAsText(const AccessLogEntry& entry) const;

    /**
     * @brief 检查是否需要轮转日志文件
     * @return 是否需要轮转
     */
    bool needsRotation() const;

    /**
     * @brief 轮转日志文件
     */
    void rotateLogFile();

    /**
     * @brief 获取当前日志文件路径
     * @return 日志文件路径
     */
    std::string getCurrentLogFilePath() const;

    /**
     * @brief 获取时间戳字符串
     * @param timePoint 时间点
     * @return 时间戳字符串
     */
    std::string getTimestampString(const std::chrono::system_clock::time_point& timePoint) const;

    /**
     * @brief 更新统计信息
     * @param entry 访问日志条目
     */
    void updateStats(const AccessLogEntry& entry);

    /**
     * @brief 清理旧的日志文件
     */
    void cleanupOldLogFiles();

private:
    LogConfig m_config;                                     // 日志配置
    mutable std::mutex m_mutex;                             // 线程安全锁
    std::queue<AccessLogEntry> m_logQueue;                  // 日志队列
    std::condition_variable m_condition;                    // 条件变量
    std::unique_ptr<std::thread> m_writerThread;           // 写入线程
    std::atomic<bool> m_running{false};                     // 运行状态
    std::atomic<bool> m_stopRequested{false};              // 停止请求
    
    std::unique_ptr<std::ofstream> m_logFile;              // 日志文件流
    std::string m_currentLogFilePath;                       // 当前日志文件路径
    size_t m_currentFileSize;                               // 当前文件大小
    
    mutable std::mutex m_statsMutex;                        // 统计信息锁
    AccessStats m_stats;                                    // 访问统计
    std::queue<AccessLogEntry> m_recentLogs;               // 最近的日志条目
    static const size_t MAX_RECENT_LOGS = 1000;            // 最大保留的最近日志数量
    
    std::chrono::steady_clock::time_point m_lastFlush;     // 最后刷新时间
};

} // namespace AISecurityVision
