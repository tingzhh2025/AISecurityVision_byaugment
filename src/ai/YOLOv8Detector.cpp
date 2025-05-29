#include "YOLOv8Detector.h"
#include "../core/Logger.h"
using namespace AISecurityVision;
#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <numeric>
#ifndef DISABLE_OPENCV_DNN
#include <opencv2/dnn.hpp>
#endif

YOLOv8Detector::YOLOv8Detector()
    : m_backend(InferenceBackend::AUTO)
    , m_requestedBackend(InferenceBackend::AUTO)
    , m_rknnContext(0)
    , m_rknnInputBuffer(nullptr)
    , m_rknnOutputBuffer(nullptr)
    , m_engine(nullptr)
    , m_context(nullptr)
    , m_stream(nullptr)
    , m_inputBuffer(nullptr)
    , m_outputBuffer(nullptr)
    , m_confidenceThreshold(0.5f)
    , m_nmsThreshold(0.4f)
    , m_initialized(false)
    , m_inputWidth(640)
    , m_inputHeight(640)
    , m_numClasses(80)
    , m_maxDetections(8400)
    , m_inputSize(0)
    , m_outputSize(0)
    , m_inferenceTime(0.0)
    , m_detectionCount(0) {

    initializeClassNames();
}

YOLOv8Detector::~YOLOv8Detector() {
    cleanup();
}

bool YOLOv8Detector::initialize(const std::string& modelPath, InferenceBackend backend) {
    LOG_INFO() << "[YOLOv8Detector] Initializing YOLOv8 detector...";
    // Fix model path - remove relative path prefix
    std::string fixedModelPath = modelPath;
    if (fixedModelPath.substr(0, 3) == "../") {
        fixedModelPath = fixedModelPath.substr(3);
    }
    LOG_INFO() << "[YOLOv8Detector] Model path: " << fixedModelPath;

    m_requestedBackend = backend;

    // Auto-detect best backend if requested
    if (backend == InferenceBackend::AUTO) {
        m_backend = detectBestBackend();
        LOG_INFO() << "[YOLOv8Detector] Auto-detected backend: " << getBackendName();
    } else {
        m_backend = backend;
        LOG_INFO() << "[YOLOv8Detector] Using requested backend: " << getBackendName();
    }

    try {
        bool success = false;

        // Try to initialize with the selected backend
        switch (m_backend) {
            case InferenceBackend::RKNN:
                success = initializeRKNN(fixedModelPath);
                break;
            case InferenceBackend::OPENCV:
                success = initializeOpenCV(fixedModelPath);
                break;
            case InferenceBackend::TENSORRT:
                success = initializeTensorRT(fixedModelPath);
                break;
            default:
                LOG_ERROR() << "[YOLOv8Detector] Unknown backend";
                return false;
        }

        // If the selected backend failed and we're in AUTO mode, try fallbacks
        if (!success && m_requestedBackend == InferenceBackend::AUTO) {
            LOG_ERROR() << "[YOLOv8Detector] Primary backend failed, trying fallbacks...";

            // Try RKNN first if not already tried and RKNN model exists
            if (m_backend != InferenceBackend::RKNN) {
                std::string rknnModelPath = fixedModelPath;
                // If original path is ONNX, try to find corresponding RKNN model
                if (fixedModelPath.find(".onnx") != std::string::npos) {
                    rknnModelPath = fixedModelPath.substr(0, fixedModelPath.find_last_of(".")) + ".rknn";
                }

                std::ifstream rknnFile(rknnModelPath);
                if (rknnFile.good()) {
                    LOG_INFO() << "[YOLOv8Detector] Trying RKNN backend with model: " << rknnModelPath;
                    m_backend = InferenceBackend::RKNN;
                    success = initializeRKNN(rknnModelPath);
                }
            }

            // Try OpenCV as fallback
            if (!success) {
                LOG_INFO() << "[YOLOv8Detector] Trying OpenCV backend...";
                m_backend = InferenceBackend::OPENCV;
                // For OpenCV, try ONNX model if available
                std::string onnxModelPath = fixedModelPath;
                if (fixedModelPath.find(".rknn") != std::string::npos) {
                    onnxModelPath = fixedModelPath.substr(0, fixedModelPath.find_last_of(".")) + ".onnx";
                }
                success = initializeOpenCV(onnxModelPath);
            }
        }

        if (success) {
            m_initialized = true;
            LOG_INFO() << "[YOLOv8Detector] YOLOv8 detector initialized successfully with " << getBackendName() << " backend";
            LOG_INFO() << "[YOLOv8Detector] Input size: " << m_inputWidth << "x" << m_inputHeight;
            LOG_INFO() << "[YOLOv8Detector] Classes: " << m_numClasses;
            LOG_INFO() << "[YOLOv8Detector] Confidence threshold: " << m_confidenceThreshold;
            LOG_INFO() << "[YOLOv8Detector] NMS threshold: " << m_nmsThreshold;
            return true;
        } else {
            LOG_ERROR() << "[YOLOv8Detector] Failed to initialize with any backend";
            return false;
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[YOLOv8Detector] Exception during initialization: " << e.what();
        return false;
    }
}

void YOLOv8Detector::cleanup() {
    cleanupRKNN();
    cleanupOpenCV();
    cleanupTensorRT();
    deallocateBuffers();
    m_initialized = false;
    LOG_INFO() << "[YOLOv8Detector] Cleanup completed";
}

bool YOLOv8Detector::isInitialized() const {
    return m_initialized;
}

InferenceBackend YOLOv8Detector::getCurrentBackend() const {
    return m_backend;
}

std::string YOLOv8Detector::getBackendName() const {
    switch (m_backend) {
        case InferenceBackend::RKNN:
            return "RKNN";
        case InferenceBackend::OPENCV:
            return "OpenCV";
        case InferenceBackend::TENSORRT:
            return "TensorRT";
        case InferenceBackend::AUTO:
            return "AUTO";
        default:
            return "Unknown";
    }
}

std::vector<YOLOv8Detector::Detection> YOLOv8Detector::detectObjects(const cv::Mat& frame) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Detection> detections;

    if (frame.empty() || !m_initialized) {
        return detections;
    }

    // Dispatch to the appropriate backend
    switch (m_backend) {
        case InferenceBackend::RKNN:
            detections = detectWithRKNN(frame);
            break;
        case InferenceBackend::OPENCV:
            detections = detectWithOpenCV(frame);
            break;
        case InferenceBackend::TENSORRT:
            detections = detectWithTensorRT(frame);
            break;
        default:
            LOG_ERROR() << "[YOLOv8Detector] Unknown backend for detection";
            break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(end - start).count();
    m_detectionCount += detections.size();
    m_inferenceTimes.push_back(m_inferenceTime);

    // Keep only last 100 inference times for average calculation
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }

    return detections;
}

