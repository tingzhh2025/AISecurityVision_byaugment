#include "../core/Logger.h"
#include <thread>
#include <chrono>

using namespace AISecurityVision;

/**
 * @brief Logger使用示例
 * 
 * 演示如何使用Logger类替代LOGGER_OUT和LOGGER_ERR
 */

void demonstrateBasicLogging() {
    LOG_INFO() << "\n=== 基础日志功能演示 ===";
    
    // 基础日志级别使用
    LOG_TRACE() << "这是一条TRACE级别的日志";
    LOG_DEBUG() << "这是一条DEBUG级别的日志";
    LOG_INFO() << "这是一条INFO级别的日志";
    LOG_WARN() << "这是一条WARN级别的日志";
    LOG_ERROR() << "这是一条ERROR级别的日志";
    LOG_FATAL() << "这是一条FATAL级别的日志";
    
    // 替代LOGGER_OUT和LOGGER_ERR的用法
    LOGGER_OUT << "替代LOGGER_OUT的输出";
    LOGGER_ERR << "替代LOGGER_ERR的错误输出";
}

void demonstrateAdvancedFeatures() {
    LOG_INFO() << "\n=== 高级功能演示 ===";
    
    // 条件日志
    bool condition = true;
    LOG_IF(condition, LogLevel::INFO) << "条件为真时才输出的日志";
    
    // 一次性日志（同一位置只记录一次）
    for (int i = 0; i < 5; ++i) {
        LOG_ONCE(LogLevel::WARN) << "这条日志只会输出一次，即使在循环中";
    }
    
    // 频率限制日志（每2秒最多记录一次）
    for (int i = 0; i < 10; ++i) {
        LOG_EVERY_N_SEC(LogLevel::INFO, 2) << "频率限制日志，每2秒最多一次: " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void demonstrateConfiguration() {
    LOG_INFO() << "\n=== 配置功能演示 ===";
    
    Logger& logger = Logger::getInstance();
    
    // 设置日志级别
    logger.setLogLevel(LogLevel::DEBUG);
    LOG_DEBUG() << "设置日志级别为DEBUG后，这条DEBUG日志会显示";
    
    // 设置文件输出
    logger.setLogFile("logs/application.log");
    logger.setLogTarget(LogTarget::BOTH);  // 同时输出到控制台和文件
    LOG_INFO() << "这条日志会同时输出到控制台和文件";
    
    // 配置显示选项
    logger.setTimestamp(true);
    logger.setThreadId(true);
    logger.setColorOutput(true);
    LOG_INFO() << "配置了时间戳、线程ID和彩色输出";
    
    // 设置文件轮转
    logger.setMaxFileSize(1024 * 1024);  // 1MB
    logger.setMaxFileCount(3);
    LOG_INFO() << "配置了文件轮转：最大1MB，保留3个文件";
}

void demonstrateMultiThreading() {
    LOG_INFO() << "\n=== 多线程安全演示 ===";
    
    Logger& logger = Logger::getInstance();
    logger.setThreadId(true);
    
    auto worker = [](int threadId) {
        for (int i = 0; i < 5; ++i) {
            LOG_INFO() << "线程 " << threadId << " 的第 " << i << " 条日志";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    };
    
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    std::thread t3(worker, 3);
    
    t1.join();
    t2.join();
    t3.join();
}

void demonstrateErrorHandling() {
    LOG_INFO() << "\n=== 错误处理演示 ===";
    
    try {
        // 模拟一些可能出错的操作
        throw std::runtime_error("模拟的运行时错误");
    } catch (const std::exception& e) {
        LOG_ERROR() << "捕获到异常: " << e.what();
    }
    
    // 模拟不同类型的错误
    LOG_WARN() << "警告：配置文件未找到，使用默认配置";
    LOG_ERROR() << "错误：无法连接到数据库";
    LOG_FATAL() << "致命错误：系统内存不足";
}

void demonstrateDataTypes() {
    LOG_INFO() << "\n=== 数据类型支持演示 ===";
    
    // 支持各种数据类型
    int intValue = 42;
    double doubleValue = 3.14159;
    std::string stringValue = "Hello Logger";
    bool boolValue = true;
    
    LOG_INFO() << "整数: " << intValue 
               << ", 浮点数: " << doubleValue 
               << ", 字符串: " << stringValue 
               << ", 布尔值: " << boolValue;
    
    // 支持格式化
    LOG_INFO() << "格式化输出: " 
               << std::fixed << std::setprecision(2) << doubleValue;
    
    // 支持十六进制
    LOG_DEBUG() << "十六进制: 0x" << std::hex << intValue;
}

// 替换原有LOGGER_OUT和LOGGER_ERR的示例函数
void oldStyleLogging() {
    // 原来的代码风格
    // LOG_INFO() << "[Info] 这是信息日志";
    // LOG_ERROR() << "[Error] 这是错误日志";
    
    // 新的Logger风格
    LOG_INFO() << "这是信息日志";
    LOG_ERROR() << "这是错误日志";
}

int main() {
    LOG_INFO() << "Logger类使用示例";
    LOG_INFO() << "=================";
    
    // 初始化Logger配置
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::TRACE);
    logger.setColorOutput(true);
    logger.setTimestamp(true);
    
    // 演示各种功能
    demonstrateBasicLogging();
    demonstrateAdvancedFeatures();
    demonstrateConfiguration();
    demonstrateMultiThreading();
    demonstrateErrorHandling();
    demonstrateDataTypes();
    oldStyleLogging();
    
    // 刷新日志缓冲区
    logger.flush();
    
    LOG_INFO() << "\n=== 演示完成 ===";
    LOG_INFO() << "请查看生成的日志文件: logs/application.log";
    
    return 0;
}

/*
使用说明：

1. 基础用法：
   LOG_INFO() << "你的日志消息";
   LOG_ERROR() << "错误消息";

2. 替换LOGGER_OUT和LOGGER_ERR：
   LOGGER_OUT << "替代LOGGER_OUT";
   LOGGER_ERR << "替代LOGGER_ERR";

3. 配置Logger：
   Logger& logger = Logger::getInstance();
   logger.setLogLevel(LogLevel::DEBUG);
   logger.setLogFile("app.log");
   logger.setLogTarget(LogTarget::BOTH);

4. 高级功能：
   LOG_IF(condition, LogLevel::WARN) << "条件日志";
   LOG_ONCE(LogLevel::INFO) << "一次性日志";
   LOG_EVERY_N_SEC(LogLevel::DEBUG, 5) << "频率限制日志";

5. 编译时需要链接filesystem库：
   g++ -std=c++17 LoggerExample.cpp Logger.cpp -lstdc++fs -pthread -o logger_example
*/
