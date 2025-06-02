/**
 * @file YOLOv8RKNNDetector.cpp
 * @brief YOLOv8 RKNN NPU Implementation
 *
 * This file implements YOLOv8 object detection using RKNN NPU acceleration.
 * Based on the reference implementation from /userdata/source/source/yolov8_rknn
 */

#include "YOLOv8RKNNDetector.h"
#include "../core/Logger.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <set>
#include <map>

using namespace AISecurityVision;

#ifndef HAVE_RKNN
// Fallback implementations when RKNN is not available
const char* get_type_string(int type) { return "unknown"; }
const char* get_format_string(int fmt) { return "unknown"; }
#else
// RKNN helper functions
const char* get_type_string(int type) {
    switch (type) {
        case RKNN_TENSOR_FLOAT32: return "FP32";
        case RKNN_TENSOR_FLOAT16: return "FP16";
        case RKNN_TENSOR_INT8: return "INT8";
        case RKNN_TENSOR_UINT8: return "UINT8";
        case RKNN_TENSOR_INT16: return "INT16";
        case RKNN_TENSOR_UINT16: return "UINT16";
        case RKNN_TENSOR_INT32: return "INT32";
        case RKNN_TENSOR_UINT32: return "UINT32";
        case RKNN_TENSOR_INT64: return "INT64";
        default: return "UNKNOWN";
    }
}

const char* get_format_string(int fmt) {
    switch (fmt) {
        case RKNN_TENSOR_NCHW: return "NCHW";
        case RKNN_TENSOR_NHWC: return "NHWC";
        case RKNN_TENSOR_NC1HWC2: return "NC1HWC2";
        default: return "UNKNOWN";
    }
}
#endif

// Constants matching reference implementation
#define OBJ_CLASS_NUM 80
#define OBJ_NUMB_MAX_SIZE 128

namespace AISecurityVision {

// Static utility functions (matching reference implementation exactly)
float YOLOv8RKNNDetector::calculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                                           float xmin1, float ymin1, float xmax1, float ymax1) {
    float w = std::max(0.0f, std::min(xmax0, xmax1) - std::max(xmin0, xmin1) + 1.0f);
    float h = std::max(0.0f, std::min(ymax0, ymax1) - std::max(ymin0, ymin1) + 1.0f);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0f) * (ymax0 - ymin0 + 1.0f) +
              (xmax1 - xmin1 + 1.0f) * (ymax1 - ymin1 + 1.0f) - i;
    return u <= 0.0f ? 0.0f : (i / u);
}

int YOLOv8RKNNDetector::nms(int validCount, std::vector<float>& outputLocations,
                           std::vector<int>& classIds, std::vector<int>& order,
                           int filterId, float threshold) {
    for (int i = 0; i < validCount; ++i) {
        int n = order[i];
        if (n == -1 || classIds[n] != filterId) {
            continue;
        }
        for (int j = i + 1; j < validCount; ++j) {
            int m = order[j];
            if (m == -1 || classIds[m] != filterId) {
                continue;
            }
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];

            float xmin1 = outputLocations[m * 4 + 0];
            float ymin1 = outputLocations[m * 4 + 1];
            float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
            float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];

            float iou = calculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}

int YOLOv8RKNNDetector::quickSortIndiceInverse(std::vector<float>& input, int left, int right,
                                              std::vector<int>& indices) {
    float key;
    int key_index;
    int low = left;
    int high = right;
    if (left < right) {
        key_index = indices[left];
        key = input[left];
        while (low < high) {
            while (low < high && input[high] <= key) {
                high--;
            }
            input[low] = input[high];
            indices[low] = indices[high];
            while (low < high && input[low] >= key) {
                low++;
            }
            input[high] = input[low];
            indices[high] = indices[low];
        }
        input[low] = key;
        indices[low] = key_index;
        quickSortIndiceInverse(input, left, low - 1, indices);
        quickSortIndiceInverse(input, low + 1, right, indices);
    }
    return low;
}

