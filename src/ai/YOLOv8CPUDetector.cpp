/**
 * @file YOLOv8CPUDetector.cpp
 * @brief YOLOv8 CPU Implementation (Fallback)
 */

#include "YOLOv8CPUDetector.h"
#include <chrono>
#include <random>
#include <thread>
#include <iostream>

namespace AISecurityVision {

YOLOv8CPUDetector::YOLOv8CPUDetector() {
    m_backend = InferenceBackend::CPU;
    initializeDefaultClassNames();
}

YOLOv8CPUDetector::~YOLOv8CPUDetector() {
    cleanup();
}

bool YOLOv8CPUDetector::initialize(const std::string& modelPath) {
    m_modelPath = modelPath;
    m_initialized = true;
    
    std::cout << "[CPU Detector] Initialized with model: " << modelPath << std::endl;
    std::cout << "[CPU Detector] Note: This is a placeholder implementation" << std::endl;
    std::cout << "[CPU Detector] For real CPU inference, consider using ONNX Runtime" << std::endl;
    
    return true;
}

std::vector<Detection> YOLOv8CPUDetector::detectObjects(const cv::Mat& frame) {
    if (!m_initialized) {
        return {};
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Simulate preprocessing
    cv::Mat processed = preprocessImage(frame);
    
    // Simulate inference delay (20-50ms for CPU)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    
    // Generate some dummy detections for testing
    auto detections = generateDummyDetections(frame);
    
    // Update timing
    auto endTime = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_inferenceTimes.push_back(m_inferenceTime);
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }
    
    m_detectionCount += detections.size();
    
    return filterDetectionsByCategory(detections);
}

cv::Mat YOLOv8CPUDetector::preprocessImage(const cv::Mat& image) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(m_inputWidth, m_inputHeight));
    
    cv::Mat normalized;
    resized.convertTo(normalized, CV_32FC3, 1.0 / 255.0);
    
    return normalized;
}

std::vector<Detection> YOLOv8CPUDetector::generateDummyDetections(const cv::Mat& frame) {
    std::vector<Detection> detections;
    
    // Random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> conf_dist(0.3, 0.9);
    std::uniform_int_distribution<> class_dist(0, 5); // Only first 6 classes
    std::uniform_int_distribution<> count_dist(0, 3);
    
    int numDetections = count_dist(gen);
    
    for (int i = 0; i < numDetections; ++i) {
        Detection det;
        
        // Generate random bbox
        int x = frame.cols / 4 + (i * frame.cols / 4);
        int y = frame.rows / 4;
        int w = frame.cols / 6;
        int h = frame.rows / 3;
        
        det.bbox = cv::Rect(x, y, w, h);
        det.confidence = conf_dist(gen);
        det.classId = class_dist(gen);
        
        if (det.classId < m_classNames.size()) {
            det.className = m_classNames[det.classId];
        }
        
        detections.push_back(det);
    }
    
    return detections;
}

bool YOLOv8CPUDetector::isInitialized() const {
    return m_initialized;
}

InferenceBackend YOLOv8CPUDetector::getCurrentBackend() const {
    return m_backend;
}

std::string YOLOv8CPUDetector::getBackendName() const {
    return "CPU (Fallback)";
}

void YOLOv8CPUDetector::cleanup() {
    m_initialized = false;
}

std::vector<std::string> YOLOv8CPUDetector::getModelInfo() const {
    std::vector<std::string> info;
    
    info.push_back("Backend: CPU (Fallback implementation)");
    info.push_back("Model: " + m_modelPath);
    info.push_back("Input size: " + std::to_string(m_inputWidth) + "x" + std::to_string(m_inputHeight));
    info.push_back("Note: This is a placeholder - consider using ONNX Runtime for real CPU inference");
    
    return info;
}

} // namespace AISecurityVision
