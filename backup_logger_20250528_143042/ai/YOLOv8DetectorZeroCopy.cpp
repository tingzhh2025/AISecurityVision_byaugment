#include "YOLOv8DetectorZeroCopy.h"
#include <iostream>
#include <chrono>
#include <cstring>

#ifdef HAVE_RKNN
#include <rknn_api.h>
#endif

static ZeroCopyPerformanceStats g_perfStats;

YOLOv8DetectorZeroCopy::YOLOv8DetectorZeroCopy() 
    : YOLOv8Detector()
#ifdef HAVE_RKNN
    , m_inputAttrs(nullptr)
    , m_outputAttrs(nullptr)
    , m_inputBuffer(nullptr)
    , m_inputBufferSize(0)
    , m_inputMem(nullptr)
    , m_outputMem(nullptr)
    , m_zeroCopyEnabled(false)
#endif
{
    std::cout << "[YOLOv8DetectorZeroCopy] Creating zero-copy optimized detector" << std::endl;
}

YOLOv8DetectorZeroCopy::~YOLOv8DetectorZeroCopy() {
#ifdef HAVE_RKNN
    cleanupZeroCopy();
#endif
    std::cout << "[YOLOv8DetectorZeroCopy] Zero-copy detector destroyed" << std::endl;
    g_perfStats.print();
}

bool YOLOv8DetectorZeroCopy::initialize(const std::string& modelPath, InferenceBackend backend) {
    std::cout << "[YOLOv8DetectorZeroCopy] Initializing zero-copy detector..." << std::endl;
    
    // 首先调用基类初始化
    if (!YOLOv8Detector::initialize(modelPath, backend)) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Base initialization failed" << std::endl;
        return false;
    }
    
    // 只对RKNN后端启用zero-copy优化
    if (backend == InferenceBackend::RKNN) {
#ifdef HAVE_RKNN
        if (!initializeZeroCopy()) {
            std::cerr << "[YOLOv8DetectorZeroCopy] Zero-copy initialization failed" << std::endl;
            return false;
        }
        std::cout << "[YOLOv8DetectorZeroCopy] Zero-copy mode enabled successfully" << std::endl;
#else
        std::cout << "[YOLOv8DetectorZeroCopy] RKNN not available, using standard mode" << std::endl;
#endif
    }
    
    return true;
}

#ifdef HAVE_RKNN
bool YOLOv8DetectorZeroCopy::initializeZeroCopy() {
    if (m_rknnContext == 0) {
        std::cerr << "[YOLOv8DetectorZeroCopy] RKNN context not initialized" << std::endl;
        return false;
    }
    
    // 查询输入输出数量
    int ret = rknn_query(m_rknnContext, RKNN_QUERY_IN_OUT_NUM, &m_ioNum, sizeof(m_ioNum));
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to query I/O number: " << ret << std::endl;
        return false;
    }
    
    std::cout << "[YOLOv8DetectorZeroCopy] Model has " << m_ioNum.n_input 
              << " inputs, " << m_ioNum.n_output << " outputs" << std::endl;
    
    // 分配输入输出属性数组
    m_inputAttrs = new rknn_tensor_attr[m_ioNum.n_input];
    m_outputAttrs = new rknn_tensor_attr[m_ioNum.n_output];
    
    // 查询输入属性
    for (uint32_t i = 0; i < m_ioNum.n_input; i++) {
        m_inputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_INPUT_ATTR, &m_inputAttrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            std::cerr << "[YOLOv8DetectorZeroCopy] Failed to query input attr " << i << std::endl;
            return false;
        }
    }
    
    // 查询输出属性
    for (uint32_t i = 0; i < m_ioNum.n_output; i++) {
        m_outputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_OUTPUT_ATTR, &m_outputAttrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            std::cerr << "[YOLOv8DetectorZeroCopy] Failed to query output attr " << i << std::endl;
            return false;
        }
    }
    
    // 创建输入tensor内存（DMA缓冲区）
    m_inputMem = rknn_create_mem(m_rknnContext, m_inputAttrs[0].size);
    if (m_inputMem == nullptr) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to create input memory" << std::endl;
        return false;
    }
    
    // 创建输出tensor内存（DMA缓冲区）
    m_outputMem = rknn_create_mem(m_rknnContext, m_outputAttrs[0].size);
    if (m_outputMem == nullptr) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to create output memory" << std::endl;
        return false;
    }
    
    std::cout << "[YOLOv8DetectorZeroCopy] Created DMA buffers - Input: " 
              << m_inputAttrs[0].size << " bytes, Output: " 
              << m_outputAttrs[0].size << " bytes" << std::endl;
    
    m_zeroCopyEnabled = true;
    return true;
}

void YOLOv8DetectorZeroCopy::cleanupZeroCopy() {
    if (m_inputMem) {
        rknn_destroy_mem(m_rknnContext, m_inputMem);
        m_inputMem = nullptr;
    }
    
    if (m_outputMem) {
        rknn_destroy_mem(m_rknnContext, m_outputMem);
        m_outputMem = nullptr;
    }
    
    delete[] m_inputAttrs;
    delete[] m_outputAttrs;
    m_inputAttrs = nullptr;
    m_outputAttrs = nullptr;
    
    m_zeroCopyEnabled = false;
}