void YOLOv8RKNNDetector::computeDFL(float* tensor, int dfl_len, float* box) {
    for (int b = 0; b < 4; b++) {
        float exp_t[dfl_len];
        float exp_sum = 0;
        float acc_sum = 0;

        for (int i = 0; i < dfl_len; i++) {
            exp_t[i] = exp(tensor[i + b * dfl_len]);
            exp_sum += exp_t[i];
        }

        for (int i = 0; i < dfl_len; i++) {
            acc_sum += exp_t[i] / exp_sum * i;
        }
        box[b] = acc_sum;
    }
}

float YOLOv8RKNNDetector::deqntAffineToF32(int8_t qnt, int32_t zp, float scale) {
    return ((float)qnt - (float)zp) * scale;
}

int8_t YOLOv8RKNNDetector::qntF32ToAffine(float f32, int32_t zp, float scale) {
    float dst_val = (f32 / scale) + zp;
    return (int8_t)std::max(-128.0f, std::min(127.0f, dst_val));
}

// Core processing function (matching reference implementation exactly)
int YOLOv8RKNNDetector::processI8(int8_t* box_tensor, int32_t box_zp, float box_scale,
                                  int8_t* score_tensor, int32_t score_zp, float score_scale,
                                  int8_t* score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
                                  int grid_h, int grid_w, int stride, int dfl_len,
                                  std::vector<float>& boxes, std::vector<float>& objProbs,
                                  std::vector<int>& classId, float threshold) {
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    int8_t score_thres_i8 = qntF32ToAffine(threshold, score_zp, score_scale);
    int8_t score_sum_thres_i8 = qntF32ToAffine(threshold, score_sum_zp, score_sum_scale);

    for (int i = 0; i < grid_h; i++) {
        for (int j = 0; j < grid_w; j++) {
            int offset = i * grid_w + j;
            int max_class_id = -1;

            // 通过 score sum 起到快速过滤的作用
            if (score_sum_tensor != nullptr) {
                if (score_sum_tensor[offset] < score_sum_thres_i8) {
                    continue;
                }
            }

            int8_t max_score = -score_zp;
            for (int c = 0; c < OBJ_CLASS_NUM; c++) {
                if ((score_tensor[offset] > score_thres_i8) && (score_tensor[offset] > max_score)) {
                    max_score = score_tensor[offset];
                    max_class_id = c;
                }
                offset += grid_len;
            }

            // compute box
            if (max_score > score_thres_i8) {
                offset = i * grid_w + j;
                float box[4];
                float before_dfl[dfl_len * 4];
                for (int k = 0; k < dfl_len * 4; k++) {
                    before_dfl[k] = deqntAffineToF32(box_tensor[offset], box_zp, box_scale);
                    offset += grid_len;
                }
                computeDFL(before_dfl, dfl_len, box);

                float x1, y1, x2, y2, w, h;
                x1 = (-box[0] + j + 0.5) * stride;
                y1 = (-box[1] + i + 0.5) * stride;
                x2 = (box[2] + j + 0.5) * stride;
                y2 = (box[3] + i + 0.5) * stride;
                w = x2 - x1;
                h = y2 - y1;
                boxes.push_back(x1);
                boxes.push_back(y1);
                boxes.push_back(w);
                boxes.push_back(h);

                objProbs.push_back(deqntAffineToF32(max_score, score_zp, score_scale));
                classId.push_back(max_class_id);
                validCount++;
            }
        }
    }
    return validCount;
}

// Constructor and destructor
YOLOv8RKNNDetector::YOLOv8RKNNDetector()
    : YOLOv8Detector()
#ifdef HAVE_RKNN
    , m_rknnContext(0)
    , m_inputAttrs(nullptr)
    , m_outputAttrs(nullptr)
    , m_isQuantized(false)
    , m_multiCoreEnabled(false)
    , m_zeroCopyMode(false)
#endif
{
    m_backend = InferenceBackend::RKNN;
    LOG_INFO() << "[YOLOv8RKNNDetector] RKNN detector created";
}