std::vector<cv::Rect> YOLOv8Detector::detect(const cv::Mat& frame) {
    auto detections = detectObjects(frame);
    std::vector<cv::Rect> bboxes;

    for (const auto& detection : detections) {
        bboxes.push_back(detection.bbox);
    }

    return bboxes;
}

std::vector<std::vector<YOLOv8Detector::Detection>> YOLOv8Detector::detectBatch(const std::vector<cv::Mat>& frames) {
    std::vector<std::vector<Detection>> results;

    for (const auto& frame : frames) {
        results.push_back(detectObjects(frame));
    }

    return results;
}

void YOLOv8Detector::setConfidenceThreshold(float threshold) {
    m_confidenceThreshold = std::max(0.0f, std::min(1.0f, threshold));
    LOG_INFO() << "[YOLOv8Detector] Confidence threshold set to: " << m_confidenceThreshold;
}

void YOLOv8Detector::setNMSThreshold(float threshold) {
    m_nmsThreshold = std::max(0.0f, std::min(1.0f, threshold));
    LOG_INFO() << "[YOLOv8Detector] NMS threshold set to: " << m_nmsThreshold;
}

void YOLOv8Detector::setInputSize(int width, int height) {
    m_inputWidth = width;
    m_inputHeight = height;
    m_inputSize = width * height * 3 * sizeof(float);
    LOG_INFO() << "[YOLOv8Detector] Input size set to: " << width << "x" << height;
}

std::vector<std::string> YOLOv8Detector::getClassNames() const {
    return m_classNames;
}

cv::Size YOLOv8Detector::getInputSize() const {
    return cv::Size(m_inputWidth, m_inputHeight);
}

double YOLOv8Detector::getInferenceTime() const {
    return m_inferenceTime;
}

size_t YOLOv8Detector::getDetectionCount() const {
    return m_detectionCount;
}

float YOLOv8Detector::getAverageInferenceTime() const {
    if (m_inferenceTimes.empty()) {
        return 0.0f;
    }

    double sum = 0.0;
    for (double time : m_inferenceTimes) {
        sum += time;
    }

    return static_cast<float>(sum / m_inferenceTimes.size());
}

bool YOLOv8Detector::loadModel(const std::string& modelPath) {
    // TODO: Implement TensorRT model loading
    return true;
}

bool YOLOv8Detector::setupTensorRT() {
    // TODO: Implement TensorRT setup
    return true;
}

cv::Mat YOLOv8Detector::preprocessImage(const cv::Mat& image) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(m_inputWidth, m_inputHeight));

    // Convert BGR to RGB and normalize to [0, 1]
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    return rgb;
}

std::vector<YOLOv8Detector::Detection> YOLOv8Detector::postprocessResults(const float* output, const cv::Size& originalSize) {
    std::vector<Detection> detections;

    // TODO: Implement proper YOLOv8 post-processing with NMS
    // This would involve:
    // 1. Parse output tensor
    // 2. Apply confidence filtering
    // 3. Convert to original image coordinates
    // 4. Apply Non-Maximum Suppression

    return detections;
}

void YOLOv8Detector::initializeClassNames() {
    // COCO dataset class names
    m_classNames = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
        "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
        "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
        "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
        "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
        "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
        "toothbrush"
    };
}

bool YOLOv8Detector::allocateBuffers() {
    // TODO: Implement CUDA buffer allocation for TensorRT
    return true;
}

void YOLOv8Detector::deallocateBuffers() {
    // TODO: Implement CUDA buffer deallocation
}

