#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <opencv2/opencv.hpp>

// Forward declarations
class FFmpegDecoder;
class YOLOv8Detector;
class ByteTracker;
class FaceRecognizer;
class LicensePlateRecognizer;
class BehaviorAnalyzer;
class Recorder;
class Streamer;
class AlarmTrigger;

// VideoSource definition (moved from TaskManager.h to avoid circular dependency)
struct VideoSource {
    std::string id;
    std::string url;
    std::string protocol; // "rtsp", "onvif", "gb28181"
    std::string username;
    std::string password;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    bool enabled = true;

    // Validation
    bool isValid() const;
    std::string toString() const;
};

/**
 * @brief Main video processing pipeline for a single video stream
 *
 * This class implements the complete processing chain:
 * Input -> Decode -> Detect -> Track -> Recognize -> Analyze -> Output
 *
 * Each pipeline runs in its own thread and processes frames sequentially
 * through the AI modules with proper error handling and resource management.
 */
class VideoPipeline {
public:
    explicit VideoPipeline(const VideoSource& source);
    ~VideoPipeline();

    // Lifecycle management
    bool initialize();
    void start();
    void stop();
    bool isRunning() const;
    bool isHealthy() const;

    // Configuration
    void setDetectionEnabled(bool enabled);
    void setRecordingEnabled(bool enabled);
    void setStreamingEnabled(bool enabled);

    // Statistics
    double getFrameRate() const;
    size_t getProcessedFrames() const;
    size_t getDroppedFrames() const;
    std::string getLastError() const;

private:
    // Processing thread
    void processingThread();
    void processFrame(const cv::Mat& frame, int64_t timestamp);

    // Error handling
    void handleError(const std::string& error);
    bool shouldReconnect() const;
    void attemptReconnection();

    // Member variables
    VideoSource m_source;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_healthy{true};
    std::thread m_processingThread;
    mutable std::mutex m_mutex;

    // Processing modules
    std::unique_ptr<FFmpegDecoder> m_decoder;
    std::unique_ptr<YOLOv8Detector> m_detector;
    std::unique_ptr<ByteTracker> m_tracker;
    std::unique_ptr<FaceRecognizer> m_faceRecognizer;
    std::unique_ptr<LicensePlateRecognizer> m_plateRecognizer;
    std::unique_ptr<BehaviorAnalyzer> m_behaviorAnalyzer;

    // Output modules
    std::unique_ptr<Recorder> m_recorder;
    std::unique_ptr<Streamer> m_streamer;
    std::unique_ptr<AlarmTrigger> m_alarmTrigger;

    // Configuration flags
    std::atomic<bool> m_detectionEnabled{true};
    std::atomic<bool> m_recordingEnabled{false};
    std::atomic<bool> m_streamingEnabled{false};

    // Statistics
    mutable std::atomic<double> m_frameRate{0.0};
    mutable std::atomic<size_t> m_processedFrames{0};
    mutable std::atomic<size_t> m_droppedFrames{0};
    mutable std::string m_lastError;

    // Timing
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::chrono::steady_clock::time_point m_startTime;

    // Constants
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_DELAY_MS = 5000;
    static constexpr double HEALTH_CHECK_INTERVAL_S = 10.0;
};

/**
 * @brief Frame processing result structure
 */
struct FrameResult {
    cv::Mat frame;
    int64_t timestamp;
    std::vector<cv::Rect> detections;
    std::vector<int> trackIds;
    std::vector<std::string> faceIds;
    std::vector<std::string> plateNumbers;
    std::vector<std::string> events;
    bool hasAlarm = false;
};