bool YOLOv8DetectorZeroCopy::preprocessToBuffer(const cv::Mat& frame, void* buffer) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // 直接在DMA缓冲区中进行预处理，避免额外的内存拷贝
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(m_inputWidth, m_inputHeight));
    
    // 转换为RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    
    // 根据模型输入类型进行转换
    if (m_inputAttrs[0].type == RKNN_TENSOR_UINT8) {
        // 直接拷贝到缓冲区（无需归一化）
        memcpy(buffer, rgb.data, rgb.total() * rgb.elemSize());
    } else if (m_inputAttrs[0].type == RKNN_TENSOR_FLOAT32) {
        // 归一化并转换为float32
        float* float_buffer = static_cast<float*>(buffer);
        uint8_t* src = rgb.data;
        size_t total_pixels = rgb.total() * rgb.channels();
        
        for (size_t i = 0; i < total_pixels; i++) {
            float_buffer[i] = static_cast<float>(src[i]) / 255.0f;
        }
    } else {
        std::cerr << "[YOLOv8DetectorZeroCopy] Unsupported input type" << std::endl;
        return false;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double preprocess_time = std::chrono::duration<double, std::milli>(end - start).count();
    
    return true;
}

std::vector<YOLOv8DetectorZeroCopy::Detection> YOLOv8DetectorZeroCopy::inferenceZeroCopy(const cv::Mat& frame) {
    std::vector<Detection> detections;
    
    auto total_start = std::chrono::high_resolution_clock::now();
    
    // 1. 预处理 - 直接写入DMA缓冲区
    auto preprocess_start = std::chrono::high_resolution_clock::now();
    if (!preprocessToBuffer(frame, m_inputMem->virt_addr)) {
        return detections;
    }
    auto preprocess_end = std::chrono::high_resolution_clock::now();
    double preprocess_time = std::chrono::duration<double, std::milli>(preprocess_end - preprocess_start).count();
    
    // 2. 设置输入（zero-copy模式）
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = m_inputAttrs[0].type;
    inputs[0].fmt = m_inputAttrs[0].fmt;
    inputs[0].size = m_inputAttrs[0].size;
    inputs[0].buf = m_inputMem->virt_addr;
    
    int ret = rknn_inputs_set(m_rknnContext, 1, inputs);
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to set inputs: " << ret << std::endl;
        return detections;
    }
    
    // 3. 推理
    auto inference_start = std::chrono::high_resolution_clock::now();
    ret = rknn_run(m_rknnContext, nullptr);
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to run inference: " << ret << std::endl;
        return detections;
    }
    auto inference_end = std::chrono::high_resolution_clock::now();
    double inference_time = std::chrono::duration<double, std::milli>(inference_end - inference_start).count();
    
    // 4. 获取输出（zero-copy模式）
    rknn_output outputs[1];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].index = 0;
    outputs[0].want_float = 0;  // 获取原始输出
    outputs[0].is_prealloc = 1; // 使用预分配的缓冲区
    outputs[0].buf = m_outputMem->virt_addr;
    outputs[0].size = m_outputAttrs[0].size;
    
    ret = rknn_outputs_get(m_rknnContext, 1, outputs, nullptr);
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorZeroCopy] Failed to get outputs: " << ret << std::endl;
        return detections;
    }
    
    // 5. 后处理 - 直接从DMA缓冲区读取
    auto postprocess_start = std::chrono::high_resolution_clock::now();
    detections = postprocessFromBuffer(m_outputMem->virt_addr, frame.size());
    auto postprocess_end = std::chrono::high_resolution_clock::now();
    double postprocess_time = std::chrono::duration<double, std::milli>(postprocess_end - postprocess_start).count();
    
    // 释放输出（zero-copy模式下不需要释放）
    // rknn_outputs_release(m_rknnContext, 1, outputs);
    
    auto total_end = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double, std::milli>(total_end - total_start).count();
    
    // 更新性能统计
    g_perfStats.update(preprocess_time, inference_time, postprocess_time);
    
    std::cout << "[ZeroCopy] Frame processed in " << total_time << "ms "
              << "(prep: " << preprocess_time << "ms, inf: " << inference_time 
              << "ms, post: " << postprocess_time << "ms)" << std::endl;
    
    return detections;
}

std::vector<YOLOv8DetectorZeroCopy::Detection> YOLOv8DetectorZeroCopy::postprocessFromBuffer(void* buffer, const cv::Size& originalSize) {
    // 使用基类的官方后处理算法，但直接从DMA缓冲区读取
    rknn_output outputs[1];
    outputs[0].buf = buffer;
    outputs[0].size = m_outputAttrs[0].size;
    
    return YOLOv8Detector::postprocessRKNNResultsOfficial(outputs, m_outputAttrs, 1, originalSize);
}
#endif

std::vector<YOLOv8DetectorZeroCopy::Detection> YOLOv8DetectorZeroCopy::detectObjects(const cv::Mat& frame) {
#ifdef HAVE_RKNN
    if (m_backend == InferenceBackend::RKNN && m_zeroCopyEnabled) {
        return inferenceZeroCopy(frame);
    }
#endif
    
    // 回退到基类实现
    return YOLOv8Detector::detectObjects(frame);
}
