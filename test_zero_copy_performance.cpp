#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "src/ai/YOLOv8Detector.h"
#include "src/ai/YOLOv8DetectorOptimized.h"
#include "src/ai/YOLOv8DetectorZeroCopy.h"

void testDetectorPerformance(const std::string& name, YOLOv8Detector* detector, const cv::Mat& testImage, int iterations = 50) {
    std::cout << "\n=== " << name << " 性能测试 ===" << std::endl;
    
    // 预热
    std::cout << "预热中..." << std::endl;
    for (int i = 0; i < 5; i++) {
        detector->detectObjects(testImage);
    }
    
    // 性能测试
    std::vector<double> times;
    auto totalStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto detections = detector->detectObjects(testImage);
        auto end = std::chrono::high_resolution_clock::now();
        
        double time = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(time);
        
        if (i % 10 == 0) {
            std::cout << "Frame " << i << ": " << time << "ms, " 
                      << detections.size() << " detections" << std::endl;
        }
    }
    
    auto totalEnd = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
    
    // 计算统计
    double avgTime = 0;
    for (double t : times) avgTime += t;
    avgTime /= times.size();
    
    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    
    std::cout << "\n📊 " << name << " 结果:" << std::endl;
    std::cout << "平均时间: " << avgTime << " ms" << std::endl;
    std::cout << "最小时间: " << minTime << " ms" << std::endl;
    std::cout << "最大时间: " << maxTime << " ms" << std::endl;
    std::cout << "平均FPS: " << 1000.0 / avgTime << std::endl;
    std::cout << "总时间: " << totalTime << " ms" << std::endl;
}

int main() {
    std::cout << "🚀 YOLOv8 Zero-Copy 性能对比测试" << std::endl;
    std::cout << "=" * 60 << std::endl;
    
    const std::string modelPath = "models/yolov8n.rknn";
    
    // 检查模型文件
    if (!std::ifstream(modelPath).good()) {
        std::cerr << "❌ 模型文件未找到: " << modelPath << std::endl;
        return -1;
    }
    
    // 创建测试图像
    cv::Mat testImage = cv::Mat::zeros(640, 640, CV_8UC3);
    
    // 添加一些测试内容
    cv::rectangle(testImage, cv::Rect(100, 100, 200, 150), cv::Scalar(255, 0, 0), -1);
    cv::rectangle(testImage, cv::Rect(400, 300, 180, 120), cv::Scalar(0, 255, 0), -1);
    cv::circle(testImage, cv::Point(320, 320), 80, cv::Scalar(0, 0, 255), -1);
    
    std::cout << "✓ 测试图像创建完成: " << testImage.size() << std::endl;
    
    // 测试1: 基础YOLOv8检测器
    std::cout << "\n🔧 测试基础YOLOv8检测器..." << std::endl;
    YOLOv8Detector basicDetector;
    if (basicDetector.initialize(modelPath, InferenceBackend::RKNN)) {
        testDetectorPerformance("基础YOLOv8", &basicDetector, testImage);
    } else {
        std::cout << "❌ 基础检测器初始化失败" << std::endl;
    }
    
    // 测试2: 优化版YOLOv8检测器
    std::cout << "\n⚡ 测试优化版YOLOv8检测器..." << std::endl;
    YOLOv8DetectorOptimized optimizedDetector(3); // 使用3个线程
    if (optimizedDetector.initialize(modelPath, InferenceBackend::RKNN)) {
        testDetectorPerformance("优化版YOLOv8", &optimizedDetector, testImage);
    } else {
        std::cout << "❌ 优化检测器初始化失败" << std::endl;
    }
    
    // 测试3: Zero-Copy YOLOv8检测器
    std::cout << "\n🚀 测试Zero-Copy YOLOv8检测器..." << std::endl;
    YOLOv8DetectorZeroCopy zeroCopyDetector;
    if (zeroCopyDetector.initialize(modelPath, InferenceBackend::RKNN)) {
        testDetectorPerformance("Zero-Copy YOLOv8", &zeroCopyDetector, testImage);
    } else {
        std::cout << "❌ Zero-Copy检测器初始化失败" << std::endl;
    }
    
    std::cout << "\n" << "=" * 60 << std::endl;
    std::cout << "🎯 性能对比测试完成!" << std::endl;
    std::cout << "\n预期性能提升:" << std::endl;
    std::cout << "- Zero-Copy应该比基础版本快20-30%" << std::endl;
    std::cout << "- 内存拷贝次数显著减少" << std::endl;
    std::cout << "- DMA缓冲区直接访问" << std::endl;
    std::cout << "- 目标: 推理时间 < 50ms" << std::endl;
    
    return 0;
}