YOLOv8RKNNDetector::~YOLOv8RKNNDetector() {
    cleanup();
}

bool YOLOv8RKNNDetector::initialize(const std::string& modelPath) {
    LOG_INFO() << "[YOLOv8RKNNDetector] Initializing RKNN YOLOv8 detector...";
    LOG_INFO() << "[YOLOv8RKNNDetector] Model path: " << modelPath;

#ifdef HAVE_RKNN
    // Check if model file exists and has .rknn extension
    std::ifstream modelFile(modelPath);
    if (!modelFile.good()) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] RKNN model file not found: " << modelPath;
        return false;
    }

    // Check file extension
    if (modelPath.substr(modelPath.find_last_of(".") + 1) != "rknn") {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Model file must have .rknn extension for RKNN backend";
        return false;
    }

    // Read model file
    modelFile.seekg(0, std::ios::end);
    size_t modelSize = modelFile.tellg();
    modelFile.seekg(0, std::ios::beg);

    std::vector<char> modelData(modelSize);
    modelFile.read(modelData.data(), modelSize);
    modelFile.close();

    // Initialize RKNN context
    int ret = rknn_init(&m_rknnContext, modelData.data(), modelSize, 0, nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to initialize RKNN context: " << ret;
        return false;
    }

    // Enable multi-core NPU for better performance (RK3588 has 3 NPU cores)
    if (enableMultiCore(true)) {
        LOG_INFO() << "[YOLOv8RKNNDetector] Successfully enabled 3-core NPU acceleration";
    } else {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Warning: Failed to set multi-core NPU, using default core";
    }

    // Query model input/output info
    ret = rknn_query(m_rknnContext, RKNN_QUERY_IN_OUT_NUM, &m_ioNum, sizeof(m_ioNum));
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to query RKNN input/output number: " << ret;
        rknn_destroy(m_rknnContext);
        return false;
    }

    LOG_INFO() << "[YOLOv8RKNNDetector] RKNN model inputs: " << m_ioNum.n_input << ", outputs: " << m_ioNum.n_output;

    // Print detailed model information like reference implementation
    std::cout << "model input num: " << m_ioNum.n_input << ", output num: " << m_ioNum.n_output << std::endl;

    // Allocate and query input attributes
    m_inputAttrs = new rknn_tensor_attr[m_ioNum.n_input];
    memset(m_inputAttrs, 0, sizeof(rknn_tensor_attr) * m_ioNum.n_input);
    for (uint32_t i = 0; i < m_ioNum.n_input; i++) {
        m_inputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_INPUT_ATTR, &(m_inputAttrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to query input attr " << i << ": " << ret;
            cleanup();
            return false;
        }
    }

    // Print input tensors information like reference implementation
    std::cout << "input tensors:" << std::endl;

    // Update input dimensions from model
    if (m_ioNum.n_input > 0) {
        // Print detailed input tensor info
        std::cout << "  index=0, name=" << m_inputAttrs[0].name
                  << ", n_dims=" << m_inputAttrs[0].n_dims << ", dims=[";
        for (uint32_t i = 0; i < m_inputAttrs[0].n_dims; i++) {
            std::cout << m_inputAttrs[0].dims[i];
            if (i < m_inputAttrs[0].n_dims - 1) std::cout << ", ";
        }
        std::cout << "], n_elems=" << m_inputAttrs[0].n_elems
                  << ", size=" << m_inputAttrs[0].size
                  << ", fmt=" << (m_inputAttrs[0].fmt == RKNN_TENSOR_NHWC ? "NHWC" : "NCHW")
                  << ", type=" << get_type_string(m_inputAttrs[0].type)
                  << ", qnt_type=AFFINE, zp=" << m_inputAttrs[0].zp
                  << ", scale=" << std::fixed << std::setprecision(6) << m_inputAttrs[0].scale << std::endl;

        // RKNN input format is typically NHWC: [batch, height, width, channels]
        if (m_inputAttrs[0].n_dims == 4) {
            m_inputHeight = m_inputAttrs[0].dims[1];  // H
            m_inputWidth = m_inputAttrs[0].dims[2];   // W
            int channels = m_inputAttrs[0].dims[3];   // C
            LOG_INFO() << "[YOLOv8RKNNDetector] RKNN model input size: " << m_inputWidth << "x" << m_inputHeight << "x" << channels;
            LOG_INFO() << "[YOLOv8RKNNDetector] Input format: " << (m_inputAttrs[0].fmt == RKNN_TENSOR_NHWC ? "NHWC" : "NCHW");
            LOG_INFO() << "[YOLOv8RKNNDetector] Input type: " << get_type_string(m_inputAttrs[0].type);
        }
    }

    // Allocate and query output attributes
    m_outputAttrs = new rknn_tensor_attr[m_ioNum.n_output];
    std::cout << "output tensors:" << std::endl;
    for (uint32_t i = 0; i < m_ioNum.n_output; i++) {
        m_outputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_OUTPUT_ATTR, &(m_outputAttrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to query output attr " << i << ": " << ret;
            cleanup();
            return false;
        }

        // Print detailed output tensor info
        std::cout << "  index=" << i << ", name=" << m_outputAttrs[i].name
                  << ", n_dims=" << m_outputAttrs[i].n_dims << ", dims=[";
        for (uint32_t j = 0; j < m_outputAttrs[i].n_dims; j++) {
            std::cout << m_outputAttrs[i].dims[j];
            if (j < m_outputAttrs[i].n_dims - 1) std::cout << ", ";
        }
        std::cout << "], n_elems=" << m_outputAttrs[i].n_elems
                  << ", size=" << m_outputAttrs[i].size
                  << ", fmt=" << (m_outputAttrs[i].fmt == RKNN_TENSOR_NHWC ? "NHWC" : "NCHW")
                  << ", type=" << get_type_string(m_outputAttrs[i].type)
                  << ", qnt_type=AFFINE, zp=" << m_outputAttrs[i].zp
                  << ", scale=" << std::fixed << std::setprecision(6) << m_outputAttrs[i].scale << std::endl;
    }

    // Check if model is quantized
    if (m_outputAttrs[0].type == RKNN_TENSOR_INT8) {
        m_isQuantized = true;
    } else {
        m_isQuantized = false;
    }

    m_initialized = true;
    LOG_INFO() << "[YOLOv8RKNNDetector] RKNN backend initialized successfully";
    LOG_INFO() << "[YOLOv8RKNNDetector] Input size: " << m_inputWidth << "x" << m_inputHeight;
    LOG_INFO() << "[YOLOv8RKNNDetector] Classes: " << m_numClasses;
    LOG_INFO() << "[YOLOv8RKNNDetector] Confidence threshold: " << m_confidenceThreshold;
    LOG_INFO() << "[YOLOv8RKNNDetector] NMS threshold: " << m_nmsThreshold;
    LOG_INFO() << "[YOLOv8RKNNDetector] Model is quantized: " << (m_isQuantized ? "yes" : "no");
    return true;
