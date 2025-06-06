/**
 * @file YOLOv8CPUDetector.h
 * @brief YOLOv8 CPU Implementation (Fallback)
 *
 * This file implements a basic CPU detector as a fallback
 * when hardware acceleration is not available.
 */

#ifndef YOLOV8_CPU_DETECTOR_H
#define YOLOV8_CPU_DETECTOR_H

#include "YOLOv8Detector.h"

namespace AISecurityVision {

/**
 * @brief YOLOv8 detector implementation using CPU
 *
 * This is a placeholder implementation that provides basic functionality
 * when neither TensorRT nor RKNN is available. In a real implementation,
 * this could use ONNX Runtime or OpenCV DNN module.
 */
class YOLOv8CPUDetector : public YOLOv8Detector {
public:
    YOLOv8CPUDetector();
    virtual ~YOLOv8CPUDetector();

    // YOLOv8Detector interface implementation
    bool initialize(const std::string& modelPath) override;
    std::vector<Detection> detectObjects(const cv::Mat& frame) override;
    bool isInitialized() const override;
    InferenceBackend getCurrentBackend() const override;
    std::string getBackendName() const override;
    void cleanup() override;
    std::vector<std::string> getModelInfo() const override;

private:
    // Placeholder for model data
    std::string m_modelPath;
    
    // Simple preprocessing
    cv::Mat preprocessImage(const cv::Mat& image);
    
    // Generate dummy detections for testing
    std::vector<Detection> generateDummyDetections(const cv::Mat& frame);
};

} // namespace AISecurityVision

#endif // YOLOV8_CPU_DETECTOR_H