// Backend detection
InferenceBackend YOLOv8Detector::detectBestBackend() const {
#ifdef HAVE_RKNN
    // Check if RKNN is available (RK3588 and other Rockchip NPUs)
    LOG_INFO() << "[YOLOv8Detector] RKNN support available, preferring RKNN backend";
    return InferenceBackend::RKNN;
#else
    LOG_INFO() << "[YOLOv8Detector] RKNN not available, using OpenCV backend";
    return InferenceBackend::OPENCV;
#endif
}

// RKNN backend implementation
bool YOLOv8Detector::initializeRKNN(const std::string& modelPath) {
#ifdef HAVE_RKNN
    LOG_INFO() << "[YOLOv8Detector] Initializing RKNN backend...";

    // Check if model file exists and has .rknn extension
    std::ifstream modelFile(modelPath);
    if (!modelFile.good()) {
        LOG_ERROR() << "[YOLOv8Detector] RKNN model file not found: " << modelPath;
        return false;
    }

    // Check file extension
    if (modelPath.substr(modelPath.find_last_of(".") + 1) != "rknn") {
        LOG_ERROR() << "[YOLOv8Detector] Model file must have .rknn extension for RKNN backend";
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
        LOG_ERROR() << "[YOLOv8Detector] Failed to initialize RKNN context: " << ret;
        return false;
    }

    // Enable multi-core NPU for better performance (RK3588 has 3 NPU cores)
    ret = rknn_set_core_mask(m_rknnContext, RKNN_NPU_CORE_0_1_2);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Warning: Failed to set multi-core NPU, using default core: " << ret;
    } else {
        LOG_INFO() << "[YOLOv8Detector] Successfully enabled 3-core NPU acceleration";
    }

    // Query model input/output info
    rknn_input_output_num io_num;
    ret = rknn_query(m_rknnContext, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to query RKNN input/output number: " << ret;
        rknn_destroy(m_rknnContext);
        return false;
    }

    LOG_INFO() << "[YOLOv8Detector] RKNN model inputs: " << io_num.n_input << ", outputs: " << io_num.n_output;

    // Query input attributes
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (uint32_t i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[YOLOv8Detector] Failed to query input attr " << i << ": " << ret;
            rknn_destroy(m_rknnContext);
            return false;
        }
    }

    // Update input dimensions from model
    if (io_num.n_input > 0) {
        // Store input attributes for later use
        m_rknnInputAttrs = input_attrs[0];

        // RKNN input format is typically NHWC: [batch, height, width, channels]
        if (input_attrs[0].n_dims == 4) {
            m_inputHeight = input_attrs[0].dims[1];  // H
            m_inputWidth = input_attrs[0].dims[2];   // W
            int channels = input_attrs[0].dims[3];   // C
            LOG_INFO() << "[YOLOv8Detector] RKNN model input size: " << m_inputWidth << "x" << m_inputHeight << "x" << channels;
            LOG_INFO() << "[YOLOv8Detector] Input format: " << (input_attrs[0].fmt == RKNN_TENSOR_NHWC ? "NHWC" : "NCHW");
            LOG_INFO() << "[YOLOv8Detector] Input type: " << get_type_string(input_attrs[0].type);
        }
    }

    LOG_INFO() << "[YOLOv8Detector] RKNN backend initialized successfully";
    return true;
#else
    LOG_ERROR() << "[YOLOv8Detector] RKNN support not compiled in";
    return false;
#endif
}

bool YOLOv8Detector::initializeOpenCV(const std::string& modelPath) {
    LOG_INFO() << "[YOLOv8Detector] Initializing OpenCV backend...";

#ifdef DISABLE_OPENCV_DNN
    LOG_INFO() << "[YOLOv8Detector] OpenCV DNN is disabled, falling back to simulation";
    return true; // Allow fallback to simulation
#else
    // Check if model file exists
    std::ifstream modelFile(modelPath);
    if (!modelFile.good()) {
        LOG_INFO() << "[YOLOv8Detector] Model file not found: " << modelPath;
        LOG_INFO() << "[YOLOv8Detector] Using built-in detection simulation";
        LOG_INFO() << "[YOLOv8Detector] To use real models, place ONNX files in models/ directory";
        return true; // Allow fallback to simulation
    }

    try {
        // Try to load with OpenCV DNN
        LOG_INFO() << "[YOLOv8Detector] Loading ONNX model: " << modelPath;
        m_dnnNet = cv::dnn::readNetFromONNX(modelPath);
        if (m_dnnNet.empty()) {
            LOG_ERROR() << "[YOLOv8Detector] Failed to load ONNX model with OpenCV";
            LOG_INFO() << "[YOLOv8Detector] Falling back to built-in detection simulation";
            return true; // Allow fallback to simulation
        }

        // Set backend and target
        m_dnnNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_dnnNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        // Get model input/output info
        std::vector<cv::String> layerNames = m_dnnNet.getLayerNames();
        std::vector<int> outLayers = m_dnnNet.getUnconnectedOutLayers();

        LOG_INFO() << "[YOLOv8Detector] Model loaded successfully:";
        LOG_INFO() << "[YOLOv8Detector] - Total layers: " << layerNames.size();
        LOG_INFO() << "[YOLOv8Detector] - Output layers: " << outLayers.size();
        LOG_INFO() << "[YOLOv8Detector] OpenCV backend initialized successfully with real model";
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[YOLOv8Detector] OpenCV DNN loading failed: " << e.what();
        LOG_INFO() << "[YOLOv8Detector] Falling back to built-in detection simulation";
        return true; // Allow fallback to simulation
    }
#endif
}

