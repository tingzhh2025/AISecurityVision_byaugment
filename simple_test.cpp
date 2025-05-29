#include "src/core/Logger.h"
#include <iostream>

using namespace AISecurityVision;

int main() {
    std::cout << "=== Logger简单测试 ===" << std::endl;
    
    // 配置Logger
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::TRACE);
    logger.setColorOutput(true);
    logger.setTimestamp(true);
    
    // 测试基础日志功能
    LOG_TRACE() << "这是TRACE级别日志";
    LOG_DEBUG() << "这是DEBUG级别日志";
    LOG_INFO() << "这是INFO级别日志";
    LOG_WARN() << "这是WARN级别日志";
    LOG_ERROR() << "这是ERROR级别日志";
    LOG_FATAL() << "这是FATAL级别日志";
    
    // 测试兼容性宏
    LOGGER_OUT << "使用LOGGER_OUT替代std::cout";
    LOGGER_ERR << "使用LOGGER_ERR替代std::cerr";
    
    // 测试数据类型
    int number = 42;
    double pi = 3.14159;
    std::string text = "Hello Logger";
    
    LOG_INFO() << "数字: " << number << ", 浮点数: " << pi << ", 文本: " << text;
    
    // 测试条件日志
    bool condition = true;
    LOG_IF(condition, LogLevel::INFO) << "条件为真时显示的日志";
    
    condition = false;
    LOG_IF(condition, LogLevel::INFO) << "这条日志不会显示";
    
    // 测试文件输出
    logger.setLogFile("simple_test.log");
    logger.setLogTarget(LogTarget::BOTH);
    LOG_INFO() << "这条日志会同时输出到控制台和文件";
    
    // 刷新日志
    logger.flush();
    
    std::cout << "=== 测试完成 ===" << std::endl;
    std::cout << "请检查生成的日志文件: simple_test.log" << std::endl;
    
    return 0;
}
