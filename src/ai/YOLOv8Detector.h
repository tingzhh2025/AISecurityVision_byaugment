#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

/**
 * @brief YOLOv8 object detector with TensorRT optimization
 *
 * This class implements object detection using YOLOv8 model
 * optimized for TensorRT inference on NVIDIA GPUs.
 *
 * Features:
 * - YOLOv8n/s/m/l/x model support
 * - TensorRT FP16/INT8 optimization
 * - Batch processing capability
 * - NMS post-processing
 * - COCO class detection
 */
class YOLOv8Detector {
public:
    // Detection result structure
    struct Detection {
        cv::Rect bbox;
        float confidence;
        int classId;
        std::string className;
    };

    YOLOv8Detector();
    ~YOLOv8Detector();

    // Initialization
    bool initialize(const std::string& modelPath = "models/yolov8n.onnx");
    void cleanup();
    bool isInitialized() const;

    // Detection
    std::vector<Detection> detectObjects(const cv::Mat& frame);
    std::vector<cv::Rect> detect(const cv::Mat& frame); // Legacy interface

    // Batch processing
    std::vector<std::vector<Detection>> detectBatch(const std::vector<cv::Mat>& frames);

    // Configuration
    void setConfidenceThreshold(float threshold);
    void setNMSThreshold(float threshold);
    void setInputSize(int width, int height);

    // Model information
    std::vector<std::string> getClassNames() const;
    cv::Size getInputSize() const;

    // Statistics
    double getInferenceTime() const;
    size_t getDetectionCount() const;
    float getAverageInferenceTime() const;

private:
    // TensorRT engine and context
    void* m_engine;
    void* m_context;
    void* m_stream;

    // GPU memory buffers
    void* m_inputBuffer;
    void* m_outputBuffer;

    // Configuration
    float m_confidenceThreshold;
    float m_nmsThreshold;
    bool m_initialized;

    // Model parameters
    int m_inputWidth;
    int m_inputHeight;
    int m_numClasses;
    int m_maxDetections;
    size_t m_inputSize;
    size_t m_outputSize;

    // Class names (COCO dataset)
    std::vector<std::string> m_classNames;

    // Statistics
    mutable double m_inferenceTime;
    mutable size_t m_detectionCount;
    mutable std::vector<double> m_inferenceTimes;

    // Internal methods
    bool loadModel(const std::string& modelPath);
    bool setupTensorRT();
    cv::Mat preprocessImage(const cv::Mat& image);
    std::vector<Detection> postprocessResults(const float* output, const cv::Size& originalSize);
    void initializeClassNames();
    bool allocateBuffers();
    void deallocateBuffers();
};
