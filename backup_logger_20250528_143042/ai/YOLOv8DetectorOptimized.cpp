#include "YOLOv8DetectorOptimized.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <fstream>

#ifdef HAVE_RKNN
#include <rknn_api.h>
#endif

// Thread-local storage for buffers
thread_local YOLOv8DetectorOptimized::ThreadLocalBuffers YOLOv8DetectorOptimized::t_buffers;

YOLOv8DetectorOptimized::YOLOv8DetectorOptimized(int numThreads)
    : YOLOv8Detector(), m_numThreads(numThreads) {
    m_startTime = std::chrono::high_resolution_clock::now();
    std::cout << "[YOLOv8DetectorOptimized] Creating optimized detector with " << numThreads << " threads" << std::endl;
}

YOLOv8DetectorOptimized::~YOLOv8DetectorOptimized() {
    // Stop worker threads
    m_stopWorkers = true;
    m_queueCondition.notify_all();

    // Wait for all workers to finish
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    // Cleanup RKNN contexts
    cleanupMultiRKNN();

    std::cout << "[YOLOv8DetectorOptimized] Optimized detector destroyed" << std::endl;
}

bool YOLOv8DetectorOptimized::initialize(const std::string& modelPath, InferenceBackend backend) {
    std::cout << "[YOLOv8DetectorOptimized] Initializing optimized detector..." << std::endl;

    // Initialize base detector first
    if (!YOLOv8Detector::initialize(modelPath, backend)) {
        std::cerr << "[YOLOv8DetectorOptimized] Failed to initialize base detector" << std::endl;
        return false;
    }

    // Only optimize for RKNN backend
    if (m_backend != InferenceBackend::RKNN) {
        std::cout << "[YOLOv8DetectorOptimized] Multi-threading optimization only available for RKNN backend" << std::endl;
        return true;
    }

    // Initialize multi-RKNN contexts
    if (!initializeMultiRKNN(modelPath)) {
        std::cerr << "[YOLOv8DetectorOptimized] Failed to initialize multi-RKNN contexts" << std::endl;
        return false;
    }

    // Start worker threads
    m_workers.reserve(m_numThreads);
    for (int i = 0; i < m_numThreads; ++i) {
        m_workers.emplace_back(&YOLOv8DetectorOptimized::workerThread, this, i);
    }

    std::cout << "[YOLOv8DetectorOptimized] Optimized detector initialized successfully with "
              << m_numThreads << " worker threads" << std::endl;
    return true;
}