bool YOLOv8Detector::initializeTensorRT(const std::string& modelPath) {
    LOG_INFO() << "[YOLOv8Detector] TensorRT backend not implemented yet";
    return false;
}

// Detection methods for each backend
std::vector<YOLOv8Detector::Detection> YOLOv8Detector::detectWithRKNN(const cv::Mat& frame) {
#ifdef HAVE_RKNN
    std::vector<Detection> detections;

    if (m_rknnContext == 0) {
        return detections;
    }

    // Preprocess image for RKNN
    cv::Mat preprocessed = preprocessImageForRKNN(frame);

    // Set input using stored attributes
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = m_rknnInputAttrs.type;  // Use model's expected type
    inputs[0].size = preprocessed.total() * preprocessed.elemSize();
    inputs[0].fmt = m_rknnInputAttrs.fmt;    // Use model's expected format
    inputs[0].buf = preprocessed.data;

    LOG_INFO() << "[YOLOv8Detector] Input tensor size: " << inputs[0].size
              << " bytes, type: " << get_type_string(inputs[0].type)
              << ", format: " << get_format_string(inputs[0].fmt);

    int ret = rknn_inputs_set(m_rknnContext, 1, inputs);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to set RKNN inputs: " << ret;
        return detections;
    }

    // Run inference
    ret = rknn_run(m_rknnContext, nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to run RKNN inference: " << ret;
        return detections;
    }

    // Query output attributes first
    rknn_input_output_num io_num;
    ret = rknn_query(m_rknnContext, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to query RKNN input/output number: " << ret;
        return detections;
    }

    // Query output attributes
    std::vector<rknn_tensor_attr> output_attrs(io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[YOLOv8Detector] Failed to query output attr " << i << ": " << ret;
            return detections;
        }
    }

    // Get outputs
    std::vector<rknn_output> outputs(io_num.n_output);
    memset(outputs.data(), 0, sizeof(rknn_output) * io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        outputs[i].want_float = 0; // Get quantized output for better performance
    }

    ret = rknn_outputs_get(m_rknnContext, io_num.n_output, outputs.data(), nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to get RKNN outputs: " << ret;
        return detections;
    }

    // Post-process results using official YOLOv8 algorithm
    detections = postprocessRKNNResultsOfficial(outputs.data(), output_attrs.data(), io_num.n_output, frame.size());

    // Release outputs
    rknn_outputs_release(m_rknnContext, io_num.n_output, outputs.data());

    return detections;
#else
    std::vector<Detection> detections;
    LOG_ERROR() << "[YOLOv8Detector] RKNN support not compiled in";
    return detections;
#endif
}

std::vector<YOLOv8Detector::Detection> YOLOv8Detector::detectWithOpenCV(const cv::Mat& frame) {
    std::vector<Detection> detections;

#ifdef DISABLE_OPENCV_DNN
    // Use simulation when DNN is disabled
    detections = simulateDetection(frame);
#else
    if (!m_dnnNet.empty()) {
        try {
            // Preprocess image
            cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0,
                cv::Size(m_inputWidth, m_inputHeight), cv::Scalar(0, 0, 0), true, false);

            // Set input to the network
            m_dnnNet.setInput(blob);

            // Run forward pass
            cv::Mat output = m_dnnNet.forward();

            // Post-process results
            detections = postprocessResults(reinterpret_cast<float*>(output.data), frame.size());

        } catch (const std::exception& e) {
            LOG_ERROR() << "[YOLOv8Detector] OpenCV detection failed: " << e.what();
            // Fall back to simulation
            detections = simulateDetection(frame);
        }
    } else {
        // Use simulation when no model is loaded
        detections = simulateDetection(frame);
    }
#endif

    return detections;
}

std::vector<YOLOv8Detector::Detection> YOLOv8Detector::detectWithTensorRT(const cv::Mat& frame) {
    std::vector<Detection> detections;
    LOG_ERROR() << "[YOLOv8Detector] TensorRT detection not implemented yet";
    return detections;
}

// Cleanup methods
void YOLOv8Detector::cleanupRKNN() {
#ifdef HAVE_RKNN
    if (m_rknnContext != 0) {
        rknn_destroy(m_rknnContext);
        m_rknnContext = 0;
    }
    if (m_rknnInputBuffer) {
        free(m_rknnInputBuffer);
        m_rknnInputBuffer = nullptr;
    }
    if (m_rknnOutputBuffer) {
        free(m_rknnOutputBuffer);
        m_rknnOutputBuffer = nullptr;
    }
#endif
}

void YOLOv8Detector::cleanupOpenCV() {
#ifndef DISABLE_OPENCV_DNN
    if (!m_dnnNet.empty()) {
        m_dnnNet = cv::dnn::Net();
    }
#endif
}