#else
    LOG_ERROR() << "[YOLOv8RKNNDetector] RKNN support not compiled in";
    return false;
#endif
}

bool YOLOv8RKNNDetector::enableMultiCore(bool enable) {
#ifdef HAVE_RKNN
    if (!m_rknnContext) {
        return false;
    }

    rknn_core_mask core_mask = enable ? RKNN_NPU_CORE_0_1_2 : RKNN_NPU_CORE_AUTO;
    int ret = rknn_set_core_mask(m_rknnContext, core_mask);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to set NPU core mask: " << ret;
        return false;
    }

    m_multiCoreEnabled = enable;
    return true;
#else
    return false;
#endif
}

void YOLOv8RKNNDetector::setZeroCopyMode(bool enable) {
    m_zeroCopyMode = enable;
}

bool YOLOv8RKNNDetector::isInitialized() const {
    return m_initialized;
}

InferenceBackend YOLOv8RKNNDetector::getCurrentBackend() const {
    return InferenceBackend::RKNN;
}

std::string YOLOv8RKNNDetector::getBackendName() const {
    return "RKNN";
}

std::vector<std::string> YOLOv8RKNNDetector::getModelInfo() const {
    std::vector<std::string> info;
    info.push_back("Backend: RKNN NPU");
    info.push_back("Input size: " + std::to_string(m_inputWidth) + "x" + std::to_string(m_inputHeight));
    info.push_back("Classes: " + std::to_string(m_numClasses));
    info.push_back("Confidence threshold: " + std::to_string(m_confidenceThreshold));
    info.push_back("NMS threshold: " + std::to_string(m_nmsThreshold));
#ifdef HAVE_RKNN
    info.push_back("Multi-core enabled: " + std::string(m_multiCoreEnabled ? "yes" : "no"));
    info.push_back("Zero-copy mode: " + std::string(m_zeroCopyMode ? "yes" : "no"));
    info.push_back("Model quantized: " + std::string(m_isQuantized ? "yes" : "no"));
#endif
    return info;
}

