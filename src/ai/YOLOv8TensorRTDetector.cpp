/**
 * @file YOLOv8TensorRTDetector.cpp
 * @brief YOLOv8 TensorRT GPU Implementation
 */

#include "YOLOv8TensorRTDetector.h"
#include "../core/Logger.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstring>

#ifdef HAVE_TENSORRT
#include "CUDAUtils.h"
#include <NvOnnxParser.h>
#include <cuda_runtime.h>
#include <NvInfer.h>

namespace AISecurityVision {

// TensorRT Logger implementation
void YOLOv8TensorRTDetector::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        LOG_INFO() << "[TensorRT] " << msg;
    }
}

YOLOv8TensorRTDetector::YOLOv8TensorRTDetector()
    : m_logger(std::make_unique<Logger>()),
      m_hostInputBuffer(nullptr),
      m_hostOutputBuffer(nullptr),
      m_cudaStream(nullptr),
      m_inputIndex(-1),
      m_outputBoxesIndex(-1),
      m_outputScoresIndex(-1) {

    m_backend = InferenceBackend::TENSORRT;
    std::memset(m_deviceBuffers, 0, sizeof(m_deviceBuffers));

    // Create dedicated CUDA stream for inference
    cudaError_t err = cudaStreamCreate(&m_cudaStream);
    if (err != cudaSuccess) {
        LOG_ERROR() << "Failed to create CUDA stream: " << cudaGetErrorString(err);
        m_cudaStream = nullptr;
    }

    // Initialize default COCO class names
    initializeDefaultClassNames();
}

YOLOv8TensorRTDetector::~YOLOv8TensorRTDetector() {
    cleanup();
}