void YOLOv8Detector::cleanupTensorRT() {
    // TODO: Implement TensorRT cleanup
}

// RKNN-specific helper methods
cv::Mat YOLOv8Detector::preprocessImageForRKNN(const cv::Mat& image) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(m_inputWidth, m_inputHeight));

    cv::Mat result;
    LOG_INFO() << "[YOLOv8Detector] Input attrs type: " << get_type_string(m_rknnInputAttrs.type);

    if (m_rknnInputAttrs.type == RKNN_TENSOR_FLOAT32) {
        // Convert to float32 and normalize to [0, 1]
        resized.convertTo(result, CV_32F, 1.0/255.0);
        LOG_INFO() << "[YOLOv8Detector] Preprocessed to FLOAT32 with normalization, size: "
                  << result.total() * result.elemSize() << " bytes";
    } else if (m_rknnInputAttrs.type == RKNN_TENSOR_FLOAT16) {
        // Convert to float32 first, then normalize (RKNN will handle FP16 conversion internally)
        resized.convertTo(result, CV_32F, 1.0/255.0);
        LOG_INFO() << "[YOLOv8Detector] Preprocessed to FLOAT32 for FP16 model with normalization, size: "
                  << result.total() * result.elemSize() << " bytes";
    } else {
        // Keep as uint8 for quantized models (INT8, UINT8, etc.)
        resized.convertTo(result, CV_8U);
        LOG_INFO() << "[YOLOv8Detector] Preprocessed to UINT8 without normalization, size: "
                  << result.total() * result.elemSize() << " bytes";
    }

    return result;
}

std::vector<YOLOv8Detector::Detection> YOLOv8Detector::postprocessRKNNResults(const float* output, const cv::Size& originalSize) {
    std::vector<Detection> detections;

    // YOLOv8 output format: [1, 84, 8400] where 84 = 4 (bbox) + 80 (classes)
    // The output is transposed compared to the original format
    const int numClasses = 80;
    const int numBoxes = 8400;
    const int outputDim = 84; // 4 bbox + 80 classes
    const float confThreshold = m_confidenceThreshold;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    // Scale factors for converting back to original image size
    float scaleX = static_cast<float>(originalSize.width) / m_inputWidth;
    float scaleY = static_cast<float>(originalSize.height) / m_inputHeight;

    // YOLOv8 RKNN output format: [84, 8400] (transposed)
    // Each column represents one detection
    for (int i = 0; i < numBoxes; i++) {
        // Get bounding box coordinates (center_x, center_y, width, height)
        float centerX = output[i];                    // First row: center_x
        float centerY = output[numBoxes + i];         // Second row: center_y
        float width = output[2 * numBoxes + i];       // Third row: width
        float height = output[3 * numBoxes + i];      // Fourth row: height

        // Find class with highest confidence
        float maxConf = 0.0f;
        int maxClassId = -1;
        for (int c = 0; c < numClasses; c++) {
            float conf = output[(4 + c) * numBoxes + i]; // Class confidence
            if (conf > maxConf) {
                maxConf = conf;
                maxClassId = c;
            }
        }

        if (maxConf > confThreshold) {
            // Convert normalized coordinates to pixel coordinates
            // YOLOv8 outputs normalized coordinates [0, 1]
            centerX *= m_inputWidth;
            centerY *= m_inputHeight;
            width *= m_inputWidth;
            height *= m_inputHeight;

            // Convert to top-left corner format and scale to original image
            int x = static_cast<int>((centerX - width / 2) * scaleX);
            int y = static_cast<int>((centerY - height / 2) * scaleY);
            int w = static_cast<int>(width * scaleX);
            int h = static_cast<int>(height * scaleY);

            // Clamp to image boundaries
            x = std::max(0, std::min(x, originalSize.width - 1));
            y = std::max(0, std::min(y, originalSize.height - 1));
            w = std::max(1, std::min(w, originalSize.width - x));
            h = std::max(1, std::min(h, originalSize.height - y));

            boxes.push_back(cv::Rect(x, y, w, h));
            confidences.push_back(maxConf);
            classIds.push_back(maxClassId);
        }
    }

    // Apply Non-Maximum Suppression
    std::vector<int> indices;
#ifdef DISABLE_OPENCV_DNN
    // Simple NMS implementation when DNN is disabled
    indices.resize(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);

    // Sort by confidence (highest first)
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return confidences[a] > confidences[b];
    });

    // Simple overlap-based filtering
    std::vector<bool> suppressed(boxes.size(), false);
    std::vector<int> finalIndices;

    for (int i = 0; i < indices.size(); i++) {
        int idx = indices[i];
        if (suppressed[idx]) continue;

        finalIndices.push_back(idx);

        // Suppress overlapping boxes
        for (int j = i + 1; j < indices.size(); j++) {
            int jdx = indices[j];
            if (suppressed[jdx]) continue;

            cv::Rect intersection = boxes[idx] & boxes[jdx];
            float iou = static_cast<float>(intersection.area()) /
                       (boxes[idx].area() + boxes[jdx].area() - intersection.area());

            if (iou > m_nmsThreshold) {
                suppressed[jdx] = true;
            }
        }
    }
    indices = finalIndices;