cv::Mat YOLOv8RKNNDetector::preprocessImageWithLetterbox(const cv::Mat& image, LetterboxInfo& letterbox) {
    // Calculate letterbox parameters to maintain aspect ratio
    float scale_x = static_cast<float>(m_inputWidth) / image.cols;
    float scale_y = static_cast<float>(m_inputHeight) / image.rows;
    letterbox.scale = std::min(scale_x, scale_y);

    int new_width = static_cast<int>(image.cols * letterbox.scale);
    int new_height = static_cast<int>(image.rows * letterbox.scale);

    letterbox.x_pad = (m_inputWidth - new_width) / 2.0f;
    letterbox.y_pad = (m_inputHeight - new_height) / 2.0f;

    // Resize image
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_width, new_height));

    // Create letterboxed image with padding
    cv::Mat letterboxed = cv::Mat::zeros(m_inputHeight, m_inputWidth, CV_8UC3);
    resized.copyTo(letterboxed(cv::Rect(static_cast<int>(letterbox.x_pad),
                                       static_cast<int>(letterbox.y_pad),
                                       new_width, new_height)));

    return letterboxed;
}

std::vector<Detection> YOLOv8RKNNDetector::detectObjects(const cv::Mat& frame) {
    std::vector<Detection> detections;

    if (!m_initialized) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Detector not initialized";
        return detections;
    }

#ifdef HAVE_RKNN
    auto start_time = std::chrono::high_resolution_clock::now();

    // Preprocess image with letterbox
    LetterboxInfo letterbox;
    cv::Mat preprocessed = preprocessImageWithLetterbox(frame, letterbox);

    // Prepare input
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = preprocessed.total() * preprocessed.elemSize();
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].buf = preprocessed.data;

    // Set input
    int ret = rknn_inputs_set(m_rknnContext, m_ioNum.n_input, inputs);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to set inputs: " << ret;
        return detections;
    }

    // Run inference
    std::cout << "rknn_run" << std::endl;
    auto inference_start = std::chrono::high_resolution_clock::now();
    ret = rknn_run(m_rknnContext, nullptr);
    auto inference_end = std::chrono::high_resolution_clock::now();

    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to run inference: " << ret;
        return detections;
    }

    // Calculate and print inference time like reference implementation
    double inference_time = std::chrono::duration<double, std::milli>(inference_end - inference_start).count();
    std::cout << "rknn_run time=" << std::fixed << std::setprecision(2) << inference_time
              << "ms, FPS = " << std::setprecision(2) << (1000.0 / inference_time) << std::endl;

    // Get outputs
    rknn_output outputs[m_ioNum.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (uint32_t i = 0; i < m_ioNum.n_output; i++) {
        outputs[i].want_float = 0;  // Keep quantized format for better performance
    }

    ret = rknn_outputs_get(m_rknnContext, m_ioNum.n_output, outputs, nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8RKNNDetector] Failed to get outputs: " << ret;
        return detections;
    }

    // Post-process results
    auto postprocess_start = std::chrono::high_resolution_clock::now();
    detections = postprocessResults(outputs, m_outputAttrs, m_ioNum.n_output, frame.size(), letterbox);
    auto postprocess_end = std::chrono::high_resolution_clock::now();

    double postprocess_time = std::chrono::duration<double, std::milli>(postprocess_end - postprocess_start).count();
    std::cout << "post_process time=" << std::fixed << std::setprecision(2) << postprocess_time
              << "ms, FPS = " << std::setprecision(2) << (1000.0 / postprocess_time) << std::endl;

    // Release outputs
    rknn_outputs_release(m_rknnContext, m_ioNum.n_output, outputs);

    // Update performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    m_inferenceTimes.push_back(m_inferenceTime);
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }
    m_detectionCount += detections.size();

