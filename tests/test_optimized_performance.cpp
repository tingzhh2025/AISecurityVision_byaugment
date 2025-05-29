#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <iomanip>
#include <random>

// Include AI components
#include "../src/ai/YOLOv8DetectorOptimized.h"
#include <opencv2/opencv.hpp>

// Global flag for graceful shutdown
volatile bool g_running = true;

void signalHandler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;
}

// Generate synthetic test frames
cv::Mat generateTestFrame(int width = 640, int height = 640) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    cv::Mat frame(height, width, CV_8UC3);
    
    // Fill with random colors to simulate real camera data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            frame.at<cv::Vec3b>(y, x) = cv::Vec3b(dis(gen), dis(gen), dis(gen));
        }
    }
    
    // Add some geometric shapes to make it more realistic
    cv::rectangle(frame, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(255, 0, 0), -1);
    cv::circle(frame, cv::Point(300, 300), 50, cv::Scalar(0, 255, 0), -1);
    cv::rectangle(frame, cv::Point(400, 200), cv::Point(500, 350), cv::Scalar(0, 0, 255), -1);
    
    return frame;
}

void performanceTest(YOLOv8DetectorOptimized& detector, int numFrames = 100) {
    std::cout << "\n🚀 === Starting Performance Test ===" << std::endl;
    std::cout << "Testing with " << numFrames << " synthetic frames..." << std::endl;
    
    std::vector<std::future<std::vector<YOLOv8DetectorOptimized::Detection>>> futures;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Submit all frames asynchronously
    for (int i = 0; i < numFrames; i++) {
        cv::Mat frame = generateTestFrame();
        futures.push_back(detector.detectAsync(frame));
        
        // Small delay to simulate real camera frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "📤 Submitted " << numFrames << " frames for processing..." << std::endl;
    
    // Collect all results
    int totalDetections = 0;
    for (int i = 0; i < numFrames; i++) {
        auto detections = futures[i].get();
        totalDetections += detections.size();
        
        if (i % 20 == 0) {
            std::cout << "✅ Processed frame " << (i + 1) << "/" << numFrames 
                      << " (detections: " << detections.size() << ")" << std::endl;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration<double>(endTime - startTime).count();
    
    // Calculate performance metrics
    double throughput = numFrames / totalTime;
    double avgTimePerFrame = totalTime / numFrames * 1000; // ms
    
    std::cout << "\n📊 === Performance Results ===" << std::endl;
    std::cout << "🔥 Total Throughput: " << std::fixed << std::setprecision(1) << throughput << " FPS" << std::endl;
    std::cout << "⚡ Avg Time per Frame: " << std::setprecision(1) << avgTimePerFrame << " ms" << std::endl;
    std::cout << "🎯 Total Detections: " << totalDetections << std::endl;
    std::cout << "⏱️  Total Time: " << std::setprecision(2) << totalTime << " seconds" << std::endl;
    
    // Get detailed detector stats
    auto stats = detector.getPerformanceStats();
    std::cout << "\n📈 === Detailed Detector Stats ===" << std::endl;
    std::cout << "🧠 Avg Inference Time: " << std::setprecision(1) << stats.avgInferenceTime << " ms" << std::endl;
    std::cout << "⏳ Avg Queue Time: " << std::setprecision(1) << stats.avgQueueTime << " ms" << std::endl;
    std::cout << "📊 Total Inferences: " << stats.totalInferences << std::endl;
    std::cout << "🔄 Detector Throughput: " << std::setprecision(1) << stats.throughput << " FPS" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void continuousPerformanceTest(YOLOv8DetectorOptimized& detector) {
    std::cout << "\n🔄 === Starting Continuous Performance Test ===" << std::endl;
    std::cout << "Running continuous inference to measure sustained performance..." << std::endl;
    std::cout << "⏸️  Press Ctrl+C to stop the test..." << std::endl;
    
    auto lastStatsTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    
    while (g_running) {
        // Generate and process frame
        cv::Mat frame = generateTestFrame();
        auto future = detector.detectAsync(frame);
        auto detections = future.get();
        
        frameCount++;
        
        // Print stats every 5 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - lastStatsTime).count();
        
        if (elapsed >= 5.0) {
            double fps = frameCount / elapsed;
            auto stats = detector.getPerformanceStats();
            
            std::cout << "📊 FPS: " << std::fixed << std::setprecision(1) << fps
                      << ", Inference: " << std::setprecision(1) << stats.avgInferenceTime << "ms"
                      << ", Queue: " << stats.queueSize
                      << ", Detections: " << detections.size()
                      << ", Total: " << stats.totalInferences << std::endl;
            
            // Reset counters
            lastStatsTime = now;
            frameCount = 0;
        }
        
        // Small delay to prevent overwhelming
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "🎉 === OPTIMIZED RKNN YOLOv8 PERFORMANCE TEST ===" << std::endl;
    std::cout << "🧠 Multi-threaded RKNN NPU inference performance evaluation" << std::endl;
    std::cout << "🎯 Testing RK3588 3-core NPU optimization" << std::endl;
    
    // Initialize optimized RKNN YOLOv8 detector with 3 threads
    std::cout << "\nInitializing Optimized RKNN YOLOv8 detector..." << std::endl;
    YOLOv8DetectorOptimized detector(3); // 3 threads for 3 NPU cores
    
    if (!detector.initialize("models/yolov8n.rknn", InferenceBackend::RKNN)) {
        std::cerr << "❌ Failed to initialize optimized RKNN YOLOv8 detector" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Optimized RKNN YOLOv8 detector initialized successfully!" << std::endl;
    std::cout << "🧠 Backend: Multi-threaded RKNN (3 cores)" << std::endl;
    auto inputSize = detector.getInputSize();
    std::cout << "📐 Input size: " << inputSize.width << "x" << inputSize.height << std::endl;
    
    // Set optimized queue size for high throughput
    detector.setMaxQueueSize(10);
    std::cout << "📋 Queue size: 10 frames" << std::endl;
    
    std::cout << "\n🎯 === Testing Scenarios ===" << std::endl;
    std::cout << "1. Batch Performance Test (100 frames)" << std::endl;
    std::cout << "2. Continuous Performance Test (until Ctrl+C)" << std::endl;
    
    // Test 1: Batch performance test
    performanceTest(detector, 100);
    
    if (!g_running) {
        std::cout << "Test interrupted by user" << std::endl;
        return 0;
    }
    
    // Test 2: Continuous performance test
    continuousPerformanceTest(detector);
    
    std::cout << "\n🎯 === Performance Test Completed ===" << std::endl;
    std::cout << "✅ All tests finished successfully" << std::endl;
    
    return 0;
}