bool YOLOv8TensorRTDetector::initialize(const std::string& modelPath) {
    if (m_initialized) {
        cleanup();
    }
    
    // Check if it's an engine file or ONNX file
    std::string ext = modelPath.substr(modelPath.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    bool success = false;
    if (ext == "engine" || ext == "trt") {
        success = loadEngine(modelPath);
    } else if (ext == "onnx") {
        // Build engine from ONNX
        std::string enginePath = modelPath.substr(0, modelPath.find_last_of(".")) + ".engine";
        if (!fileExists(enginePath)) {
            LOG_INFO() << "Building TensorRT engine from ONNX model...";
            if (!buildEngineFromONNX(modelPath, enginePath)) {
                LOG_ERROR() << "Failed to build engine from ONNX";
                return false;
            }
        }
        success = loadEngine(enginePath);
    } else {
        LOG_ERROR() << "Unsupported model format: " << ext;
        return false;
    }
    
    if (success) {
        m_initialized = true;
        
        // Print device info
        CudaDeviceInfo::getDeviceInfo(0);
    }
    
    return m_initialized;
}

bool YOLOv8TensorRTDetector::buildEngineFromONNX(const std::string& onnxPath, const std::string& enginePath) {
    auto builder = std::unique_ptr<nvinfer1::IBuilder>(
        nvinfer1::createInferBuilder(*m_logger));
    if (!builder) return false;
    
    // Create network (explicit batch is now default)
    auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(
        builder->createNetworkV2(0U));
    if (!network) return false;
    
    auto parser = std::unique_ptr<nvonnxparser::IParser>(
        nvonnxparser::createParser(*network, *m_logger));
    if (!parser) return false;
    
    // Parse ONNX model
    std::ifstream file(onnxPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR() << "Failed to open ONNX file: " << onnxPath;
        return false;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> onnxModel(fileSize);
    file.read(onnxModel.data(), fileSize);
    file.close();
    
    if (!parser->parse(onnxModel.data(), fileSize)) {
        LOG_ERROR() << "Failed to parse ONNX model";
        for (int i = 0; i < parser->getNbErrors(); ++i) {
            LOG_ERROR() << parser->getError(i)->desc();
        }
        return false;
    }
    
    // Build configuration
    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(
        builder->createBuilderConfig());
    if (!config) return false;
    
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, m_workspaceSize);
    
    // Set precision
    if (m_precision == "FP16") {
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
    } else if (m_precision == "INT8") {
        config->setFlag(nvinfer1::BuilderFlag::kINT8);
        // Note: INT8 calibration would be needed here
    }
    
    // Build engine
    LOG_INFO() << "Building TensorRT engine... This may take a few minutes.";
    auto engine = std::unique_ptr<nvinfer1::ICudaEngine>(
        builder->buildEngineWithConfig(*network, *config));
    if (!engine) {
        LOG_ERROR() << "Failed to build engine";
        return false;
    }
    
    // Save engine
    auto serialized = std::unique_ptr<nvinfer1::IHostMemory>(engine->serialize());
    std::ofstream engineFile(enginePath, std::ios::binary);
    if (!engineFile) {
        LOG_ERROR() << "Failed to open engine file for writing";
        return false;
    }

    engineFile.write(reinterpret_cast<const char*>(serialized->data()), serialized->size());
    engineFile.close();

    LOG_INFO() << "TensorRT engine saved to: " << enginePath;
    return true;
}

bool YOLOv8TensorRTDetector::loadEngine(const std::string& enginePath) {
    std::ifstream file(enginePath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR() << "Failed to open engine file: " << enginePath;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();
    
    // Create runtime
    m_runtime.reset(nvinfer1::createInferRuntime(*m_logger));
    if (!m_runtime) return false;
    
    // Deserialize engine
    m_engine.reset(m_runtime->deserializeCudaEngine(engineData.data(), size));
    if (!m_engine) {
        LOG_ERROR() << "Failed to deserialize engine";
        return false;
    }
    
    // Create execution context
    m_context.reset(m_engine->createExecutionContext());
    if (!m_context) return false;
    
    // Get binding indices and dimensions
    int numBindings = m_engine->getNbIOTensors();
    for (int i = 0; i < numBindings; ++i) {
        const char* name = m_engine->getIOTensorName(i);
        bool isInput = (m_engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT);
        auto dims = m_engine->getTensorShape(name);
        
        LOG_DEBUG() << "Binding " << i << ": " << name
                    << " (" << (isInput ? "input" : "output") << ") "
                    << "dims: ";

        // Build dimensions string
        std::string dimsStr;
        for (int d = 0; d < dims.nbDims; ++d) {
            dimsStr += std::to_string(dims.d[d]) + " ";
        }
        LOG_DEBUG() << "Dimensions: " << dimsStr;

        // Debug output dimensions
        if (!isInput) {
            std::string shapeStr = "[";
            for (int d = 0; d < dims.nbDims; ++d) {
                shapeStr += std::to_string(dims.d[d]);
                if (d < dims.nbDims - 1) shapeStr += ", ";
            }
            shapeStr += "]";
            LOG_INFO() << "Output tensor shape: " << shapeStr;
        }
        
        if (isInput) {
            m_inputIndex = i;
            m_inputName = name;
            m_inputDims = dims;
            // Update input dimensions
            if (dims.nbDims >= 4) {
                m_inputHeight = dims.d[2];
                m_inputWidth = dims.d[3];
            }
        } else {
            // YOLOv8 typically has one output: [batch, num_detections, 85]
            // where 85 = 4 (bbox) + 1 (objectness) + 80 (classes)
            m_outputBoxesIndex = i;
            m_outputBoxesName = name;
            m_outputBoxesDims = dims;
        }
    }
    
    if (m_inputIndex < 0) {
        LOG_ERROR() << "No input binding found";
        return false;
    }
    
    // Allocate buffers
    return allocateBuffers();
}

bool YOLOv8TensorRTDetector::allocateBuffers() {
    // Free existing buffers
    freeBuffers();
    
    // Calculate sizes
    size_t inputSize = getSizeByDim(m_inputDims);
    size_t outputSize = getSizeByDim(m_outputBoxesDims);
    
    // Allocate host buffers
    m_hostInputBuffer = new float[inputSize];
    m_hostOutputBuffer = new float[outputSize];
    
    // Allocate device buffers
    CUDA_CHECK(cudaMalloc(&m_deviceBuffers[m_inputIndex], inputSize * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&m_deviceBuffers[m_outputBoxesIndex], outputSize * sizeof(float)));
    
    return true;
}

void YOLOv8TensorRTDetector::freeBuffers() {
    delete[] m_hostInputBuffer;
    delete[] m_hostOutputBuffer;
    m_hostInputBuffer = nullptr;
    m_hostOutputBuffer = nullptr;
    
    for (int i = 0; i < 3; ++i) {
        if (m_deviceBuffers[i]) {
            cudaFree(m_deviceBuffers[i]);
            m_deviceBuffers[i] = nullptr;
        }
    }
}

std::vector<Detection> YOLOv8TensorRTDetector::detectObjects(const cv::Mat& frame) {
    if (!m_initialized) {
        LOG_ERROR() << "Detector not initialized";
        return {};
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Preprocess image
    LetterboxInfo letterbox;
    cv::Mat preprocessed = preprocessImageWithLetterbox(frame, letterbox);
    
    // Do inference
    if (!doInference(preprocessed)) {
        LOG_ERROR() << "Inference failed";
        return {};
    }
    
    // Postprocess results
    auto detections = postprocessResults(
        m_hostOutputBuffer, nullptr,
        m_outputBoxesDims.d[1],
        frame.size(), letterbox);

    // Update performance metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_inferenceTimes.push_back(m_inferenceTime);
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }

    m_detectionCount += detections.size();

    // Debug logging for detection results
    if (!detections.empty()) {
        LOG_DEBUG() << "[TensorRT] Raw detections before filtering: " << detections.size();
        for (size_t i = 0; i < std::min(detections.size(), size_t(3)); ++i) {
            const auto& det = detections[i];
            LOG_DEBUG() << "  Detection " << i << ": class=" << det.classId
                        << " (" << det.className << "), conf=" << det.confidence
                        << ", bbox=" << det.bbox.x << "," << det.bbox.y
                        << "," << det.bbox.width << "," << det.bbox.height;
        }
    }

    // Filter by enabled categories
    auto filteredDetections = filterDetectionsByCategory(detections);

    // Debug logging for filtered results
    if (detections.size() != filteredDetections.size()) {
        LOG_DEBUG() << "[TensorRT] Filtered detections: " << filteredDetections.size()
                    << " (from " << detections.size() << " raw detections)";
    }

    return filteredDetections;
}

cv::Mat YOLOv8TensorRTDetector::preprocessImageWithLetterbox(const cv::Mat& image, LetterboxInfo& letterbox) {
    // Calculate scale to fit the image into input size while maintaining aspect ratio
    float scale_x = static_cast<float>(m_inputWidth) / image.cols;
    float scale_y = static_cast<float>(m_inputHeight) / image.rows;
    float scale = std::min(scale_x, scale_y);
    
    int new_width = static_cast<int>(image.cols * scale);
    int new_height = static_cast<int>(image.rows * scale);
    
    // Resize image
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);
    
    // Create letterboxed image
    cv::Mat letterboxed = cv::Mat::zeros(m_inputHeight, m_inputWidth, CV_8UC3);
    letterboxed.setTo(cv::Scalar(114, 114, 114)); // Gray padding
    
    // Calculate padding
    int pad_x = (m_inputWidth - new_width) / 2;
    int pad_y = (m_inputHeight - new_height) / 2;
    
    // Copy resized image to center
    resized.copyTo(letterboxed(cv::Rect(pad_x, pad_y, new_width, new_height)));
    
    // Update letterbox info
    letterbox.scale = scale;
    letterbox.x_pad = pad_x;
    letterbox.y_pad = pad_y;
    
    // Convert to float and normalize
    cv::Mat floatImg;
    letterboxed.convertTo(floatImg, CV_32FC3, 1.0 / 255.0);
    
    return floatImg;
}

bool YOLOv8TensorRTDetector::doInference(const cv::Mat& input) {
    // Convert OpenCV image to CHW format
    size_t inputSize = m_inputWidth * m_inputHeight * 3;

    // Use default stream if dedicated stream creation failed
    cudaStream_t stream = m_cudaStream ? m_cudaStream : 0;
    
    // Split channels and copy to host buffer
    std::vector<cv::Mat> channels(3);
    cv::split(input, channels);
    
    // Copy in CHW format
    for (int c = 0; c < 3; ++c) {
        std::memcpy(m_hostInputBuffer + c * m_inputWidth * m_inputHeight,
                   channels[c].data,
                   m_inputWidth * m_inputHeight * sizeof(float));
    }
    
    // Copy input to device using stream
    if (stream) {
        CUDA_CHECK(cudaMemcpyAsync(m_deviceBuffers[m_inputIndex], m_hostInputBuffer,
                                  inputSize * sizeof(float), cudaMemcpyHostToDevice, stream));
    } else {
        CUDA_CHECK(cudaMemcpy(m_deviceBuffers[m_inputIndex], m_hostInputBuffer,
                             inputSize * sizeof(float), cudaMemcpyHostToDevice));
    }

    // Set input tensor address
    m_context->setTensorAddress(m_inputName.c_str(), m_deviceBuffers[m_inputIndex]);
    m_context->setTensorAddress(m_outputBoxesName.c_str(), m_deviceBuffers[m_outputBoxesIndex]);

    // Run inference using stream
    bool status = m_context->enqueueV3(stream);
    if (!status) {
        LOG_ERROR() << "TensorRT inference failed";
        return false;
    }

    // Copy output back to host using stream
    size_t outputSize = getSizeByDim(m_outputBoxesDims);
    if (stream) {
        CUDA_CHECK(cudaMemcpyAsync(m_hostOutputBuffer, m_deviceBuffers[m_outputBoxesIndex],
                                  outputSize * sizeof(float), cudaMemcpyDeviceToHost, stream));
        // Synchronize stream
        CUDA_CHECK(cudaStreamSynchronize(stream));
    } else {
        CUDA_CHECK(cudaMemcpy(m_hostOutputBuffer, m_deviceBuffers[m_outputBoxesIndex],
                             outputSize * sizeof(float), cudaMemcpyDeviceToHost));
    }
    
    return true;
}

std::vector<Detection> YOLOv8TensorRTDetector::postprocessResults(
    float* output, float* scores,
    int numDetections,
    const cv::Size& originalSize,
    const LetterboxInfo& letterbox) {

    std::vector<Detection> detections;

    // YOLOv8 output format can be either:
    // [batch, num_predictions, 84] where 84 = 4 (bbox) + 80 (classes) - no objectness
    // or [batch, 84, num_predictions] - transposed format

    // Determine the correct format based on dimensions
    int numClasses = 80;  // Standard COCO classes
    bool isTransposed = false;

    if (m_outputBoxesDims.nbDims == 3) {
        if (m_outputBoxesDims.d[1] == 84 && m_outputBoxesDims.d[2] > 84) {
            // Format: [batch, 84, num_predictions] - transposed
            isTransposed = true;
            numDetections = m_outputBoxesDims.d[2];
        } else if (m_outputBoxesDims.d[2] == 84 && m_outputBoxesDims.d[1] > 84) {
            // Format: [batch, num_predictions, 84] - standard
            isTransposed = false;
            numDetections = m_outputBoxesDims.d[1];
        } else {
            LOG_ERROR() << "[TensorRT] Unexpected output format: ["
                        << m_outputBoxesDims.d[0] << ", "
                        << m_outputBoxesDims.d[1] << ", "
                        << m_outputBoxesDims.d[2] << "]";
        }
    }

    // Debug logging
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 30 == 0) {  // Log every 30 frames
        LOG_DEBUG() << "[TensorRT] Processing " << numDetections << " predictions, "
                    << numClasses << " classes, confidence threshold: " << m_confidenceThreshold
                    << ", format: " << (isTransposed ? "transposed" : "standard");
    }
    
    int validDetections = 0;
    int personDetections = 0;

    for (int i = 0; i < numDetections; ++i) {
        float cx, cy, w, h;
        float bestScore = 0.0f;
        int bestClass = 0;

        if (isTransposed) {
            // Format: [batch, 84, num_predictions]
            // bbox: indices 0-3, classes: indices 4-83
            cx = output[0 * numDetections + i];
            cy = output[1 * numDetections + i];
            w = output[2 * numDetections + i];
            h = output[3 * numDetections + i];

            // Find best class from indices 4-83
            for (int c = 0; c < numClasses; ++c) {
                float score = output[(4 + c) * numDetections + i];
                if (score > bestScore) {
                    bestScore = score;
                    bestClass = c;
                }
            }
        } else {
            // Format: [batch, num_predictions, 84]
            float* ptr = output + i * 84;
            cx = ptr[0];
            cy = ptr[1];
            w = ptr[2];
            h = ptr[3];

            // Find best class from indices 4-83
            for (int c = 0; c < numClasses; ++c) {
                float score = ptr[4 + c];
                if (score > bestScore) {
                    bestScore = score;
                    bestClass = c;
                }
            }
        }

        float confidence = bestScore;  // YOLOv8 doesn't use objectness score

        // Count person detections (class 0 in COCO)
        if (bestClass == 0 && confidence > 0.1f) {  // Lower threshold for counting
            personDetections++;
        }

        if (confidence < m_confidenceThreshold) {
            continue;
        }

        validDetections++;

        // Debug: Print raw bbox coordinates for high-confidence detections
        if (confidence > 0.8f && frameCount % 30 == 0) {
            LOG_DEBUG() << "[TensorRT] Raw bbox for detection " << validDetections
                        << ": cx=" << cx << ", cy=" << cy << ", w=" << w << ", h=" << h
                        << ", class=" << bestClass << ", conf=" << confidence;
        }
        
        // YOLOv8 coordinates are normalized (0-1), scale to input size
        cx *= m_inputWidth;
        cy *= m_inputHeight;
        w *= m_inputWidth;
        h *= m_inputHeight;

        // Convert from center format to corner format
        float x1 = cx - w / 2.0f;
        float y1 = cy - h / 2.0f;
        float x2 = cx + w / 2.0f;
        float y2 = cy + h / 2.0f;

        // Debug: Print scaled coordinates
        if (confidence > 0.8f && frameCount % 30 == 0) {
            LOG_DEBUG() << "[TensorRT] Scaled bbox: x1=" << x1 << ", y1=" << y1
                        << ", x2=" << x2 << ", y2=" << y2;
        }

        // Remove padding effect
        x1 = (x1 - letterbox.x_pad) / letterbox.scale;
        y1 = (y1 - letterbox.y_pad) / letterbox.scale;
        x2 = (x2 - letterbox.x_pad) / letterbox.scale;
        y2 = (y2 - letterbox.y_pad) / letterbox.scale;
        
        // Clip to image bounds
        x1 = std::max(0.0f, std::min(x1, static_cast<float>(originalSize.width - 1)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(originalSize.height - 1)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(originalSize.width - 1)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(originalSize.height - 1)));
        
        Detection det;
        det.bbox = cv::Rect(
            static_cast<int>(x1),
            static_cast<int>(y1),
            static_cast<int>(x2 - x1),
            static_cast<int>(y2 - y1)
        );
        det.confidence = confidence;
        det.classId = bestClass;
        
        if (bestClass < m_classNames.size()) {
            det.className = m_classNames[bestClass];
        }
        
        detections.push_back(det);
    }

    // Debug logging for detection counts
    if (frameCount % 30 == 0) {  // Log every 30 frames
        LOG_DEBUG() << "[TensorRT] Found " << personDetections << " person detections (conf>0.1), "
                    << validDetections << " valid detections (conf>" << m_confidenceThreshold << ")";
    }

    // Apply NMS
    auto nmsResults = performNMS(detections);

    // Debug logging for NMS results
    if (frameCount % 30 == 0 && !nmsResults.empty()) {
        LOG_DEBUG() << "[TensorRT] After NMS: " << nmsResults.size() << " detections";
    }

    return nmsResults;
}