bool YOLOv8DetectorOptimized::initializeMultiRKNN(const std::string& modelPath) {
#ifdef HAVE_RKNN
    std::cout << "[YOLOv8DetectorOptimized] Initializing " << m_numThreads << " RKNN contexts..." << std::endl;

    // Read model file once
    std::ifstream modelFile(modelPath, std::ios::binary);
    if (!modelFile.good()) {
        std::cerr << "[YOLOv8DetectorOptimized] Failed to open model file: " << modelPath << std::endl;
        return false;
    }

    modelFile.seekg(0, std::ios::end);
    size_t modelSize = modelFile.tellg();
    modelFile.seekg(0, std::ios::beg);

    std::vector<char> modelData(modelSize);
    modelFile.read(modelData.data(), modelSize);
    modelFile.close();

    // Initialize multiple RKNN contexts
    m_rknnContexts.resize(m_numThreads);
    m_rknnInputAttrsVec.resize(m_numThreads);

    for (int i = 0; i < m_numThreads; ++i) {
        // Initialize RKNN context
        int ret = rknn_init(&m_rknnContexts[i], modelData.data(), modelSize, 0, nullptr);
        if (ret < 0) {
            std::cerr << "[YOLOv8DetectorOptimized] Failed to initialize RKNN context " << i << ": " << ret << std::endl;
            return false;
        }

        // Enable multi-core NPU for each context for maximum performance
        ret = rknn_set_core_mask(m_rknnContexts[i], RKNN_NPU_CORE_0_1_2);
        if (ret < 0) {
            std::cout << "[YOLOv8DetectorOptimized] Warning: Failed to set multi-core NPU for context " << i << std::endl;
        } else {
            std::cout << "[YOLOv8DetectorOptimized] Enabled multi-core NPU (0_1_2) for context " << i << std::endl;
        }

        // Note: NPU frequency optimization should be done at system level
        // Use the optimize_npu_performance.sh script for system-wide NPU optimization

        // Query input attributes for each context
        rknn_input_output_num io_num;
        ret = rknn_query(m_rknnContexts[i], RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
        if (ret < 0) {
            std::cerr << "[YOLOv8DetectorOptimized] Failed to query I/O for context " << i << std::endl;
            return false;
        }

        if (io_num.n_input > 0) {
            m_rknnInputAttrsVec[i].index = 0;
            ret = rknn_query(m_rknnContexts[i], RKNN_QUERY_INPUT_ATTR, &m_rknnInputAttrsVec[i], sizeof(rknn_tensor_attr));
            if (ret < 0) {
                std::cerr << "[YOLOv8DetectorOptimized] Failed to query input attr for context " << i << std::endl;
                return false;
            }
        }

        std::cout << "[YOLOv8DetectorOptimized] RKNN context " << i << " initialized successfully" << std::endl;
    }

    return true;
#else
    std::cerr << "[YOLOv8DetectorOptimized] RKNN support not compiled in" << std::endl;
    return false;
#endif
}

void YOLOv8DetectorOptimized::cleanupMultiRKNN() {
#ifdef HAVE_RKNN
    for (size_t i = 0; i < m_rknnContexts.size(); ++i) {
        if (m_rknnContexts[i] != 0) {
            rknn_destroy(m_rknnContexts[i]);
            m_rknnContexts[i] = 0;
        }
    }
    m_rknnContexts.clear();
    m_rknnInputAttrsVec.clear();
#endif
}

std::future<std::vector<YOLOv8DetectorOptimized::Detection>> YOLOv8DetectorOptimized::detectAsync(const cv::Mat& frame) {
    auto task = std::make_unique<InferenceTask>(frame);
    auto future = task->promise.get_future();

    // Add task to queue
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);

        // Check queue size limit
        if (m_taskQueue.size() >= m_maxQueueSize) {
            // Drop oldest task if queue is full
            if (!m_taskQueue.empty()) {
                auto droppedTask = std::move(m_taskQueue.front());
                m_taskQueue.pop();
                // Set empty result for dropped task
                droppedTask->promise.set_value(std::vector<Detection>());
            }
        }

        m_taskQueue.push(std::move(task));
    }

    m_queueCondition.notify_one();
    return future;
}

std::vector<YOLOv8DetectorOptimized::Detection> YOLOv8DetectorOptimized::detect(const cv::Mat& frame) {
    // For synchronous detection, use async and wait
    auto future = detectAsync(frame);
    return future.get();
}

void YOLOv8DetectorOptimized::workerThread(int threadId) {
    std::cout << "[YOLOv8DetectorOptimized] Worker thread " << threadId << " started" << std::endl;

    while (!m_stopWorkers) {
        std::unique_ptr<InferenceTask> task;

        // Get task from queue
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] { return !m_taskQueue.empty() || m_stopWorkers; });

            if (m_stopWorkers) {
                break;
            }

            if (!m_taskQueue.empty()) {
                task = std::move(m_taskQueue.front());
                m_taskQueue.pop();
            }
        }

        if (task) {
            processInferenceTask(std::move(task), threadId);
        }
    }

    std::cout << "[YOLOv8DetectorOptimized] Worker thread " << threadId << " stopped" << std::endl;
}

void YOLOv8DetectorOptimized::processInferenceTask(std::unique_ptr<InferenceTask> task, int threadId) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto queueTime = std::chrono::duration<double, std::milli>(startTime - task->submitTime).count();

    std::vector<Detection> detections;

    try {
        if (m_backend == InferenceBackend::RKNN && threadId < static_cast<int>(m_rknnContexts.size())) {
            // Use thread-specific RKNN context for parallel processing
            detections = detectWithRKNNOptimized(task->frame, threadId);
        } else {
            // Fallback to base implementation
            detections = YOLOv8Detector::detectObjects(task->frame);
        }
    } catch (const std::exception& e) {
        std::cerr << "[YOLOv8DetectorOptimized] Inference failed in thread " << threadId << ": " << e.what() << std::endl;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto inferenceTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Update performance statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_inferenceTimes.push_back(inferenceTime);
        m_queueTimes.push_back(queueTime);

        // Keep only recent measurements (last 100)
        if (m_inferenceTimes.size() > 100) {
            m_inferenceTimes.erase(m_inferenceTimes.begin());
            m_queueTimes.erase(m_queueTimes.begin());
        }
    }

    // Set result
    task->promise.set_value(std::move(detections));
}

