#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <thread>
#include <map>

namespace AISecurityVision {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
};

/**
 * @brief 日志输出目标枚举
 */
enum class LogTarget {
    CONSOLE = 1,
    FILE = 2,
    BOTH = 3
};

/**
 * @brief 高性能日志类，支持多线程安全、文件输出、格式化等功能
 *
 * 特性：
 * - 线程安全
 * - 支持控制台和文件输出
 * - 自动显示文件名、行号、函数名
 * - 支持日志级别过滤
 * - 支持彩色输出
 * - 支持流式操作符
 * - 支持日志文件轮转
 */
class Logger {
public:
    /**
     * @brief 获取Logger单例实例
     */
    static Logger& getInstance();

    /**
     * @brief 设置日志级别
     * @param level 最低输出级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 设置输出目标
     * @param target 输出目标（控制台/文件/两者）
     */
    void setLogTarget(LogTarget target);

    /**
     * @brief 设置日志文件路径
     * @param filePath 日志文件路径
     */
    void setLogFile(const std::string& filePath);

    /**
     * @brief 启用/禁用彩色输出
     * @param enable 是否启用彩色输出
     */
    void setColorOutput(bool enable);

    /**
     * @brief 启用/禁用时间戳
     * @param enable 是否显示时间戳
     */
    void setTimestamp(bool enable);

    /**
     * @brief 启用/禁用线程ID显示
     * @param enable 是否显示线程ID
     */
    void setThreadId(bool enable);

    /**
     * @brief 设置日志文件最大大小（字节）
     * @param maxSize 最大文件大小，超过后会轮转
     */
    void setMaxFileSize(size_t maxSize);

    /**
     * @brief 设置保留的日志文件数量
     * @param count 保留文件数量
     */
    void setMaxFileCount(int count);

    /**
     * @brief 刷新日志缓冲区
     */
    void flush();

    /**
     * @brief 日志记录函数
     * @param level 日志级别
     * @param file 文件名
     * @param line 行号
     * @param func 函数名
     * @param message 日志消息
     */
    void log(LogLevel level, const char* file, int line, const char* func, const std::string& message);

    /**
     * @brief 日志流类，支持流式操作
     */
    class LogStream {
    public:
        LogStream(Logger& logger, LogLevel level, const char* file, int line, const char* func);
        ~LogStream();

        template<typename T>
        LogStream& operator<<(const T& value) {
            m_stream << value;
            return *this;
        }

        // 特殊处理 std::endl
        LogStream& operator<<(std::ostream& (*manip)(std::ostream&));

    private:
        Logger& m_logger;
        LogLevel m_level;
        const char* m_file;
        int m_line;
        const char* m_func;
        std::ostringstream m_stream;
    };

    /**
     * @brief 创建日志流
     */
    LogStream stream(LogLevel level, const char* file, int line, const char* func);

private:
    Logger();
    ~Logger();

    // 禁用拷贝构造和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 格式化日志消息
     */
    std::string formatMessage(LogLevel level, const char* file, int line, const char* func, const std::string& message);

    /**
     * @brief 获取日志级别字符串
     */
    std::string getLevelString(LogLevel level);

    /**
     * @brief 获取日志级别颜色代码
     */
    std::string getLevelColor(LogLevel level);

    /**
     * @brief 获取当前时间戳字符串
     */
    std::string getCurrentTimestamp();

    /**
     * @brief 提取文件名（去除路径）
     */
    std::string extractFileName(const char* filePath);

    /**
     * @brief 写入到控制台
     */
    void writeToConsole(const std::string& message, LogLevel level);

    /**
     * @brief 写入到文件
     */
    void writeToFile(const std::string& message);

    /**
     * @brief 检查并执行日志文件轮转
     */
    void rotateLogFile();

    /**
     * @brief 获取文件大小
     */
    size_t getFileSize(const std::string& filePath);

private:
    mutable std::mutex m_mutex;
    LogLevel m_logLevel;
    LogTarget m_logTarget;
    std::string m_logFilePath;
    std::unique_ptr<std::ofstream> m_logFile;
    bool m_colorOutput;
    bool m_showTimestamp;
    bool m_showThreadId;
    size_t m_maxFileSize;
    int m_maxFileCount;

    // 颜色代码
    static const std::map<LogLevel, std::string> s_colorCodes;
    static const std::string s_resetColor;
};

} // namespace AISecurityVision

// 便利宏定义
#define LOG_TRACE() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::TRACE, __FILE__, __LINE__, __FUNCTION__)
#define LOG_DEBUG() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__)
#define LOG_INFO() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::INFO, __FILE__, __LINE__, __FUNCTION__)
#define LOG_WARN() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::WARN, __FILE__, __LINE__, __FUNCTION__)
#define LOG_ERROR() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::ERROR, __FILE__, __LINE__, __FUNCTION__)
#define LOG_FATAL() AISecurityVision::Logger::getInstance().stream(AISecurityVision::LogLevel::FATAL, __FILE__, __LINE__, __FUNCTION__)

// 兼容性宏，用于替换std::cout和std::cerr
#define LOGGER_OUT LOG_INFO()
#define LOGGER_ERR LOG_ERROR()

// 条件日志宏
#define LOG_IF(condition, level) \
    if (condition) AISecurityVision::Logger::getInstance().stream(level, __FILE__, __LINE__, __FUNCTION__)
