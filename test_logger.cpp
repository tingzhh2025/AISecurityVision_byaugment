#include "src/core/Logger.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace AISecurityVision;

void testBasicFunctionality() {
    std::cout << "\n=== 测试基础功能 ===" << std::endl;

    // 测试所有日志级别
    LOG_TRACE() << "这是TRACE级别日志";
    LOG_DEBUG() << "这是DEBUG级别日志";
    LOG_INFO() << "这是INFO级别日志";
    LOG_WARN() << "这是WARN级别日志";
    LOG_ERROR() << "这是ERROR级别日志";
    LOG_FATAL() << "这是FATAL级别日志";

    // 测试兼容性宏
    LOGGER_OUT << "使用LOGGER_OUT替代std::cout";
    LOGGER_ERR << "使用LOGGER_ERR替代std::cerr";
}

void testDataTypes() {
    std::cout << "\n=== 测试数据类型支持 ===" << std::endl;

    int intVal = 42;
    double doubleVal = 3.14159;
    std::string strVal = "Hello Logger";
    bool boolVal = true;

    LOG_INFO() << "整数: " << intVal
               << ", 浮点数: " << doubleVal
               << ", 字符串: " << strVal
               << ", 布尔值: " << boolVal;

    // 测试格式化
    LOG_INFO() << "格式化浮点数: " << std::fixed << std::setprecision(2) << doubleVal;
    LOG_INFO() << "十六进制: 0x" << std::hex << intVal;
}

void testConfiguration() {
    std::cout << "\n=== 测试配置功能 ===" << std::endl;

    Logger& logger = Logger::getInstance();

    // 测试日志级别设置
    logger.setLogLevel(LogLevel::WARN);
    LOG_DEBUG() << "这条DEBUG日志不应该显示";
    LOG_WARN() << "这条WARN日志应该显示";

    // 恢复日志级别
    logger.setLogLevel(LogLevel::TRACE);

    // 测试文件输出
    logger.setLogFile("test_output.log");
    logger.setLogTarget(LogTarget::BOTH);
    LOG_INFO() << "这条日志会同时输出到控制台和文件";

    // 测试时间戳和线程ID
    logger.setTimestamp(true);
    logger.setThreadId(true);
    LOG_INFO() << "显示时间戳和线程ID的日志";
}

void testMultiThreading() {
    std::cout << "\n=== 测试多线程安全 ===" << std::endl;

    Logger& logger = Logger::getInstance();
    logger.setThreadId(true);

    auto worker = [](int threadId) {
        for (int i = 0; i < 3; ++i) {
            LOG_INFO() << "线程 " << threadId << " 输出第 " << i << " 条日志";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    std::thread t1(worker, 1);
    std::thread t2(worker, 2);

    t1.join();
    t2.join();
}

void testAdvancedFeatures() {
    std::cout << "\n=== 测试高级功能 ===" << std::endl;

    // 测试条件日志
    bool condition = true;
    LOG_IF(condition, LogLevel::INFO) << "条件为真时的日志";

    condition = false;
    LOG_IF(condition, LogLevel::INFO) << "这条日志不会显示";

    LOG_INFO() << "高级功能测试完成";
}

void simulateRealUsage() {
    std::cout << "\n=== 模拟实际使用场景 ===" << std::endl;

    // 模拟系统初始化
    LOG_INFO() << "系统初始化开始";

    try {
        // 模拟一些操作
        LOG_DEBUG() << "加载配置文件";
        LOG_INFO() << "配置文件加载成功";

        LOG_DEBUG() << "初始化数据库连接";
        LOG_INFO() << "数据库连接建立成功";

        LOG_DEBUG() << "启动服务";
        LOG_INFO() << "服务启动成功，监听端口: 8080";

        // 模拟一个警告
        LOG_WARN() << "检测到配置项缺失，使用默认值";

        // 模拟错误处理
        throw std::runtime_error("模拟的网络连接错误");

    } catch (const std::exception& e) {
        LOG_ERROR() << "捕获到异常: " << e.what();
        LOG_INFO() << "系统将尝试恢复";
    }

    LOG_INFO() << "系统初始化完成";
}

int main() {
    std::cout << "Logger类功能测试" << std::endl;
    std::cout << "=================" << std::endl;

    // 配置Logger
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::TRACE);
    logger.setColorOutput(true);
    logger.setTimestamp(true);

    // 运行各种测试
    testBasicFunctionality();
    testDataTypes();
    testConfiguration();
    testMultiThreading();
    testAdvancedFeatures();
    simulateRealUsage();

    // 刷新日志
    logger.flush();

    std::cout << "\n=== 测试完成 ===" << std::endl;
    std::cout << "请检查生成的日志文件: test_output.log" << std::endl;

    return 0;
}

/*
编译命令:
g++ -std=c++17 -pthread test_logger.cpp src/core/Logger.cpp -lstdc++fs -o test_logger

运行:
./test_logger

预期输出:
- 控制台会显示彩色的格式化日志
- 会生成test_output.log文件
- 多线程日志会正确交错显示
- 高级功能（条件日志、频率限制等）会按预期工作
*/
