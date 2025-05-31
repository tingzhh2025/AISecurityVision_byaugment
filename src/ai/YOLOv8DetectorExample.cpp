/**
 * @file YOLOv8DetectorExample.cpp
 * @brief Example usage of the refactored YOLOv8 detector classes
 * 
 * This file demonstrates how to use the new YOLOv8 detector architecture
 * with platform-specific implementations (RKNN and TensorRT).
 */

#include "YOLOv8Detector.h"
#include "YOLOv8RKNNDetector.h"
#include "YOLOv8TensorRTDetector.h"
#include "../core/Logger.h"
using namespace AISecurityVision;

#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>

void demonstrateFactoryUsage() {
    LOG_INFO() << "=== YOLOv8 Detector Factory Usage ===";
    
    // Method 1: Auto-detect best available backend
    auto detector = createYOLOv8Detector(InferenceBackend::AUTO);
    if (detector) {
        LOG_INFO() << "Created detector with backend: " << detector->getBackendName();
        
        // Initialize with model
        if (detector->initialize("models/yolov8n.rknn")) {
            LOG_INFO() << "Detector initialized successfully";

            // Print basic information
            LOG_INFO() << "Backend: " << detector->getBackendName();
            LOG_INFO() << "Input size: " << detector->getInputSize().width << "x" << detector->getInputSize().height;
            LOG_INFO() << "Classes: " << detector->getClassNames().size();
        }
    }
    
    // Method 2: Explicitly request RKNN backend
    auto rknnDetector = createYOLOv8Detector(InferenceBackend::RKNN);
    if (rknnDetector) {
        LOG_INFO() << "Created RKNN detector: " << rknnDetector->getBackendName();
    }
    
    // Method 3: Explicitly request TensorRT backend
    auto tensorrtDetector = createYOLOv8Detector(InferenceBackend::TENSORRT);
    if (tensorrtDetector) {
        LOG_INFO() << "Created TensorRT detector: " << tensorrtDetector->getBackendName();
    }
}

void demonstrateDirectUsage() {
    LOG_INFO() << "=== Direct YOLOv8 Detector Usage ===";
    
    // Direct instantiation of RKNN detector
    auto rknnDetector = std::make_unique<YOLOv8RKNNDetector>();
    
    // Configure RKNN-specific settings
    rknnDetector->enableMultiCore(true);
    rknnDetector->setZeroCopyMode(true);
    
    // Initialize with RKNN model
    if (rknnDetector->initialize("models/yolov8n.rknn")) {
        LOG_INFO() << "RKNN detector initialized";
        
        // Set detection parameters
        rknnDetector->setConfidenceThreshold(0.25f);
        rknnDetector->setNMSThreshold(0.45f);
        
        // Load test image
        cv::Mat testImage = cv::imread("test_images/bus.jpg");
        if (!testImage.empty()) {
            // Perform detection
            auto detections = rknnDetector->detectObjects(testImage);
            
            LOG_INFO() << "Found " << detections.size() << " detections";
            LOG_INFO() << "Inference time: " << rknnDetector->getInferenceTime() << " ms";
            
            // Print detection results
            for (const auto& detection : detections) {
                LOG_INFO() << "Detection: " << detection.className 
                          << " (confidence: " << detection.confidence << ")"
                          << " at [" << detection.bbox.x << ", " << detection.bbox.y 
                          << ", " << detection.bbox.width << ", " << detection.bbox.height << "]";
            }
        }
    }
    
    // Direct instantiation of TensorRT detector
    auto tensorrtDetector = std::make_unique<YOLOv8TensorRTDetector>();
    
    // Configure TensorRT-specific settings
    tensorrtDetector->setPrecision(YOLOv8TensorRTDetector::Precision::FP16);
    tensorrtDetector->setMaxBatchSize(4);
    tensorrtDetector->setWorkspaceSize(1 << 30); // 1GB
    
    // Initialize with ONNX model (will build TensorRT engine)
    if (tensorrtDetector->initialize("models/yolov8n.onnx")) {
        LOG_INFO() << "TensorRT detector initialized";
        
        // Test batch processing
        std::vector<cv::Mat> batch;
        cv::Mat testImage = cv::imread("test_images/bus.jpg");
        if (!testImage.empty()) {
            batch.push_back(testImage);
            batch.push_back(testImage.clone());
            
            auto batchResults = tensorrtDetector->detectBatch(batch);
            LOG_INFO() << "Batch processing completed for " << batchResults.size() << " images";
        }
    }
}

void demonstratePolymorphicUsage() {
    LOG_INFO() << "=== Polymorphic YOLOv8 Detector Usage ===";
    
    // Create different detectors and use them polymorphically
    std::vector<std::unique_ptr<YOLOv8Detector>> detectors;
    
    // Add RKNN detector
    auto rknnDetector = std::make_unique<YOLOv8RKNNDetector>();
    if (rknnDetector->initialize("models/yolov8n.rknn")) {
        detectors.push_back(std::move(rknnDetector));
    }
    
    // Add TensorRT detector
    auto tensorrtDetector = std::make_unique<YOLOv8TensorRTDetector>();
    if (tensorrtDetector->initialize("models/yolov8n.onnx")) {
        detectors.push_back(std::move(tensorrtDetector));
    }
    
    // Use all detectors polymorphically
    cv::Mat testImage = cv::imread("test_images/bus.jpg");
    if (!testImage.empty()) {
        for (auto& detector : detectors) {
            LOG_INFO() << "Testing " << detector->getBackendName() << " detector";
            
            auto detections = detector->detectObjects(testImage);
            LOG_INFO() << "Found " << detections.size() << " detections in " 
                      << detector->getInferenceTime() << " ms";
            
            // Print performance statistics
            LOG_INFO() << "Average inference time: " << detector->getAverageInferenceTime() << " ms";
            LOG_INFO() << "Total detections: " << detector->getDetectionCount();
        }
    }
}

int main() {
    LOG_INFO() << "YOLOv8 Detector Refactoring Example";
    LOG_INFO() << "====================================";
    
    try {
        // Demonstrate different usage patterns
        demonstrateFactoryUsage();
        demonstrateDirectUsage();
        demonstratePolymorphicUsage();
        
        LOG_INFO() << "Example completed successfully";
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "Exception: " << e.what();
        return 1;
    }
}