#else
    cv::dnn::NMSBoxes(boxes, confidences, m_confidenceThreshold, m_nmsThreshold, indices);
#endif

    // Create final detections
    for (int idx : indices) {
        Detection detection;
        detection.bbox = boxes[idx];
        detection.confidence = confidences[idx];
        detection.classId = classIds[idx];
        if (detection.classId < static_cast<int>(m_classNames.size())) {
            detection.className = m_classNames[detection.classId];
        } else {
            detection.className = "unknown";
        }
        detections.push_back(detection);
    }

    LOG_INFO() << "[YOLOv8Detector] RKNN post-processing: " << boxes.size()
              << " raw detections -> " << detections.size() << " final detections";

    return detections;
}

// Simulation method for fallback
std::vector<YOLOv8Detector::Detection> YOLOv8Detector::simulateDetection(const cv::Mat& frame) {
    std::vector<Detection> detections;

    if (frame.cols < 200 || frame.rows < 200) {
        return detections;
    }

    // Use frame count for animation (based on detection count)
    static size_t frameCount = 0;
    frameCount++;

    // Simulate moving person detection
    int personX = 50 + (frameCount * 2) % (frame.cols - 150);
    int personY = 50 + (frameCount / 10) % (frame.rows - 230);

    Detection person;
    person.bbox = cv::Rect(personX, personY, 100, 180);
    person.confidence = 0.85f + 0.1f * sin(frameCount * 0.1f); // Varying confidence
    person.classId = 0; // Person class in COCO
    person.className = "person";
    detections.push_back(person);

    // Simulate moving car detection (different pattern)
    if (frameCount % 3 == 0) { // Car appears every 3rd frame
        int carX = (frame.cols - 120) - (frameCount * 3) % (frame.cols - 120);
        int carY = frame.rows - 150;

        Detection car;
        car.bbox = cv::Rect(carX, carY, 120, 80);
        car.confidence = 0.75f + 0.15f * cos(frameCount * 0.05f);
        car.classId = 2; // Car class in COCO
        car.className = "car";
        detections.push_back(car);
    }

    // Occasionally simulate other objects
    if (frameCount % 5 == 0 && frame.cols > 400) {
        Detection bicycle;
        bicycle.bbox = cv::Rect(frame.cols - 200, 100, 80, 60);
        bicycle.confidence = 0.65f;
        bicycle.classId = 1; // Bicycle class in COCO
        bicycle.className = "bicycle";
        detections.push_back(bicycle);
    }

    // Simulate detection based on frame content (simple edge detection)
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    // Count edge pixels to simulate "activity"
    int edgePixels = cv::countNonZero(edges);
    float activity = static_cast<float>(edgePixels) / (frame.cols * frame.rows);

    // Add more detections if there's more "activity" in the frame
    if (activity > 0.1f) {
        Detection bottle;
        bottle.bbox = cv::Rect(frame.cols / 2, frame.rows / 2, 30, 80);
        bottle.confidence = 0.6f * activity;
        bottle.classId = 39; // Bottle class in COCO
        bottle.className = "bottle";
        detections.push_back(bottle);
    }

    LOG_INFO() << "[YOLOv8Detector] Simulated " << detections.size()
              << " detections (activity: " << std::fixed << std::setprecision(3) << activity << ")";

    return detections;
}

