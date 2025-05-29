/**
 * @file logger_replacement_example.cpp
 * @brief 展示如何使用Logger类替换现有代码中的std::cout和std::cerr
 * 
 * 这个文件展示了实际项目中如何进行Logger替换的示例
 */

#include "src/core/Logger.h"
#include <iostream>
#include <stdexcept>

using namespace AISecurityVision;

// ========================================
// 原始代码示例（使用std::cout和std::cerr）
// ========================================

void oldStyleTaskManager() {
    std::cout << "[TaskManager] Initializing TaskManager singleton" << std::endl;
    
    try {
        // 模拟一些操作
        std::cout << "[TaskManager] Loading configuration..." << std::endl;
        std::cout << "[TaskManager] Starting monitoring thread..." << std::endl;
        std::cout << "[TaskManager] TaskManager started successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[TaskManager] Error during initialization: " << e.what() << std::endl;
    }
}

void oldStyleYOLODetector() {
    std::cout << "[YOLOv8Detector] Initializing YOLOv8 detector..." << std::endl;
    std::cout << "[YOLOv8Detector] Model path: models/yolov8n.rknn" << std::endl;
    
    bool success = true; // 模拟初始化结果
    if (success) {
        std::cout << "[YOLOv8Detector] YOLOv8 detector initialized successfully with RKNN backend" << std::endl;
        std::cout << "[YOLOv8Detector] Input size: 640x640" << std::endl;
        std::cout << "[YOLOv8Detector] Classes: 80" << std::endl;
    } else {
        std::cerr << "[YOLOv8Detector] Failed to initialize with any backend" << std::endl;
    }
}

// ========================================
// 新的Logger风格代码
// ========================================

void newStyleTaskManager() {
    LOG_INFO() << "Initializing TaskManager singleton";
    
    try {
        // 模拟一些操作
        LOG_DEBUG() << "Loading configuration...";
        LOG_DEBUG() << "Starting monitoring thread...";
        LOG_INFO() << "TaskManager started successfully";
    } catch (const std::exception& e) {
        LOG_ERROR() << "Error during initialization: " << e.what();
    }
}

void newStyleYOLODetector() {
    LOG_INFO() << "Initializing YOLOv8 detector...";
    LOG_DEBUG() << "Model path: models/yolov8n.rknn";
    
    bool success = true; // 模拟初始化结果
    if (success) {
        LOG_INFO() << "YOLOv8 detector initialized successfully with RKNN backend";
        LOG_DEBUG() << "Input size: 640x640";
        LOG_DEBUG() << "Classes: 80";
    } else {
        LOG_ERROR() << "Failed to initialize with any backend";
    }
}

// ========================================
// 兼容性替换示例
// ========================================

void compatibilityExample() {
    // 使用兼容性宏，最小化代码修改
    LOGGER_OUT << "这是替代std::cout的输出";
    LOGGER_ERR << "这是替代std::cerr的错误输出";
    
    // 或者直接替换
    // 原代码: std::cout << "message" << std::endl;
    // 新代码: LOG_INFO() << "message";
}

// ========================================
// 高级功能示例
// ========================================

void advancedFeaturesExample() {
    // 条件日志
    bool debugMode = true;
    LOG_IF(debugMode, LogLevel::DEBUG) << "调试模式已启用";
    
    // 不同级别的日志
    LOG_TRACE() << "详细的跟踪信息";
    LOG_DEBUG() << "调试信息";
    LOG_INFO() << "一般信息";
    LOG_WARN() << "警告信息";
    LOG_ERROR() << "错误信息";
    LOG_FATAL() << "致命错误";
    
    // 格式化输出
    int frameCount = 1234;
    double fps = 29.97;
    LOG_INFO() << "处理了 " << frameCount << " 帧，平均FPS: " 
               << std::fixed << std::setprecision(2) << fps;
}

// ========================================
// 实际使用场景模拟
// ========================================