std::vector<Detection> YOLOv8TensorRTDetector::performNMS(const std::vector<Detection>& detections) {
    if (detections.empty()) return {};
    
    // Group by class
    std::map<int, std::vector<int>> classIndices;
    for (size_t i = 0; i < detections.size(); ++i) {
        classIndices[detections[i].classId].push_back(i);
    }
    
    std::vector<Detection> result;
    
    for (const auto& [classId, indices] : classIndices) {
        // Sort by confidence
        std::vector<int> sortedIndices = indices;
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&detections](int a, int b) {
                return detections[a].confidence > detections[b].confidence;
            });
        
        std::vector<bool> suppressed(sortedIndices.size(), false);
        
        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            if (suppressed[i]) continue;
            
            int idx1 = sortedIndices[i];
            result.push_back(detections[idx1]);
            
            const cv::Rect& box1 = detections[idx1].bbox;
            
            for (size_t j = i + 1; j < sortedIndices.size(); ++j) {
                if (suppressed[j]) continue;
                
                int idx2 = sortedIndices[j];
                const cv::Rect& box2 = detections[idx2].bbox;
                
                // Calculate IoU
                int x1 = std::max(box1.x, box2.x);
                int y1 = std::max(box1.y, box2.y);
                int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
                int y2 = std::min(box1.y + box1.height, box2.y + box2.height);
                
                int w = std::max(0, x2 - x1);
                int h = std::max(0, y2 - y1);
                
                float inter = w * h;
                float area1 = box1.width * box1.height;
                float area2 = box2.width * box2.height;
                float iou = inter / (area1 + area2 - inter);
                
                if (iou > m_nmsThreshold) {
                    suppressed[j] = true;
                }
            }
        }
    }
    
    return result;
}

