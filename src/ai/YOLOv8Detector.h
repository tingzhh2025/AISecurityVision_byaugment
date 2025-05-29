#pragma once

#include <opencv2/opencv.hpp>
#ifndef DISABLE_OPENCV_DNN
#include <opencv2/dnn.hpp>
#endif
#include <vector>
#include <string>
#include <memory>

#ifdef HAVE_RKNN
#include "rknn_api.h"
#else
// Forward declaration for RKNN when not available
typedef void* rknn_context;
#endif

/**
 * @brief YOLOv8 object detector with multiple backend support
 *
 * This class implements object detection using YOLOv8 model
 * with support for multiple inference backends:
 * - RKNN RKNPU2 (for RK3588 and other Rockchip NPUs)
 * - OpenCV DNN (CPU fallback)
 * - TensorRT (NVIDIA GPUs)
 *
 * Features:
 * - YOLOv8n/s/m/l/x model support
 * - RKNN NPU acceleration for RK3588
 * - TensorRT FP16/INT8 optimization
 * - Batch processing capability
 * - NMS post-processing
 * - COCO class detection
 */

enum class InferenceBackend {
    AUTO,      // Automatically select best available backend
    RKNN,      // RKNN NPU backend
    OPENCV,    // OpenCV DNN backend
    TENSORRT   // TensorRT backend
};
class YOLOv8Detector {
public:
    // Detection result structure
    struct Detection {
        cv::Rect bbox;
        float confidence;
        int classId;
        std::string className;
    };

    // Letterbox information for coordinate transformation
    struct LetterboxInfo {
        float scale;
        float x_pad;
        float y_pad;
    };

    YOLOv8Detector();
    ~YOLOv8Detector();

    // Initialization
    bool initialize(const std::string& modelPath = "../models/yolov8n.rknn",
                   InferenceBackend backend = InferenceBackend::AUTO);
    void cleanup();
    bool isInitialized() const;

    // Backend information
    InferenceBackend getCurrentBackend() const;
    std::string getBackendName() const;

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

protected:
    // Backend selection
    InferenceBackend m_backend;
    InferenceBackend m_requestedBackend;
    InferenceBackend m_currentBackend;

    // RKNN context and buffers
    rknn_context m_rknnContext;
    void* m_rknnInputBuffer;
    void* m_rknnOutputBuffer;
    rknn_tensor_attr m_rknnInputAttrs;

    // OpenCV DNN network (only when DNN is enabled)
#ifndef DISABLE_OPENCV_DNN
    cv::dnn::Net m_dnnNet;
#endif

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

    // Backend-specific methods
    bool initializeRKNN(const std::string& modelPath);
    bool initializeOpenCV(const std::string& modelPath);
    bool initializeTensorRT(const std::string& modelPath);

    std::vector<Detection> detectWithRKNN(const cv::Mat& frame);
    std::vector<Detection> detectWithOpenCV(const cv::Mat& frame);
    std::vector<Detection> detectWithTensorRT(const cv::Mat& frame);

    void cleanupRKNN();
    void cleanupOpenCV();
    void cleanupTensorRT();

    // RKNN-specific helper methods
    cv::Mat preprocessImageForRKNN(const cv::Mat& image);
    cv::Mat preprocessImageForRKNNWithLetterbox(const cv::Mat& image, LetterboxInfo& letterbox);
    std::vector<Detection> postprocessRKNNResults(const float* output, const cv::Size& originalSize);
    std::vector<Detection> postprocessRKNNResultsWithLetterbox(const float* output, const cv::Size& originalSize, const LetterboxInfo& letterbox);
    std::vector<Detection> postprocessRKNNResultsOfficial(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize);
    std::vector<Detection> postprocessRKNNResultsOfficialWithLetterbox(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize, const LetterboxInfo& letterbox);

private:

    // Official YOLOv8 post-processing helper methods
    int processRKNNOutput(int8_t* box_tensor, int32_t box_zp, float box_scale,
                         int8_t* score_tensor, int32_t score_zp, float score_scale,
                         int grid_h, int grid_w, int stride, int dfl_len,
                         std::vector<float>& boxes, std::vector<float>& objProbs,
                         std::vector<int>& classId, float threshold);
    void computeDFL(float* tensor, int dfl_len, float* box);
    float calculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                          float xmin1, float ymin1, float xmax1, float ymax1);
    void applyNMS(int validCount, std::vector<float>& outputLocations, std::vector<int>& classIds,
                 std::vector<int>& order, int filterId, float threshold);
    void quickSortIndiceInverse(std::vector<float>& input, int left, int right, std::vector<int>& indices);

    // Simulation method for fallback
    std::vector<Detection> simulateDetection(const cv::Mat& frame);

    // Backend detection
    InferenceBackend detectBestBackend() const;
};
