/**
 * @file test_yolov8_backends.cpp
 * @brief Test program for YOLOv8 multi-backend support
 */

#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <iomanip>
#include <memory>
#include <vector>
#include <numeric>
#include <algorithm>

#include "ai/YOLOv8DetectorFactory.h"
#include "ai/YOLOv8Detector.h"

using namespace AISecurityVision;

void printSystemInfo() {
    std::cout << "\n" << YOLOv8DetectorFactory::getSystemInfo() << "\n" << std::endl;
}

void testBackend(InferenceBackend backend, const std::string& modelPath, const cv::Mat& testImage) {
    std::cout << "\n=== Testing " << YOLOv8DetectorFactory::getBackendName(backend) << " ===" << std::endl;
    
    // Check if backend is available
    if (!YOLOv8DetectorFactory::isBackendAvailable(backend)) {
        std::cout << "Backend not available on this system" << std::endl;
        return;
    }
    
    // Create detector
    auto detector = YOLOv8DetectorFactory::createDetector(backend);
    if (!detector) {
        std::cerr << "Failed to create detector" << std::endl;
        return;
    }
    
    // Initialize
    std::cout << "Initializing with model: " << modelPath << std::endl;
    if (!detector->initialize(modelPath)) {
        std::cerr << "Failed to initialize detector" << std::endl;
        return;
    }
    
    // Print model info
    auto modelInfo = detector->getModelInfo();
    std::cout << "\nModel Information:" << std::endl;
    for (const auto& info : modelInfo) {
        std::cout << "  " << info << std::endl;
    }
    
    // Set detection parameters
    detector->setConfidenceThreshold(0.25f);
    detector->setNMSThreshold(0.45f);
    
    // Enable only specific categories for testing
    std::vector<std::string> enabledCategories = {
        "person", "car", "truck", "bus", "bicycle", "motorcycle"
    };
    detector->setEnabledCategories(enabledCategories);
    
    // Warm-up runs
    std::cout << "\nPerforming warm-up runs..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        detector->detectObjects(testImage);
    }
    
    // Performance test
    std::cout << "\nPerformance Test (10 runs):" << std::endl;
    std::vector<double> times;
    std::vector<Detection> lastDetections;
    
    for (int i = 0; i < 10; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        lastDetections = detector->detectObjects(testImage);
        auto end = std::chrono::high_resolution_clock::now();
        
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(ms);
        
        std::cout << "  Run " << (i + 1) << ": " << std::fixed << std::setprecision(2) 
                  << ms << " ms, " << lastDetections.size() << " detections" << std::endl;
    }
    
    // Calculate statistics
    double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    
    std::cout << "\nPerformance Summary:" << std::endl;
    std::cout << "  Average: " << std::fixed << std::setprecision(2) << avgTime << " ms" 
              << " (" << (1000.0 / avgTime) << " FPS)" << std::endl;
    std::cout << "  Min: " << minTime << " ms" << std::endl;
    std::cout << "  Max: " << maxTime << " ms" << std::endl;
    
    // Show detection results
    if (!lastDetections.empty()) {
        std::cout << "\nLast Frame Detections:" << std::endl;
        for (const auto& det : lastDetections) {
            std::cout << "  - " << det.className 
                     << " (conf: " << std::fixed << std::setprecision(2) << det.confidence
                     << ") at [" << det.bbox.x << ", " << det.bbox.y 
                     << ", " << det.bbox.width << ", " << det.bbox.height << "]" << std::endl;
        }
    }
    
    // Draw detections on image
    cv::Mat resultImage = testImage.clone();
    for (const auto& det : lastDetections) {
        // Draw bounding box
        cv::rectangle(resultImage, det.bbox, cv::Scalar(0, 255, 0), 2);
        
        // Draw label
        std::string label = det.className + " " + std::to_string(int(det.confidence * 100)) + "%";
        int baseLine;
        cv::Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        
        cv::rectangle(resultImage, 
                     cv::Point(det.bbox.x, det.bbox.y - labelSize.height - 10),
                     cv::Point(det.bbox.x + labelSize.width, det.bbox.y),
                     cv::Scalar(0, 255, 0), cv::FILLED);
        
        cv::putText(resultImage, label,
                   cv::Point(det.bbox.x, det.bbox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
    
    // Save result image
    std::string outputPath = "result_" + YOLOv8DetectorFactory::getBackendName(backend) + ".jpg";
    cv::imwrite(outputPath, resultImage);
    std::cout << "\nResult saved to: " << outputPath << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image_path> [model_path]" << std::endl;
        std::cout << "Example: " << argv[0] << " test.jpg models/yolov8n.rknn" << std::endl;
        return 1;
    }
    
    std::string imagePath = argv[1];
    std::string modelPath = argc >= 3 ? argv[2] : "models/yolov8n";
    
    // Print system information
    printSystemInfo();
    
    // Load test image
    cv::Mat testImage = cv::imread(imagePath);
    if (testImage.empty()) {
        std::cerr << "Failed to load image: " << imagePath << std::endl;
        return 1;
    }
    
    std::cout << "Loaded image: " << imagePath << " (" 
              << testImage.cols << "x" << testImage.rows << ")" << std::endl;
    
    // Test available backends
    auto backends = YOLOv8DetectorFactory::getAvailableBackends();
    
    for (const auto& backend : backends) {
        std::string backendModelPath = modelPath;
        
        // Adjust model path based on backend
        switch (backend) {
            case InferenceBackend::TENSORRT:
                if (modelPath.find(".engine") == std::string::npos) {
                    backendModelPath += "_fp16.engine";
                }
                break;
            case InferenceBackend::RKNN:
                if (modelPath.find(".rknn") == std::string::npos) {
                    backendModelPath += ".rknn";
                }
                break;
            case InferenceBackend::ONNX:
            case InferenceBackend::CPU:
                if (modelPath.find(".onnx") == std::string::npos) {
                    backendModelPath += ".onnx";
                }
                break;
        }
        
        try {
            testBackend(backend, backendModelPath, testImage);
        } catch (const std::exception& e) {
            std::cerr << "Error testing " << YOLOv8DetectorFactory::getBackendName(backend) 
                     << ": " << e.what() << std::endl;
        }
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
