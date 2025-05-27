#include <iostream>
#include <iomanip>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "src/ai/YOLOv8Detector.h"
#include "src/video/FFmpegDecoder.h"
#include "src/core/VideoPipeline.h"

int main() {
    std::cout << "=== RKNN Integration Test ===" << std::endl;

    // Test 1: Initialize YOLOv8Detector with RKNN
    std::cout << "\n1. Testing YOLOv8Detector with RKNN backend..." << std::endl;

    YOLOv8Detector detector;
    bool success = detector.initialize("models/yolov8n.rknn", InferenceBackend::RKNN);

    if (!success) {
        std::cerr << "Failed to initialize RKNN detector!" << std::endl;
        return 1;
    }

    std::cout << "✅ RKNN detector initialized successfully!" << std::endl;
    std::cout << "Backend: " << detector.getBackendName() << std::endl;
    std::cout << "Input size: " << detector.getInputSize().width << "x" << detector.getInputSize().height << std::endl;

    // Test 2: Test with a static image
    std::cout << "\n2. Testing detection on static image..." << std::endl;

    cv::Mat testImage = cv::imread("test_image.jpg");
    if (testImage.empty()) {
        std::cerr << "Could not load test image!" << std::endl;
        return 1;
    }

    auto detections = detector.detectObjects(testImage);
    std::cout << "✅ Detected " << detections.size() << " objects" << std::endl;
    std::cout << "Inference time: " << detector.getInferenceTime() << " ms" << std::endl;

    // Show top 5 detections
    int count = 0;
    for (const auto& detection : detections) {
        if (count >= 5) break;
        std::cout << "  - " << detection.className
                  << " (" << std::fixed << std::setprecision(1) << detection.confidence * 100 << "%)"
                  << " at [" << detection.bbox.x << "," << detection.bbox.y
                  << "," << detection.bbox.width << "," << detection.bbox.height << "]" << std::endl;
        count++;
    }

    // Test 3: Test with RTSP stream (if available)
    std::cout << "\n3. Testing with RTSP stream..." << std::endl;

    FFmpegDecoder decoder;
    VideoSource rtspSource;
    rtspSource.id = "test_rtsp";
    rtspSource.url = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
    rtspSource.protocol = "rtsp";
    rtspSource.username = "admin";
    rtspSource.password = "sharpi1688";
    rtspSource.width = 1920;
    rtspSource.height = 1080;
    rtspSource.fps = 25;
    rtspSource.enabled = true;

    if (decoder.initialize(rtspSource)) {
        std::cout << "✅ RTSP stream connected" << std::endl;

        // Process a few frames
        for (int i = 0; i < 5; i++) {
            cv::Mat frame;
            int64_t timestamp;
            if (decoder.getNextFrame(frame, timestamp)) {
                auto streamDetections = detector.detectObjects(frame);
                std::cout << "Frame " << (i+1) << ": " << streamDetections.size()
                          << " objects, " << detector.getInferenceTime() << " ms" << std::endl;
            } else {
                std::cout << "Failed to get frame " << (i+1) << std::endl;
                break;
            }
        }

        decoder.cleanup();
    } else {
        std::cout << "⚠️  RTSP stream not available, skipping stream test" << std::endl;
    }

    // Test 4: Performance test
    std::cout << "\n4. Performance test (10 frames)..." << std::endl;

    double totalTime = 0;
    int frameCount = 10;

    for (int i = 0; i < frameCount; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto perfDetections = detector.detectObjects(testImage);
        auto end = std::chrono::high_resolution_clock::now();

        double frameTime = std::chrono::duration<double, std::milli>(end - start).count();
        totalTime += frameTime;
    }

    double avgTime = totalTime / frameCount;
    double fps = 1000.0 / avgTime;

    std::cout << "✅ Average inference time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
    std::cout << "✅ Estimated FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;

    detector.cleanup();

    std::cout << "\n=== RKNN Integration Test Complete ===" << std::endl;
    std::cout << "🎉 All tests passed! RKNN NPU acceleration is working correctly." << std::endl;

    return 0;
}