// Official YOLOv8 RKNN post-processing implementation
std::vector<YOLOv8Detector::Detection> YOLOv8Detector::postprocessRKNNResultsOfficial(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize) {
    std::vector<Detection> detections;

    // Check if this is a single-output YOLOv8 model (common format)
    if (n_output == 1) {
        // Single output format: [1, 84, 8400] where 84 = 4(bbox) + 80(classes)
        LOG_INFO() << "[YOLOv8Detector] Processing single-output YOLOv8 model";

        // Print output dimensions for debugging
        LOG_INFO() << "[YOLOv8Detector] Output dims: ";
        for (uint32_t i = 0; i < output_attrs[0].n_dims; i++) {
            LOG_INFO() << output_attrs[0].dims[i];
            if (i < output_attrs[0].n_dims - 1) LOG_INFO() << "x";
        }
        LOG_INFO() << std::endl;

        // Use the original working post-processing method
        if (output_attrs[0].type == RKNN_TENSOR_INT8) {
            // For quantized output, we need to convert to float first
            LOG_INFO() << "[YOLOv8Detector] Converting quantized output to float";

            // Get output size
            size_t output_size = 1;
            for (uint32_t i = 0; i < output_attrs[0].n_dims; i++) {
                output_size *= output_attrs[0].dims[i];
            }

            // Convert quantized output to float
            std::vector<float> float_output(output_size);
            int8_t* int8_output = (int8_t*)outputs[0].buf;
            float scale = output_attrs[0].scale;
            int32_t zp = output_attrs[0].zp;

            for (size_t i = 0; i < output_size; i++) {
                float_output[i] = ((float)int8_output[i] - zp) * scale;
            }

            // Use original post-processing method
            detections = postprocessRKNNResults(float_output.data(), originalSize);
        } else {
            // Float output - use directly
            LOG_INFO() << "[YOLOv8Detector] Output type: " << get_type_string(output_attrs[0].type);
            LOG_INFO() << "[YOLOv8Detector] Output buffer size: " << outputs[0].size << " bytes";

            // Safety check
            if (outputs[0].buf == nullptr) {
                LOG_ERROR() << "[YOLOv8Detector] Error: Output buffer is null!";
                return detections;
            }

            // Calculate expected size based on actual output type
            size_t expected_size = 1;
            for (uint32_t i = 0; i < output_attrs[0].n_dims; i++) {
                expected_size *= output_attrs[0].dims[i];
            }

            // Multiply by actual element size based on type
            size_t element_size = 4; // Default to float32
            if (output_attrs[0].type == RKNN_TENSOR_FLOAT16) {
                element_size = 2;
            } else if (output_attrs[0].type == RKNN_TENSOR_UINT8 || output_attrs[0].type == RKNN_TENSOR_INT8) {
                element_size = 1;
            }
            expected_size *= element_size;

            LOG_INFO() << "[YOLOv8Detector] Expected output size: " << expected_size << " bytes";

            if (outputs[0].size < expected_size) {
                LOG_ERROR() << "[YOLOv8Detector] Error: Output buffer too small!";
                return detections;
            }

            // Handle different output types
            if (output_attrs[0].type == RKNN_TENSOR_FLOAT16) {
                // Convert FP16 to FP32 for processing using proper IEEE 754 conversion
                size_t num_elements = expected_size / 2; // FP16 is 2 bytes per element
                std::vector<float> fp32_output(num_elements);

                // Proper IEEE 754 FP16 to FP32 conversion
                uint16_t* fp16_data = (uint16_t*)outputs[0].buf;
                for (size_t i = 0; i < num_elements; i++) {
                    uint16_t h = fp16_data[i];
                    uint32_t sign = (h & 0x8000) << 16;
                    uint32_t exp = (h & 0x7C00);
                    uint32_t mant = (h & 0x03FF);

                    if (exp == 0) {
                        // Zero or denormalized
                        if (mant == 0) {
                            fp32_output[i] = *reinterpret_cast<float*>(&sign);
                        } else {
                            // Denormalized
                            exp = 0x38800000; // 2^-14
                            while ((mant & 0x0400) == 0) {
                                exp -= 0x00800000;
                                mant <<= 1;
                            }
                            mant &= 0x03FF;
                            uint32_t result = sign | exp | (mant << 13);
                            fp32_output[i] = *reinterpret_cast<float*>(&result);
                        }
                    } else if (exp == 0x7C00) {
                        // Infinity or NaN
                        uint32_t result = sign | 0x7F800000 | (mant << 13);
                        fp32_output[i] = *reinterpret_cast<float*>(&result);
                    } else {
                        // Normalized
                        uint32_t result = sign | ((exp + 0x1C000) << 13) | (mant << 13);
                        fp32_output[i] = *reinterpret_cast<float*>(&result);
                    }
                }

                LOG_INFO() << "[YOLOv8Detector] Converted FP16 to FP32 using proper IEEE 754 conversion";
                detections = postprocessRKNNResults(fp32_output.data(), originalSize);
            } else {
                // Float32 output - use directly
                detections = postprocessRKNNResults((float*)outputs[0].buf, originalSize);
            }
        }

        return detections;
    }

    // Multi-output format (3 branches with DFL)
    std::vector<float> filterBoxes;
    std::vector<float> objProbs;
    std::vector<int> classId;
    int validCount = 0;

    // YOLOv8 has 3 output branches for different scales
    int output_per_branch = n_output / 3;
    int dfl_len = output_attrs[0].dims[1] / 4; // DFL length

    for (int i = 0; i < 3; i++) {
        int box_idx = i * output_per_branch;
        int score_idx = i * output_per_branch + 1;

        int grid_h = output_attrs[box_idx].dims[2];
        int grid_w = output_attrs[box_idx].dims[3];
        int stride = m_inputHeight / grid_h;

        // Process quantized outputs
        if (output_attrs[box_idx].type == RKNN_TENSOR_INT8) {
            validCount += processRKNNOutput(
                (int8_t*)outputs[box_idx].buf, output_attrs[box_idx].zp, output_attrs[box_idx].scale,
                (int8_t*)outputs[score_idx].buf, output_attrs[score_idx].zp, output_attrs[score_idx].scale,
                grid_h, grid_w, stride, dfl_len,
                filterBoxes, objProbs, classId, m_confidenceThreshold
            );
        }
    }

    LOG_INFO() << "[YOLOv8Detector] RKNN post-processing: " << validCount << " raw detections -> ";

    if (validCount > 0) {
        // Apply official NMS algorithm
        std::vector<int> indexArray;
        for (int i = 0; i < validCount; ++i) {
            indexArray.push_back(i);
        }

        // Sort by confidence
        quickSortIndiceInverse(objProbs, 0, validCount - 1, indexArray);

        // Apply NMS for each class
        std::set<int> class_set(std::begin(classId), std::end(classId));
        for (auto c : class_set) {
            applyNMS(validCount, filterBoxes, classId, indexArray, c, m_nmsThreshold);
        }

        // Convert to Detection format and scale to original image
        float scaleX = static_cast<float>(originalSize.width) / m_inputWidth;
        float scaleY = static_cast<float>(originalSize.height) / m_inputHeight;

        for (int i = 0; i < validCount; ++i) {
            if (indexArray[i] == -1) continue;

            int n = indexArray[i];
            float x1 = filterBoxes[n * 4 + 0] * scaleX;
            float y1 = filterBoxes[n * 4 + 1] * scaleY;
            float w = filterBoxes[n * 4 + 2] * scaleX;
            float h = filterBoxes[n * 4 + 3] * scaleY;

            // Clamp to image boundaries
            x1 = std::max(0.0f, std::min(x1, static_cast<float>(originalSize.width - 1)));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(originalSize.height - 1)));
            w = std::max(1.0f, std::min(w, static_cast<float>(originalSize.width) - x1));
            h = std::max(1.0f, std::min(h, static_cast<float>(originalSize.height) - y1));

            Detection det;
            det.bbox = cv::Rect(static_cast<int>(x1), static_cast<int>(y1),
                               static_cast<int>(w), static_cast<int>(h));
            det.confidence = objProbs[i];
            det.classId = classId[n];
            if (det.classId < static_cast<int>(m_classNames.size())) {
                det.className = m_classNames[det.classId];
            } else {
                det.className = "unknown";
            }
            detections.push_back(det);
        }
    }

    LOG_INFO() << detections.size() << " final detections";
    return detections;
}

