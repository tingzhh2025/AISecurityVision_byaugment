/**
 * @file YOLOv8Detector.h
 * @brief YOLOv8 Object Detection Interface
 *
 * This file defines the base interface for YOLOv8 object detection.
 * Based on the reference implementation from /userdata/source/source/yolov8_rknn
 */

#ifndef YOLOV8_DETECTOR_H
#define YOLOV8_DETECTOR_H

#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>

namespace AISecurityVision {

/**
 * @brief Inference backend types
 */
enum class InferenceBackend {
    RKNN,
    TENSORRT,
    ONNX,
    CPU
};
/**
 * @brief Object detection result structure
 */
struct Detection {
    cv::Rect bbox;          // Bounding box
    float confidence;       // Detection confidence
    int classId;           // Class ID
    std::string className; // Class name

    Detection() : confidence(0.0f), classId(-1) {}
};

/**
 * @brief Letterbox information for maintaining aspect ratio
 */
struct LetterboxInfo {
    float scale;    // Scale factor
    float x_pad;    // X padding
    float y_pad;    // Y padding

    LetterboxInfo() : scale(1.0f), x_pad(0.0f), y_pad(0.0f) {}
};

/**
 * @brief Base class for YOLOv8 object detection
 *
 * This class provides a common interface for different YOLOv8 implementations
 * (RKNN, TensorRT, etc.) based on the reference implementation structure.
 */
class YOLOv8Detector {
public:
    YOLOv8Detector();
    virtual ~YOLOv8Detector();

    /**
     * @brief Initialize the detector with a model file
     * @param modelPath Path to the model file
     * @return true if initialization successful, false otherwise
     */
    virtual bool initialize(const std::string& modelPath) = 0;

    /**
     * @brief Detect objects in an image
     * @param frame Input image
     * @return Vector of detected objects
     */
    virtual std::vector<Detection> detectObjects(const cv::Mat& frame) = 0;

    /**
     * @brief Check if detector is initialized
     * @return true if initialized, false otherwise
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Get current inference backend
     * @return Current backend type
     */
    virtual InferenceBackend getCurrentBackend() const = 0;

    /**
     * @brief Get backend name as string
     * @return Backend name
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief Cleanup resources
     */
    virtual void cleanup() = 0;

    // Configuration methods
    void setConfidenceThreshold(float threshold) { m_confidenceThreshold = threshold; }
    void setNMSThreshold(float threshold) { m_nmsThreshold = threshold; }
    void setClassNames(const std::vector<std::string>& classNames) { m_classNames = classNames; }

    float getConfidenceThreshold() const { return m_confidenceThreshold; }
    float getNMSThreshold() const { return m_nmsThreshold; }
    const std::vector<std::string>& getClassNames() const { return m_classNames; }

    // Category filtering methods
    void setEnabledCategories(const std::vector<std::string>& categories);
    const std::vector<std::string>& getEnabledCategories() const { return m_enabledCategories; }
    const std::vector<std::string>& getAvailableCategories() const { return m_classNames; }
    bool isCategoryEnabled(const std::string& category) const;
    bool isCategoryEnabled(int classId) const;

    // Performance metrics
    double getLastInferenceTime() const { return m_inferenceTime; }
    double getAverageInferenceTime() const;
    size_t getDetectionCount() const { return m_detectionCount; }

    // Model information
    virtual std::vector<std::string> getModelInfo() const = 0;

protected:
    // Model parameters
    int m_inputWidth = 640;
    int m_inputHeight = 640;
    int m_numClasses = 80;

    // Detection parameters (matching reference implementation)
    float m_confidenceThreshold = 0.25f;  // BOX_THRESH
    float m_nmsThreshold = 0.45f;         // NMS_THRESH

    // Class names (COCO dataset by default)
    std::vector<std::string> m_classNames;

    // Category filtering
    std::vector<std::string> m_enabledCategories;

    // State
    bool m_initialized = false;
    InferenceBackend m_backend = InferenceBackend::CPU;

    // Performance tracking
    double m_inferenceTime = 0.0;
    std::vector<double> m_inferenceTimes;
    size_t m_detectionCount = 0;

    /**
     * @brief Load COCO class names
     * @param labelPath Path to label file
     * @return true if successful, false otherwise
     */
    bool loadClassNames(const std::string& labelPath);

    /**
     * @brief Filter detections based on enabled categories
     * @param detections Input detections to filter
     * @return Filtered detections containing only enabled categories
     */
    std::vector<Detection> filterDetectionsByCategory(const std::vector<Detection>& detections) const;

    /**
     * @brief Initialize default COCO class names
     */
    void initializeDefaultClassNames();
};

} // namespace AISecurityVision

#endif // YOLOV8_DETECTOR_H
