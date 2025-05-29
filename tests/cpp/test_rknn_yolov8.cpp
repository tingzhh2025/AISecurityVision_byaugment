#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include "../../src/ai/YOLOv8Detector.h"

int main() {
    std::cout << "=== RKNN YOLOv8 Test ===" << std::endl;

    // Initialize YOLOv8 detector with RKNN backend
    YOLOv8Detector detector;

    // Initialize with RKNN model and specify RKNN backend
    std::string modelPath = "../models/yolov8n.rknn";
    std::cout << "Initializing YOLOv8 detector with RKNN model: " << modelPath << std::endl;

    if (!detector.initialize(modelPath, InferenceBackend::RKNN)) {
        std::cerr << "Failed to initialize YOLOv8 detector with RKNN" << std::endl;
        return -1;
    }

    std::cout << "YOLOv8 detector initialized successfully!" << std::endl;
    std::cout << "Backend: " << detector.getBackendName() << std::endl;

    // Create a test image (640x640 with some content)
    cv::Mat testImage = cv::Mat::zeros(640, 640, CV_8UC3);

    // Draw some test objects
    cv::rectangle(testImage, cv::Rect(100, 100, 200, 300), cv::Scalar(0, 255, 0), -1); // Green rectangle (person-like)
    cv::rectangle(testImage, cv::Rect(400, 400, 150, 100), cv::Scalar(255, 0, 0), -1); // Blue rectangle (car-like)
    cv::circle(testImage, cv::Point(320, 200), 50, cv::Scalar(0, 0, 255), -1); // Red circle

    std::cout << "Created test image: " << testImage.size() << std::endl;

    // Run detection
    std::cout << "Running RKNN inference..." << std::endl;
    auto detections = detector.detectObjects(testImage);

    std::cout << "Detection completed!" << std::endl;
    std::cout << "Number of detections: " << detections.size() << std::endl;

    // Print detection results
    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& det = detections[i];
        std::cout << "Detection " << i << ":" << std::endl;
        std::cout << "  Class: " << det.className << " (ID: " << det.classId << ")" << std::endl;
        std::cout << "  Confidence: " << det.confidence << std::endl;
        std::cout << "  BBox: (" << det.bbox.x << ", " << det.bbox.y << ", "
                  << det.bbox.width << ", " << det.bbox.height << ")" << std::endl;
    }

    // Get performance statistics
    std::cout << "\nPerformance Statistics:" << std::endl;
    std::cout << "Inference time: " << detector.getInferenceTime() << " ms" << std::endl;
    std::cout << "Average inference time: " << detector.getAverageInferenceTime() << " ms" << std::endl;
    std::cout << "Detection count: " << detector.getDetectionCount() << std::endl;

    // Test with different image sizes
    std::cout << "\nTesting with different image sizes..." << std::endl;

    std::vector<cv::Size> testSizes = {
        cv::Size(320, 240),
        cv::Size(640, 480),
        cv::Size(1280, 720),
        cv::Size(1920, 1080)
    };

    for (const auto& size : testSizes) {
        cv::Mat resizedImage;
        cv::resize(testImage, resizedImage, size);

        auto start = std::chrono::high_resolution_clock::now();
        auto dets = detector.detectObjects(resizedImage);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "Size " << size.width << "x" << size.height
                  << ": " << dets.size() << " detections in " << duration << "ms" << std::endl;
    }

    std::cout << "\n=== RKNN YOLOv8 Test Completed ===" << std::endl;
    return 0;
}
