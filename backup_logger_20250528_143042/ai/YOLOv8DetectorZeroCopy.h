#pragma once

#include "YOLOv8Detector.h"
#include <memory>

#ifdef HAVE_RKNN
#include <rknn_api.h>
#endif

/**
 * YOLOv8检测器 - Zero Copy优化版本
 * 基于官方RKNN模型库的zero-copy实现
 * 最大化性能，减少内存拷贝
 */
class YOLOv8DetectorZeroCopy : public YOLOv8Detector {
public:
    YOLOv8DetectorZeroCopy();
    virtual ~YOLOv8DetectorZeroCopy();

    // 重写初始化方法以启用zero-copy模式
    bool initialize(const std::string& modelPath, InferenceBackend backend) override;

    // 重写检测方法以使用zero-copy优化
    std::vector<Detection> detectObjects(const cv::Mat& frame) override;

private:
#ifdef HAVE_RKNN
    // Zero-copy相关成员
    rknn_input_output_num m_ioNum;
    rknn_tensor_attr* m_inputAttrs;
    rknn_tensor_attr* m_outputAttrs;
    
    // 预分配的输入输出缓冲区
    void* m_inputBuffer;
    size_t m_inputBufferSize;
    
    // DMA缓冲区用于zero-copy
    rknn_tensor_mem* m_inputMem;
    rknn_tensor_mem* m_outputMem;
    
    // Zero-copy模式标志
    bool m_zeroCopyEnabled;
    
    // 初始化zero-copy模式
    bool initializeZeroCopy();
    
    // 清理zero-copy资源
    void cleanupZeroCopy();
    
    // Zero-copy推理
    std::vector<Detection> inferenceZeroCopy(const cv::Mat& frame);
    
    // 优化的预处理（直接写入DMA缓冲区）
    bool preprocessToBuffer(const cv::Mat& frame, void* buffer);
    
    // 优化的后处理（直接从DMA缓冲区读取）
    std::vector<Detection> postprocessFromBuffer(void* buffer, const cv::Size& originalSize);
#endif
};

// 性能统计结构
struct ZeroCopyPerformanceStats {
    double avgPreprocessTime = 0.0;
    double avgInferenceTime = 0.0;
    double avgPostprocessTime = 0.0;
    double avgTotalTime = 0.0;
    int frameCount = 0;
    
    void update(double preprocess, double inference, double postprocess) {
        frameCount++;
        double alpha = 1.0 / frameCount;
        avgPreprocessTime = avgPreprocessTime * (1 - alpha) + preprocess * alpha;
        avgInferenceTime = avgInferenceTime * (1 - alpha) + inference * alpha;
        avgPostprocessTime = avgPostprocessTime * (1 - alpha) + postprocess * alpha;
        avgTotalTime = avgPreprocessTime + avgInferenceTime + avgPostprocessTime;
    }
    
    void print() const {
        std::cout << "=== Zero-Copy Performance Stats ===" << std::endl;
        std::cout << "Frames processed: " << frameCount << std::endl;
        std::cout << "Avg preprocess: " << avgPreprocessTime << " ms" << std::endl;
        std::cout << "Avg inference: " << avgInferenceTime << " ms" << std::endl;
        std::cout << "Avg postprocess: " << avgPostprocessTime << " ms" << std::endl;
        std::cout << "Avg total: " << avgTotalTime << " ms" << std::endl;
        std::cout << "Avg FPS: " << (avgTotalTime > 0 ? 1000.0 / avgTotalTime : 0) << std::endl;
    }
};