bool YOLOv8TensorRTDetector::isInitialized() const {
    return m_initialized;
}

InferenceBackend YOLOv8TensorRTDetector::getCurrentBackend() const {
    return m_backend;
}

std::string YOLOv8TensorRTDetector::getBackendName() const {
    return "TensorRT GPU";
}

void YOLOv8TensorRTDetector::cleanup() {
    if (m_initialized) {
        freeBuffers();
        m_context.reset();
        m_engine.reset();
        m_runtime.reset();
        m_initialized = false;
    }

    // Destroy CUDA stream
    if (m_cudaStream) {
        cudaStreamDestroy(m_cudaStream);
        m_cudaStream = nullptr;
    }
}

std::vector<std::string> YOLOv8TensorRTDetector::getModelInfo() const {
    std::vector<std::string> info;
    
    if (!m_initialized) {
        info.push_back("Model not loaded");
        return info;
    }
    
    info.push_back("Backend: TensorRT GPU");
    info.push_back("Precision: " + m_precision);
    info.push_back("Input size: " + std::to_string(m_inputWidth) + "x" + std::to_string(m_inputHeight));
    info.push_back("Max batch size: " + std::to_string(m_maxBatchSize));
    
    if (m_engine) {
        info.push_back("Engine version: " + std::to_string(m_engine->getNbLayers()) + " layers");
        // Note: getDeviceMemorySize() is deprecated in TensorRT 10.x
        info.push_back("Engine layers: " + std::to_string(m_engine->getNbLayers()));
    }
    
    // CUDA device info
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
        info.push_back("GPU: " + std::string(prop.name));
        info.push_back("Compute capability: " + std::to_string(prop.major) + "." + std::to_string(prop.minor));
    }
    
    return info;
}

