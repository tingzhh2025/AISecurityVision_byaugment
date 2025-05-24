#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief YOLOv8 object detector with TensorRT optimization
 * 
 * This class implements object detection using YOLOv8 model
 * optimized for TensorRT inference on NVIDIA GPUs.
 */
class YOLOv8Detector {
public:
    YOLOv8Detector();
    ~YOLOv8Detector();
    
    // Initialization
    bool initialize(const std::string& modelPath = "");
    void cleanup();
    
    // Detection
    std::vector<cv::Rect> detect(const cv::Mat& frame);
    
    // Configuration
    void setConfidenceThreshold(float threshold);
    void setNMSThreshold(float threshold);
    
    // Statistics
    double getInferenceTime() const;
    size_t getDetectionCount() const;

private:
    // TensorRT engine (placeholder)
    void* m_engine;
    void* m_context;
    
    // Configuration
    float m_confidenceThreshold;
    float m_nmsThreshold;
    
    // Model parameters
    int m_inputWidth;
    int m_inputHeight;
    int m_numClasses;
    
    // Statistics
    mutable double m_inferenceTime;
    mutable size_t m_detectionCount;
    
    // Internal methods
    cv::Mat preprocessImage(const cv::Mat& image);
    std::vector<cv::Rect> postprocessResults(const float* output);
};
