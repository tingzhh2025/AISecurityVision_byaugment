#pragma once

#include "YOLOv8Detector.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

/**
 * @brief Optimized YOLOv8 detector with multi-threading and NPU optimization
 *
 * This class extends the base YOLOv8Detector with:
 * - Multi-threaded inference pool for parallel processing
 * - Optimized memory management
 * - Asynchronous detection processing
 * - Better utilization of RK3588's 3-core NPU
 */
class YOLOv8DetectorOptimized : public YOLOv8Detector {
public:
    /**
     * @brief Inference task structure for thread pool
     */
    struct InferenceTask {
        cv::Mat frame;
        std::promise<std::vector<Detection>> promise;
        std::chrono::high_resolution_clock::time_point submitTime;

        InferenceTask(const cv::Mat& f) : frame(f.clone()), submitTime(std::chrono::high_resolution_clock::now()) {}
    };

    /**
     * @brief Constructor
     * @param numThreads Number of inference threads (default: 3 for RK3588's 3 NPU cores)
     */
    explicit YOLOv8DetectorOptimized(int numThreads = 3);

    /**
     * @brief Destructor
     */
    ~YOLOv8DetectorOptimized();

    /**
     * @brief Initialize the optimized detector
     * @param modelPath Path to the model file
     * @param backend Inference backend to use
     * @return true if initialization successful
     */
    bool initialize(const std::string& modelPath, InferenceBackend backend = InferenceBackend::AUTO);

    /**
     * @brief Asynchronous detection method
     * @param frame Input frame
     * @return Future containing detection results
     */
    std::future<std::vector<Detection>> detectAsync(const cv::Mat& frame);

    /**
     * @brief Synchronous detection method (blocking)
     * @param frame Input frame
     * @return Detection results
     */
    std::vector<Detection> detect(const cv::Mat& frame);

    /**
     * @brief Get performance statistics
     */
    struct PerformanceStats {
        double avgInferenceTime = 0.0;
        double avgQueueTime = 0.0;
        size_t totalInferences = 0;
        size_t queueSize = 0;
        double throughput = 0.0; // FPS
    };

    PerformanceStats getPerformanceStats() const;

    /**
     * @brief Set the maximum queue size for inference tasks
     * @param maxSize Maximum number of queued tasks
     */
    void setMaxQueueSize(size_t maxSize) { m_maxQueueSize = maxSize; }

private:
    /**
     * @brief Worker thread function for inference processing
     * @param threadId Thread identifier
     */
    void workerThread(int threadId);

    /**
     * @brief Initialize RKNN contexts for multi-threading
     * @param modelPath Path to the model file
     * @return true if successful
     */
    bool initializeMultiRKNN(const std::string& modelPath);

    /**
     * @brief Cleanup RKNN contexts
     */
    void cleanupMultiRKNN();

    /**
     * @brief Process a single inference task
     * @param task The inference task to process
     * @param threadId Thread identifier
     */
    void processInferenceTask(std::unique_ptr<InferenceTask> task, int threadId);

    /**
     * @brief Optimized RKNN detection for specific thread
     * @param frame Input frame
     * @param threadId Thread identifier
     * @return Detection results
     */
    std::vector<Detection> detectWithRKNNOptimized(const cv::Mat& frame, int threadId);

    /**
     * @brief Optimized RKNN post-processing
     * @param outputs RKNN output data
     * @param output_attrs Output attributes
     * @param n_output Number of outputs
     * @param originalSize Original image size
     * @return Detection results
     */
    std::vector<Detection> postprocessRKNNResultsOptimized(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize);

    /**
     * @brief Optimized RKNN post-processing with letterbox correction
     * @param outputs RKNN output data
     * @param output_attrs Output attributes
     * @param n_output Number of outputs
     * @param originalSize Original image size
     * @param letterbox Letterbox transformation info
     * @return Detection results
     */
    std::vector<Detection> postprocessRKNNResultsOptimizedWithLetterbox(rknn_output* outputs, rknn_tensor_attr* output_attrs, uint32_t n_output, const cv::Size& originalSize, const LetterboxInfo& letterbox);

    // Thread pool management
    std::vector<std::thread> m_workers;
    std::queue<std::unique_ptr<InferenceTask>> m_taskQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::atomic<bool> m_stopWorkers{false};

    // Multi-RKNN contexts (one per thread)
    std::vector<rknn_context> m_rknnContexts;
    std::vector<rknn_tensor_attr> m_rknnInputAttrsVec;

    // Configuration
    int m_numThreads;
    size_t m_maxQueueSize = 10;

    // Performance tracking
    mutable std::mutex m_statsMutex;
    std::vector<double> m_inferenceTimes;
    std::vector<double> m_queueTimes;
    std::chrono::high_resolution_clock::time_point m_startTime;

    // Memory optimization
    struct ThreadLocalBuffers {
        cv::Mat preprocessedFrame;
        std::vector<float> outputBuffer;
    };
    thread_local static ThreadLocalBuffers t_buffers;
};
