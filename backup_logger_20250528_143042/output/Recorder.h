#pragma once

#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <opencv2/opencv.hpp>

struct FrameResult;
class DatabaseManager;

/**
 * @brief Event recording configuration
 */
struct RecordingConfig {
    int preEventDuration = 30;   // seconds before event
    int postEventDuration = 30;  // seconds after event
    std::string outputDir = "./recordings";
    int maxFileSize = 100;       // MB
    bool enableTimestamp = true;
    bool enableBBoxOverlay = true;
};

/**
 * @brief Video recorder with event-triggered recording and database integration
 *
 * This class handles:
 * - Circular buffer for pre/post-event recording
 * - MP4 file generation with timestamp and bbox overlays
 * - Database integration for event metadata storage
 * - Manual recording API support
 */
class Recorder {
public:
    Recorder();
    ~Recorder();

    // Initialization
    bool initialize(const std::string& sourceId, std::shared_ptr<DatabaseManager> dbManager = nullptr);
    void setConfig(const RecordingConfig& config);

    // Frame processing
    void processFrame(const FrameResult& result);

    // Manual recording control
    bool startManualRecording(int durationSeconds = 60);
    bool stopManualRecording();
    bool isRecording() const;

    // Event-triggered recording
    void triggerEventRecording(const std::string& eventType, double confidence = 0.0,
                              const std::string& metadata = "");

    // Configuration
    void updateConfig(const RecordingConfig& config);
    RecordingConfig getConfig() const;

    // Statistics
    size_t getBufferSize() const;
    std::string getCurrentRecordingPath() const;

private:
    // Internal structures
    struct FrameData {
        cv::Mat frame;
        std::string timestamp;
        std::vector<cv::Rect> detections;
        std::vector<int> trackIds;
        std::vector<std::string> labels;
        double frameTime;

        FrameData() : frameTime(0.0) {}
    };

    // Internal methods
    void initializeCircularBuffer();
    void addFrameToBuffer(const FrameData& frameData);
    bool startRecording(const std::string& reason, const std::string& eventType = "",
                       double confidence = 0.0, const std::string& metadata = "");
    void stopRecording();
    void writeFrameToVideo(const FrameData& frameData);
    void addTimestampOverlay(cv::Mat& frame, const std::string& timestamp);
    void addBBoxOverlay(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                       const std::vector<std::string>& labels);
    std::string generateOutputPath(const std::string& eventType = "manual");
    bool saveEventToDatabase(const std::string& videoPath, const std::string& eventType,
                           double confidence, const std::string& metadata);

    // Member variables
    std::string m_sourceId;
    RecordingConfig m_config;
    std::shared_ptr<DatabaseManager> m_dbManager;

    // Circular buffer for pre-event frames
    std::vector<FrameData> m_frameBuffer;
    size_t m_bufferIndex;
    size_t m_maxBufferSize;
    mutable std::mutex m_bufferMutex;

    // Recording state
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_isManualRecording{false};
    cv::VideoWriter m_videoWriter;
    std::string m_currentOutputPath;
    std::string m_currentEventType;
    double m_currentConfidence;
    std::string m_currentMetadata;

    // Timing
    std::chrono::steady_clock::time_point m_recordingStartTime;
    std::chrono::steady_clock::time_point m_eventTriggerTime;
    int m_manualRecordingDuration;

    // Thread safety
    mutable std::mutex m_recordingMutex;
};
