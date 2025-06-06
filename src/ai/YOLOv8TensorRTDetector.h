/**
 * @file YOLOv8TensorRTDetector.h
 * @brief YOLOv8 TensorRT GPU Implementation
 *
 * This file implements YOLOv8 object detection using NVIDIA TensorRT
 * for GPU acceleration on x86_64 and other CUDA-capable platforms.
 */

#ifndef YOLOV8_TENSORRT_DETECTOR_H
#define YOLOV8_TENSORRT_DETECTOR_H

#include "YOLOv8Detector.h"
#include <memory>
#include <vector>
#include <string>

#ifdef HAVE_TENSORRT
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>
#endif

namespace AISecurityVision {

/**
 * @brief YOLOv8 detector implementation using TensorRT GPU
 *
 * This class implements YOLOv8 object detection using NVIDIA's TensorRT
 * for hardware acceleration on CUDA-capable GPUs.
 */
class YOLOv8TensorRTDetector : public YOLOv8Detector {
public:
    YOLOv8TensorRTDetector();
    virtual ~YOLOv8TensorRTDetector();

    // YOLOv8Detector interface implementation
    bool initialize(const std::string& modelPath) override;
    std::vector<Detection> detectObjects(const cv::Mat& frame) override;
    bool isInitialized() const override;
    InferenceBackend getCurrentBackend() const override;
    std::string getBackendName() const override;
    void cleanup() override;
    std::vector<std::string> getModelInfo() const override;

    // TensorRT-specific methods
    bool setMaxBatchSize(int batchSize);
    bool setWorkspaceSize(size_t size);
    bool setPrecision(const std::string& precision); // "FP32", "FP16", "INT8"
    bool enableDLA(int dlaCore); // For Jetson platforms
    
    // Model conversion methods
    bool buildEngineFromONNX(const std::string& onnxPath, const std::string& enginePath);
    bool loadEngine(const std::string& enginePath);
    bool saveEngine(const std::string& enginePath);

private:
#ifdef HAVE_TENSORRT
    // TensorRT logger
    class Logger : public nvinfer1::ILogger {
    public:
        void log(Severity severity, const char* msg) noexcept override;
    };

    // TensorRT components
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<nvinfer1::IRuntime> m_runtime;
    std::unique_ptr<nvinfer1::ICudaEngine> m_engine;
    std::unique_ptr<nvinfer1::IExecutionContext> m_context;

    // CUDA buffers
    void* m_deviceBuffers[3]; // input, output boxes, output scores
    float* m_hostInputBuffer;
    float* m_hostOutputBuffer;
    cudaStream_t m_cudaStream;  // Dedicated CUDA stream for inference
    
    // Model properties
    int m_maxBatchSize = 1;
    size_t m_workspaceSize = 1ULL << 30; // 1GB default
    std::string m_precision = "FP16";
    int m_dlaCore = -1; // -1 means use GPU
    
    // Input/Output dimensions
    int m_inputIndex;
    int m_outputBoxesIndex;
    int m_outputScoresIndex;
    std::string m_inputName;
    std::string m_outputBoxesName;
    nvinfer1::Dims m_inputDims;
    nvinfer1::Dims m_outputBoxesDims;
    nvinfer1::Dims m_outputScoresDims;
    
    // Preprocessing and postprocessing
    cv::Mat preprocessImageWithLetterbox(const cv::Mat& image, LetterboxInfo& letterbox);
    std::vector<Detection> postprocessResults(float* boxes, float* scores,
                                            int numDetections,
                                            const cv::Size& originalSize,
                                            const LetterboxInfo& letterbox);
    
    // Helper methods
    bool allocateBuffers();
    void freeBuffers();
    bool doInference(const cv::Mat& input);
    
    // NMS implementation
    std::vector<Detection> performNMS(const std::vector<Detection>& detections);
    
    // Utility functions
    size_t getSizeByDim(const nvinfer1::Dims& dims);
    bool fileExists(const std::string& path);
    std::string getPrecisionString(nvinfer1::DataType dataType);
#endif
};

} // namespace AISecurityVision

#endif // YOLOV8_TENSORRT_DETECTOR_H
