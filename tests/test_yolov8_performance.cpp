#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "../../src/ai/YOLOv8DetectorOptimized.h"

int main() {
    std::cout << "=== YOLOv8 Performance Test ===" << std::endl;

    // Test parameters
    const std::string modelPath = "models/yolov8n.rknn";
    const int numThreads = 3; // Use all 3 NPU cores
    const int testFrames = 100;

    // Create optimized detector
    YOLOv8DetectorOptimized detector(numThreads);

    // Initialize with RKNN backend
    if (!detector.initialize(modelPath, InferenceBackend::RKNN)) {
        std::cerr << "Failed to initialize YOLOv8 detector" << std::endl;
        return -1;
    }

    // Create test image (640x640 for YOLOv8)
    cv::Mat testImage = cv::Mat::zeros(640, 640, CV_8UC3);

    // Add some test patterns to make it more realistic
    cv::rectangle(testImage, cv::Rect(100, 100, 200, 150), cv::Scalar(255, 0, 0), -1);
    cv::rectangle(testImage, cv::Rect(400, 300, 180, 120), cv::Scalar(0, 255, 0), -1);
    cv::circle(testImage, cv::Point(320, 320), 80, cv::Scalar(0, 0, 255), -1);

    std::cout << "Running " << testFrames << " inference tests..." << std::endl;

    // Warm-up runs
    std::cout << "Warming up..." << std::endl;
    for (int i = 0; i < 10; i++) {
        auto detections = detector.detect(testImage);
    }

    // Performance test
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<double> inferenceTimes;

    for (int i = 0; i < testFrames; i++) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        auto detections = detector.detect(testImage);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        double frameTime = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        inferenceTimes.push_back(frameTime);

        if (i % 10 == 0) {
            std::cout << "Frame " << i << ": " << frameTime << "ms, "
                      << detections.size() << " detections" << std::endl;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Calculate statistics
    double avgTime = totalTime / testFrames;
    double minTime = *std::min_element(inferenceTimes.begin(), inferenceTimes.end());
    double maxTime = *std::max_element(inferenceTimes.begin(), inferenceTimes.end());

    // Calculate standard deviation
    double sum = 0.0;
    for (double time : inferenceTimes) {
        sum += (time - avgTime) * (time - avgTime);
    }
    double stdDev = sqrt(sum / testFrames);

    std::cout << "\n=== Performance Results ===" << std::endl;
    std::cout << "Total frames: " << testFrames << std::endl;
    std::cout << "Total time: " << totalTime << " ms" << std::endl;
    std::cout << "Average inference time: " << avgTime << " ms" << std::endl;
    std::cout << "Min inference time: " << minTime << " ms" << std::endl;
    std::cout << "Max inference time: " << maxTime << " ms" << std::endl;
    std::cout << "Standard deviation: " << stdDev << " ms" << std::endl;
    std::cout << "Average FPS: " << 1000.0 / avgTime << std::endl;

    // Get detailed performance stats
    auto stats = detector.getPerformanceStats();
    std::cout << "\n=== Detailed Stats ===" << std::endl;
    std::cout << "Queue time: " << stats.avgQueueTime << " ms" << std::endl;
    std::cout << "Throughput: " << stats.throughput << " FPS" << std::endl;
    std::cout << "Queue size: " << stats.queueSize << std::endl;

    // Performance evaluation
    std::cout << "\n=== Performance Evaluation ===" << std::endl;
    if (avgTime <= 50.0) {
        std::cout << "✓ EXCELLENT: Performance meets RK3588 expectations!" << std::endl;
    } else if (avgTime <= 100.0) {
        std::cout << "✓ GOOD: Performance is acceptable" << std::endl;
    } else if (avgTime <= 200.0) {
        std::cout << "⚠ FAIR: Performance could be improved" << std::endl;
    } else {
        std::cout << "✗ POOR: Performance needs optimization" << std::endl;
        std::cout << "Expected: ~13-50ms for YOLOv8n on RK3588" << std::endl;
        std::cout << "Actual: " << avgTime << "ms" << std::endl;

        std::cout << "\nTroubleshooting suggestions:" << std::endl;
        std::cout << "1. Run: sudo ./scripts/optimize_npu_performance.sh" << std::endl;
        std::cout << "2. Check model format (should be .rknn)" << std::endl;
        std::cout << "3. Verify NPU driver: cat /sys/kernel/debug/rknpu/version" << std::endl;
        std::cout << "4. Check NPU frequency: cat /sys/class/devfreq/fdab0000.npu/cur_freq" << std::endl;
    }

    return 0;
}
