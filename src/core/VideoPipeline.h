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
class ReIDExtractor;
class FaceRecognizer;
class LicensePlateRecognizer;
class BehaviorAnalyzer;
class Recorder;
class Streamer;
class AlarmTrigger;

// Include BehaviorEvent for FrameResult
struct BehaviorEvent;

// Forward declarations for behavior analysis types
struct ROI;
struct IntrusionRule;

// Forward declarations for streaming types
struct StreamConfig;

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

    // Streaming configuration
    bool configureStreaming(const StreamConfig& config);
    StreamConfig getStreamConfig() const;
    bool startStreaming();
    bool stopStreaming();
    bool isStreamingEnabled() const;
    std::string getStreamUrl() const;
    size_t getConnectedClients() const;
    double getStreamFps() const;

    // Behavior analysis rule management
    bool addIntrusionRule(const IntrusionRule& rule);
    bool removeIntrusionRule(const std::string& ruleId);
    bool updateIntrusionRule(const IntrusionRule& rule);
    std::vector<IntrusionRule> getIntrusionRules() const;
    bool addROI(const ROI& roi);
    bool removeROI(const std::string& roiId);
    std::vector<ROI> getROIs() const;

    // Statistics
    double getFrameRate() const;
    size_t getProcessedFrames() const;
    size_t getDroppedFrames() const;
    std::string getLastError() const;
    bool isStreamStable() const;

    // Access methods
    const VideoSource& getSource() const;
    std::chrono::steady_clock::time_point getStartTime() const;

    // Task 76: BehaviorAnalyzer access for ReID configuration
    BehaviorAnalyzer* getBehaviorAnalyzer() const;

private:
    // Processing thread
    void processingThread();
    void processFrame(const cv::Mat& frame, int64_t timestamp);

    // Error handling and health monitoring
    void handleError(const std::string& error);
    bool shouldReconnect() const;
    void attemptReconnection();
    void updateHealthMetrics();
    void checkStreamHealth();

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
    std::unique_ptr<ReIDExtractor> m_reidExtractor;
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

    // Health monitoring
    std::atomic<size_t> m_consecutiveErrors{0};
    std::atomic<size_t> m_totalReconnects{0};
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::chrono::steady_clock::time_point m_lastHealthCheck;
    std::atomic<bool> m_streamStable{true};
    std::atomic<double> m_avgFrameInterval{0.0};

    // Timing
    std::chrono::steady_clock::time_point m_startTime;

    // Constants
    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_DELAY_MS = 5000;
    static constexpr double HEALTH_CHECK_INTERVAL_S = 10.0;
    static constexpr size_t MAX_CONSECUTIVE_ERRORS = 10;
    static constexpr double FRAME_TIMEOUT_S = 30.0;
    static constexpr double STABLE_FRAME_RATE_THRESHOLD = 0.5; // 50% of expected frame rate
};

/**
 * @brief Frame processing result structure
 */
struct FrameResult {
    cv::Mat frame;
    int64_t timestamp;
    std::vector<cv::Rect> detections;
    std::vector<int> trackIds;
    std::vector<int> globalTrackIds;  // Task 75: Global cross-camera track IDs
    std::vector<std::string> labels;  // Detection class labels
    std::vector<std::vector<float>> reidEmbeddings;  // Task 74: ReID feature vectors
    std::vector<std::string> faceIds;
    std::vector<std::string> plateNumbers;
    std::vector<BehaviorEvent> events;  // Changed from std::string to BehaviorEvent
    std::vector<ROI> activeROIs;  // Task 73: Active ROIs for visualization
    bool hasAlarm = false;
};