#endif

    return detections;
}

std::vector<Detection> YOLOv8RKNNDetector::postprocessResults(rknn_output* outputs,
                                                            rknn_tensor_attr* output_attrs,
                                                            uint32_t n_output,
                                                            const cv::Size& originalSize,
                                                            const LetterboxInfo& letterbox) {
    std::vector<Detection> detections;

#ifdef HAVE_RKNN
    // YOLOv8 has 9 outputs: 3 scales × 3 tensors (box, score, score_sum)
    // Scale 1: 80×80, Scale 2: 40×40, Scale 3: 20×20
    // Each scale has: box tensor (64 channels), score tensor (80 channels), score_sum tensor (1 channel)

    std::vector<float> boxes;
    std::vector<float> objProbs;
    std::vector<int> classId;

    // Process each scale (matching reference implementation exactly)
    for (int scale_idx = 0; scale_idx < 3; scale_idx++) {
        int box_idx = scale_idx * 3;      // 0, 3, 6
        int score_idx = scale_idx * 3 + 1; // 1, 4, 7
        int score_sum_idx = scale_idx * 3 + 2; // 2, 5, 8

        if (box_idx >= n_output || score_idx >= n_output || score_sum_idx >= n_output) {
            LOG_ERROR() << "[YOLOv8RKNNDetector] Invalid output tensor indices";
            continue;
        }

        // Get tensor attributes
        rknn_tensor_attr& box_attr = output_attrs[box_idx];
        rknn_tensor_attr& score_attr = output_attrs[score_idx];
        rknn_tensor_attr& score_sum_attr = output_attrs[score_sum_idx];

        // Get tensor data
        int8_t* box_tensor = (int8_t*)outputs[box_idx].buf;
        int8_t* score_tensor = (int8_t*)outputs[score_idx].buf;
        int8_t* score_sum_tensor = (int8_t*)outputs[score_sum_idx].buf;

        // Calculate grid dimensions and stride
        int grid_h = box_attr.dims[2];  // Height
        int grid_w = box_attr.dims[3];  // Width
        int stride = 640 / grid_h;      // 8, 16, 32 for the three scales
        int dfl_len = box_attr.dims[1] / 4;  // DFL length (typically 16)

        LOG_DEBUG() << "[YOLOv8RKNNDetector] Processing scale " << scale_idx
                   << ": grid=" << grid_h << "x" << grid_w
                   << ", stride=" << stride << ", dfl_len=" << dfl_len;

        // Process this scale using the reference implementation's process_i8 function
        int validCount = processI8(box_tensor, box_attr.zp, box_attr.scale,
                                  score_tensor, score_attr.zp, score_attr.scale,
                                  score_sum_tensor, score_sum_attr.zp, score_sum_attr.scale,
                                  grid_h, grid_w, stride, dfl_len,
                                  boxes, objProbs, classId, m_confidenceThreshold);

        LOG_DEBUG() << "[YOLOv8RKNNDetector] Scale " << scale_idx << " produced " << validCount << " detections";
    }

    // Apply NMS (Non-Maximum Suppression)
    int totalDetections = boxes.size() / 4;
    if (totalDetections == 0) {
        return detections;
    }

    LOG_DEBUG() << "[YOLOv8RKNNDetector] Total detections before NMS: " << totalDetections;

    // Prepare data for NMS
    std::vector<int> order(totalDetections);
    std::iota(order.begin(), order.end(), 0);

    // Sort by confidence (descending)
    std::vector<float> confidences = objProbs;
    quickSortIndiceInverse(confidences, 0, totalDetections - 1, order);

    // Apply NMS for each class
    std::set<int> uniqueClasses(classId.begin(), classId.end());
    for (int cls : uniqueClasses) {
        nms(totalDetections, boxes, classId, order, cls, m_nmsThreshold);
    }

    // Convert results to Detection objects and transform coordinates back to original image
    for (int i = 0; i < totalDetections; i++) {
        if (order[i] == -1) continue;  // Filtered out by NMS

        int idx = order[i];
        float x = boxes[idx * 4];
        float y = boxes[idx * 4 + 1];
        float w = boxes[idx * 4 + 2];
        float h = boxes[idx * 4 + 3];

        // Transform coordinates back to original image space
        // Remove letterbox padding and scale back
        x = (x - letterbox.x_pad) / letterbox.scale;
        y = (y - letterbox.y_pad) / letterbox.scale;
        w = w / letterbox.scale;
        h = h / letterbox.scale;

        // Clamp to image boundaries
        x = std::max(0.0f, std::min(x, static_cast<float>(originalSize.width)));
        y = std::max(0.0f, std::min(y, static_cast<float>(originalSize.height)));
        w = std::max(0.0f, std::min(w, static_cast<float>(originalSize.width) - x));
        h = std::max(0.0f, std::min(h, static_cast<float>(originalSize.height) - y));

        // Create detection
        Detection detection;
        detection.bbox = cv::Rect(static_cast<int>(x), static_cast<int>(y),
                                 static_cast<int>(w), static_cast<int>(h));
        detection.confidence = objProbs[idx];
        detection.classId = classId[idx];

        // Set class name
        if (detection.classId >= 0 && detection.classId < static_cast<int>(m_classNames.size())) {
            detection.className = m_classNames[detection.classId];
        } else {
            detection.className = "unknown";
        }

        detections.push_back(detection);

        // Print detection like reference implementation
        std::cout << detection.className << " @ ("
                  << detection.bbox.x << " " << detection.bbox.y << " "
                  << (detection.bbox.x + detection.bbox.width) << " "
                  << (detection.bbox.y + detection.bbox.height) << ") "
                  << std::fixed << std::setprecision(3) << detection.confidence << std::endl;
    }

    // Print class summary like our test program
    std::map<std::string, int> classCounts;
    for (const auto& det : detections) {
        classCounts[det.className]++;
    }

    std::cout << std::endl << "Class summary:" << std::endl;
    for (const auto& pair : classCounts) {
        std::cout << "  " << pair.first << ": " << pair.second << std::endl;
    }

#endif

    // Apply category filtering before returning results
    std::vector<Detection> filteredDetections = filterDetectionsByCategory(detections);

    LOG_DEBUG() << "[YOLOv8RKNNDetector] Applied category filtering: "
                << filteredDetections.size() << "/" << detections.size() << " detections kept";

    return filteredDetections;
}

void YOLOv8RKNNDetector::cleanup() {
#ifdef HAVE_RKNN
    if (m_inputAttrs) {
        delete[] m_inputAttrs;
        m_inputAttrs = nullptr;
    }

    if (m_outputAttrs) {
        delete[] m_outputAttrs;
        m_outputAttrs = nullptr;
    }

    if (m_rknnContext) {
        rknn_destroy(m_rknnContext);
        m_rknnContext = 0;
    }
#endif

    m_initialized = false;
    LOG_INFO() << "[YOLOv8RKNNDetector] Cleanup completed";
}

} // namespace AISecurityVision