class MockVideoPipeline {
public:
    void initialize(const std::string& sourceId) {
        LOG_INFO() << "Creating pipeline for: " << sourceId;
        
        try {
            LOG_DEBUG() << "Initializing pipeline: " << sourceId;
            
            // 模拟初始化过程
            if (initializeDetector()) {
                LOG_INFO() << "Optimized RKNN YOLOv8 detector initialized successfully!";
            } else {
                LOG_WARN() << "Failed to initialize optimized detector, falling back to standard detector";
            }
            
            if (initializeStreamer()) {
                LOG_INFO() << "MJPEG stream available at: http://localhost:8161/stream";
            }
            
            LOG_INFO() << "Pipeline initialized successfully: " << sourceId;
            
        } catch (const std::exception& e) {
            LOG_ERROR() << "Pipeline initialization failed: " << e.what();
            throw;
        }
    }
    
    void processFrame(int frameId) {
        // 高频日志使用TRACE级别
        LOG_TRACE() << "Processing frame " << frameId;
        
        // 模拟检测结果
        int detectionCount = frameId % 5; // 模拟检测数量
        if (detectionCount > 0) {
            LOG_DEBUG() << "Processed " << detectionCount << " detections in frame " << frameId;
        }
        
        // 模拟错误情况
        if (frameId % 100 == 0) {
            LOG_WARN() << "Frame processing took longer than expected: " << frameId;
        }
    }
    
private:
    bool initializeDetector() {
        LOG_DEBUG() << "Initializing detector...";
        return true; // 模拟成功
    }
    
    bool initializeStreamer() {
        LOG_DEBUG() << "Initializing streamer...";
        return true; // 模拟成功
    }
};

// ========================================
// 配置示例
// ========================================

void setupLogger() {
    Logger& logger = Logger::getInstance();
    
    // 开发环境配置
    logger.setLogLevel(LogLevel::DEBUG);
    logger.setColorOutput(true);
    logger.setTimestamp(true);
    logger.setThreadId(false);
    
    // 设置文件输出
    logger.setLogFile("logs/application.log");
    logger.setLogTarget(LogTarget::BOTH); // 同时输出到控制台和文件
    
    // 设置文件轮转
    logger.setMaxFileSize(10 * 1024 * 1024); // 10MB
    logger.setMaxFileCount(5);
    
    LOG_INFO() << "Logger configured successfully";
}

// ========================================
// 主函数演示
// ========================================

int main() {
    std::cout << "=== Logger替换示例演示 ===" << std::endl;
    
    // 配置Logger
    setupLogger();
    
    std::cout << "\n1. 对比原始代码和新代码:" << std::endl;
    std::cout << "--- 原始风格 ---" << std::endl;
    oldStyleTaskManager();
    oldStyleYOLODetector();
    
    std::cout << "\n--- 新Logger风格 ---" << std::endl;
    newStyleTaskManager();
    newStyleYOLODetector();
    
    std::cout << "\n2. 兼容性示例:" << std::endl;
    compatibilityExample();
    
    std::cout << "\n3. 高级功能示例:" << std::endl;
    advancedFeaturesExample();
    
    std::cout << "\n4. 实际使用场景:" << std::endl;
    MockVideoPipeline pipeline;
    pipeline.initialize("camera_001");
    
    // 模拟处理一些帧
    for (int i = 1; i <= 105; ++i) {
        pipeline.processFrame(i);
    }
    
    // 刷新日志
    Logger::getInstance().flush();
    
    std::cout << "\n=== 演示完成 ===" << std::endl;
    std::cout << "请查看生成的日志文件: logs/application.log" << std::endl;
    
    return 0;
}

/*
编译和运行:
g++ -std=c++17 -pthread logger_replacement_example.cpp src/core/Logger.cpp -lstdc++fs -o logger_example
./logger_example

预期效果:
1. 控制台显示彩色格式化日志
2. 生成logs/application.log文件
3. 展示原始代码和新代码的对比
4. 演示各种Logger功能

替换步骤:
1. 包含Logger头文件: #include "src/core/Logger.h"
2. 配置Logger（通常在main函数开始）
3. 替换std::cout -> LOG_INFO()
4. 替换std::cerr -> LOG_ERROR()
5. 根据需要使用其他日志级别
6. 使用条件日志等高级功能

性能提示:
- 生产环境建议使用INFO级别以上
- 避免在TRACE/DEBUG日志中进行复杂计算
- 合理使用文件输出和轮转策略
*/