std::vector<YOLOv8DetectorOptimized::Detection> YOLOv8DetectorOptimized::detectWithRKNNOptimized(const cv::Mat& frame, int threadId) {
#ifdef HAVE_RKNN
    std::vector<Detection> detections;

    if (threadId >= static_cast<int>(m_rknnContexts.size()) || m_rknnContexts[threadId] == 0) {
        return detections;
    }

    rknn_context ctx = m_rknnContexts[threadId];
    rknn_tensor_attr& inputAttrs = m_rknnInputAttrsVec[threadId];

    // Preprocess image for RKNN (reuse thread-local buffer)
    cv::Mat& preprocessed = t_buffers.preprocessedFrame;

    // Resize with proper aspect ratio handling (letterbox)
    cv::resize(frame, preprocessed, cv::Size(m_inputWidth, m_inputHeight));

    // Convert to RGB if needed (OpenCV uses BGR by default)
    if (preprocessed.channels() == 3) {
        cv::cvtColor(preprocessed, preprocessed, cv::COLOR_BGR2RGB);
    }

    // Convert based on model input type with proper normalization
    if (inputAttrs.type == RKNN_TENSOR_FLOAT32) {
        // Normalize to [0, 1] for float32 models
        preprocessed.convertTo(preprocessed, CV_32F, 1.0/255.0);
    } else if (inputAttrs.type == RKNN_TENSOR_FLOAT16) {
        // Normalize to [0, 1] for float16 models (RKNN handles FP16 conversion)
        preprocessed.convertTo(preprocessed, CV_32F, 1.0/255.0);
    } else {
        // Keep as uint8 for quantized models (no normalization needed)
        preprocessed.convertTo(preprocessed, CV_8U);
    }

    // Set input
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = inputAttrs.type;
    inputs[0].size = preprocessed.total() * preprocessed.elemSize();
    inputs[0].fmt = inputAttrs.fmt;
    inputs[0].buf = preprocessed.data;

    int ret = rknn_inputs_set(ctx, 1, inputs);
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorOptimized] Failed to set RKNN inputs (thread " << threadId << "): " << ret << std::endl;
        return detections;
    }

    // Run inference
    ret = rknn_run(ctx, nullptr);
    if (ret < 0) {
        std::cerr << "[YOLOv8DetectorOptimized] Failed to run RKNN inference (thread " << threadId << "): " << ret << std::endl;
        return detections;
    }

    // Query output attributes
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        return detections;
    }

    std::vector<rknn_tensor_attr> output_attrs(io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            return detections;
        }
    }

    // Get outputs
    std::vector<rknn_output> outputs(io_num.n_output);
    memset(outputs.data(), 0, sizeof(rknn_output) * io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        outputs[i].want_float = 0; // Get quantized output for better performance
    }

    ret = rknn_outputs_get(ctx, io_num.n_output, outputs.data(), nullptr);
    if (ret < 0) {
        return detections;
    }

    // Post-process results
    detections = postprocessRKNNResultsOptimized(outputs.data(), output_attrs.data(), io_num.n_output, frame.size());

    // Release outputs
    rknn_outputs_release(ctx, io_num.n_output, outputs.data());

    return detections;
#else
    return std::vector<Detection>();
#endif
}

std::vector<YOLOv8DetectorOptimized::Detection> YOLOv8DetectorOptimized::postprocessRKNNResultsOptimized(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize) {
    std::vector<Detection> detections;

    // Use the official YOLOv8 RKNN post-processing from base class
    // This handles both single-output and multi-output formats correctly
    return YOLOv8Detector::postprocessRKNNResultsOfficial(outputs, output_attrs, n_output, originalSize);
}

YOLOv8DetectorOptimized::PerformanceStats YOLOv8DetectorOptimized::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    PerformanceStats stats;

    if (!m_inferenceTimes.empty()) {
        stats.avgInferenceTime = std::accumulate(m_inferenceTimes.begin(), m_inferenceTimes.end(), 0.0) / m_inferenceTimes.size();
        stats.avgQueueTime = std::accumulate(m_queueTimes.begin(), m_queueTimes.end(), 0.0) / m_queueTimes.size();
        stats.totalInferences = m_inferenceTimes.size();

        // Calculate throughput
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(now - m_startTime).count();
        if (elapsed > 0) {
            stats.throughput = stats.totalInferences / elapsed;
        }
    }

    {
        std::lock_guard<std::mutex> queueLock(const_cast<std::mutex&>(m_queueMutex));
        stats.queueSize = m_taskQueue.size();
    }

    return stats;
}