bool YOLOv8TensorRTDetector::setMaxBatchSize(int batchSize) {
    if (m_initialized) {
        LOG_ERROR() << "Cannot change batch size after initialization";
        return false;
    }
    m_maxBatchSize = batchSize;
    return true;
}

bool YOLOv8TensorRTDetector::setWorkspaceSize(size_t size) {
    if (m_initialized) {
        LOG_ERROR() << "Cannot change workspace size after initialization";
        return false;
    }
    m_workspaceSize = size;
    return true;
}

bool YOLOv8TensorRTDetector::setPrecision(const std::string& precision) {
    if (m_initialized) {
        LOG_ERROR() << "Cannot change precision after initialization";
        return false;
    }
    if (precision != "FP32" && precision != "FP16" && precision != "INT8") {
        LOG_ERROR() << "Invalid precision: " << precision;
        return false;
    }
    m_precision = precision;
    return true;
}

bool YOLOv8TensorRTDetector::enableDLA(int dlaCore) {
    m_dlaCore = dlaCore;
    return true;
}

bool YOLOv8TensorRTDetector::saveEngine(const std::string& enginePath) {
    if (!m_engine) {
        LOG_ERROR() << "No engine loaded";
        return false;
    }

    auto serialized = std::unique_ptr<nvinfer1::IHostMemory>(m_engine->serialize());
    std::ofstream file(enginePath, std::ios::binary);
    if (!file) {
        LOG_ERROR() << "Failed to open file for writing: " << enginePath;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(serialized->data()), serialized->size());
    file.close();
    
    return true;
}

size_t YOLOv8TensorRTDetector::getSizeByDim(const nvinfer1::Dims& dims) {
    size_t size = 1;
    for (int i = 0; i < dims.nbDims; ++i) {
        size *= dims.d[i];
    }
    return size;
}

bool YOLOv8TensorRTDetector::fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

std::string YOLOv8TensorRTDetector::getPrecisionString(nvinfer1::DataType dataType) {
    switch (dataType) {
        case nvinfer1::DataType::kFLOAT: return "FP32";
        case nvinfer1::DataType::kHALF: return "FP16";
        case nvinfer1::DataType::kINT8: return "INT8";
        default: return "Unknown";
    }
}

} // namespace AISecurityVision

#endif // HAVE_TENSORRT