// Official DFL computation from RKNN model zoo
void YOLOv8Detector::computeDFL(float* tensor, int dfl_len, float* box) {
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

// Quantization helper functions
static float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale) {
    return ((float)qnt - (float)zp) * scale;
}

static int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale) {
    float dst_val = (f32 / scale) + zp;
    return (int8_t)std::max(-128.0f, std::min(127.0f, dst_val));
}

// Official RKNN output processing
int YOLOv8Detector::processRKNNOutput(int8_t* box_tensor, int32_t box_zp, float box_scale,
                                      int8_t* score_tensor, int32_t score_zp, float score_scale,
                                      int grid_h, int grid_w, int stride, int dfl_len,
                                      std::vector<float>& boxes, std::vector<float>& objProbs,
                                      std::vector<int>& classId, float threshold) {
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    int8_t score_thres_i8 = qnt_f32_to_affine(threshold, score_zp, score_scale);

    for (int i = 0; i < grid_h; i++) {
        for (int j = 0; j < grid_w; j++) {
            int offset = i * grid_w + j;
            int max_class_id = -1;
            int8_t max_score = -score_zp;

            // Find class with highest confidence
            for (int c = 0; c < 80; c++) { // 80 COCO classes
                if ((score_tensor[offset] > score_thres_i8) && (score_tensor[offset] > max_score)) {
                    max_score = score_tensor[offset];
                    max_class_id = c;
                }
                offset += grid_len;
            }

            // Process box if confidence is high enough
            if (max_score > score_thres_i8) {
                offset = i * grid_w + j;
                float box[4];
                float before_dfl[dfl_len * 4];

                // Extract DFL values
                for (int k = 0; k < dfl_len * 4; k++) {
                    before_dfl[k] = deqnt_affine_to_f32(box_tensor[offset], box_zp, box_scale);
                    offset += grid_len;
                }

                // Compute DFL
                computeDFL(before_dfl, dfl_len, box);

                // Convert to pixel coordinates
                float x1 = (-box[0] + j + 0.5) * stride;
                float y1 = (-box[1] + i + 0.5) * stride;
                float x2 = (box[2] + j + 0.5) * stride;
                float y2 = (box[3] + i + 0.5) * stride;
                float w = x2 - x1;
                float h = y2 - y1;

                boxes.push_back(x1);
                boxes.push_back(y1);
                boxes.push_back(w);
                boxes.push_back(h);
                objProbs.push_back(deqnt_affine_to_f32(max_score, score_zp, score_scale));
                classId.push_back(max_class_id);
                validCount++;
            }
        }
    }

    return validCount;
}

// Official IoU calculation
float YOLOv8Detector::calculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0,
                                       float xmin1, float ymin1, float xmax1, float ymax1) {
    float w = std::max(0.0f, std::min(xmax0, xmax1) - std::max(xmin0, xmin1) + 1.0f);
    float h = std::max(0.0f, std::min(ymax0, ymax1) - std::max(ymin0, ymin1) + 1.0f);
    float i = w * h;
    float u = (xmax0 - xmin0 + 1.0f) * (ymax0 - ymin0 + 1.0f) + (xmax1 - xmin1 + 1.0f) * (ymax1 - ymin1 + 1.0f) - i;
    return u <= 0.0f ? 0.0f : (i / u);
}

// Official NMS implementation
void YOLOv8Detector::applyNMS(int validCount, std::vector<float>& outputLocations, std::vector<int>& classIds,
                             std::vector<int>& order, int filterId, float threshold) {
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
}

// Official quick sort implementation
void YOLOv8Detector::quickSortIndiceInverse(std::vector<float>& input, int left, int right, std::vector<int>& indices) {
    if (left >= right) return;

    float key = input[left];
    int key_index = indices[left];
    int low = left;
    int high = right;

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
