#include "Recorder.h"
#include "../core/VideoPipeline.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "../core/Logger.h"
using namespace AISecurityVision;
Recorder::Recorder()
    : m_bufferIndex(0), m_maxBufferSize(0), m_currentConfidence(0.0),
      m_manualRecordingDuration(0) {
}

Recorder::~Recorder() {
    if (m_isRecording.load()) {
        stopRecording();
    }
}

bool Recorder::initialize(const std::string& sourceId, std::shared_ptr<DatabaseManager> dbManager) {
    std::lock_guard<std::mutex> lock(m_recordingMutex);

    m_sourceId = sourceId;
    m_dbManager = dbManager;

    // Create output directory if it doesn't exist
    try {
        std::filesystem::create_directories(m_config.outputDir);
    } catch (const std::exception& e) {
        LOG_ERROR() << "[Recorder] Failed to create output directory: " << e.what();
        return false;
    }

    // Initialize circular buffer
    initializeCircularBuffer();

    LOG_INFO() << "[Recorder] Initialized for " << sourceId
              << " with output directory: " << m_config.outputDir;
    return true;
}

void Recorder::setConfig(const RecordingConfig& config) {
    std::lock_guard<std::mutex> lock(m_recordingMutex);
    m_config = config;

    // Recreate output directory if changed
    try {
        std::filesystem::create_directories(m_config.outputDir);
    } catch (const std::exception& e) {
        LOG_ERROR() << "[Recorder] Failed to create output directory: " << e.what();
    }

    // Reinitialize buffer if duration changed
    initializeCircularBuffer();
}

void Recorder::initializeCircularBuffer() {
    // Calculate buffer size based on pre-event duration
    // Assuming 25 FPS, buffer size = preEventDuration * 25
    m_maxBufferSize = static_cast<size_t>(m_config.preEventDuration * 25);

    std::lock_guard<std::mutex> bufferLock(m_bufferMutex);
    m_frameBuffer.clear();
    m_frameBuffer.reserve(m_maxBufferSize);
    m_bufferIndex = 0;

    LOG_INFO() << "[Recorder] Circular buffer initialized with size: " << m_maxBufferSize;
}

void Recorder::processFrame(const FrameResult& result) {
    // Convert FrameResult to FrameData
    FrameData frameData;
    frameData.frame = result.frame.clone();
    frameData.detections = result.detections;
    frameData.trackIds = result.trackIds;
    frameData.labels = result.labels;
    frameData.frameTime = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    frameData.timestamp = ss.str();

    // Add to circular buffer
    addFrameToBuffer(frameData);

    // If recording, write frame to video
    if (m_isRecording.load()) {
        writeFrameToVideo(frameData);

        // Check if manual recording should stop
        if (m_isManualRecording.load()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - m_recordingStartTime).count();

            if (elapsed >= m_manualRecordingDuration) {
                stopManualRecording();
            }
        }

        // Check if event recording should stop (post-event duration)
        if (!m_isManualRecording.load() && !m_currentEventType.empty()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - m_eventTriggerTime).count();

            if (elapsed >= m_config.postEventDuration) {
                stopRecording();
            }
        }
    }
}

void Recorder::addFrameToBuffer(const FrameData& frameData) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    if (m_frameBuffer.size() < m_maxBufferSize) {
        m_frameBuffer.push_back(frameData);
    } else {
        // Circular buffer - overwrite oldest frame
        m_frameBuffer[m_bufferIndex] = frameData;
        m_bufferIndex = (m_bufferIndex + 1) % m_maxBufferSize;
    }
}

bool Recorder::startManualRecording(int durationSeconds) {
    std::lock_guard<std::mutex> lock(m_recordingMutex);

    if (m_isRecording.load()) {
        LOG_INFO() << "[Recorder] Already recording, cannot start manual recording";
        return false;
    }

    m_manualRecordingDuration = durationSeconds;
    m_isManualRecording.store(true);

    return startRecording("Manual recording", "manual", 0.0, "");
}

bool Recorder::stopManualRecording() {
    std::lock_guard<std::mutex> lock(m_recordingMutex);

    if (!m_isManualRecording.load()) {
        return false;
    }

    m_isManualRecording.store(false);
    stopRecording();
    return true;
}

bool Recorder::isRecording() const {
    return m_isRecording.load();
}

void Recorder::triggerEventRecording(const std::string& eventType, double confidence,
                                    const std::string& metadata) {
    std::lock_guard<std::mutex> lock(m_recordingMutex);

    if (m_isRecording.load()) {
        LOG_INFO() << "[Recorder] Already recording, ignoring event trigger";
        return;
    }

    m_eventTriggerTime = std::chrono::steady_clock::now();
    startRecording("Event triggered: " + eventType, eventType, confidence, metadata);
}

bool Recorder::startRecording(const std::string& reason, const std::string& eventType,
                             double confidence, const std::string& metadata) {
    // Generate output path
    m_currentOutputPath = generateOutputPath(eventType);
    m_currentEventType = eventType;
    m_currentConfidence = confidence;
    m_currentMetadata = metadata;

    // Initialize video writer
    cv::Size frameSize(1920, 1080); // Default size, should be configurable
    int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');
    double fps = 25.0;

    if (!m_videoWriter.open(m_currentOutputPath, fourcc, fps, frameSize)) {
        LOG_ERROR() << "[Recorder] Failed to open video writer: " << m_currentOutputPath;
        return false;
    }

    m_recordingStartTime = std::chrono::steady_clock::now();
    m_isRecording.store(true);

    LOG_INFO() << "[Recorder] Started recording: " << reason
              << " -> " << m_currentOutputPath;

    // Write pre-event frames from circular buffer
    {
        std::lock_guard<std::mutex> bufferLock(m_bufferMutex);

        // Write frames in chronological order
        size_t startIdx = m_frameBuffer.size() < m_maxBufferSize ? 0 : m_bufferIndex;
        size_t count = std::min(m_frameBuffer.size(), m_maxBufferSize);

        for (size_t i = 0; i < count; ++i) {
            size_t idx = (startIdx + i) % m_frameBuffer.size();
            writeFrameToVideo(m_frameBuffer[idx]);
        }
    }

    return true;
}

void Recorder::stopRecording() {
    if (!m_isRecording.load()) {
        return;
    }

    m_isRecording.store(false);

    // Close video writer
    if (m_videoWriter.isOpened()) {
        m_videoWriter.release();
    }

    // Save event to database
    if (!m_currentEventType.empty() && m_dbManager) {
        saveEventToDatabase(m_currentOutputPath, m_currentEventType,
                          m_currentConfidence, m_currentMetadata);
    }

    LOG_INFO() << "[Recorder] Recording stopped: " << m_currentOutputPath;

    // Reset state
    m_currentOutputPath.clear();
    m_currentEventType.clear();
    m_currentConfidence = 0.0;
    m_currentMetadata.clear();
}

void Recorder::writeFrameToVideo(const FrameData& frameData) {
    if (!m_videoWriter.isOpened()) {
        return;
    }

    cv::Mat outputFrame = frameData.frame.clone();

    // Add timestamp overlay
    if (m_config.enableTimestamp) {
        addTimestampOverlay(outputFrame, frameData.timestamp);
    }

    // Add bounding box overlay
    if (m_config.enableBBoxOverlay && !frameData.detections.empty()) {
        addBBoxOverlay(outputFrame, frameData.detections, frameData.labels);
    }

    // Write frame to video
    m_videoWriter.write(outputFrame);
}

void Recorder::addTimestampOverlay(cv::Mat& frame, const std::string& timestamp) {
    // Add timestamp in bottom-left corner
    cv::Point textPos(10, frame.rows - 10);
    cv::Scalar textColor(255, 255, 255); // White text
    cv::Scalar bgColor(0, 0, 0);         // Black background

    // Get text size for background rectangle
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.6;
    int thickness = 1;
    int baseline = 0;

    cv::Size textSize = cv::getTextSize(timestamp, fontFace, fontScale, thickness, &baseline);

    // Draw background rectangle
    cv::Point bgTopLeft(textPos.x - 2, textPos.y - textSize.height - 2);
    cv::Point bgBottomRight(textPos.x + textSize.width + 2, textPos.y + baseline + 2);
    cv::rectangle(frame, bgTopLeft, bgBottomRight, bgColor, cv::FILLED);

    // Draw text
    cv::putText(frame, timestamp, textPos, fontFace, fontScale, textColor, thickness);
}

void Recorder::addBBoxOverlay(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                             const std::vector<std::string>& labels) {
    for (size_t i = 0; i < detections.size(); ++i) {
        const cv::Rect& bbox = detections[i];

        // Draw bounding box
        cv::Scalar boxColor(0, 255, 0); // Green
        cv::rectangle(frame, bbox, boxColor, 2);

        // Draw label if available
        if (i < labels.size() && !labels[i].empty()) {
            cv::Point labelPos(bbox.x, bbox.y - 5);
            cv::Scalar textColor(255, 255, 255);
            cv::Scalar bgColor(0, 255, 0);

            int fontFace = cv::FONT_HERSHEY_SIMPLEX;
            double fontScale = 0.5;
            int thickness = 1;
            int baseline = 0;

            cv::Size textSize = cv::getTextSize(labels[i], fontFace, fontScale, thickness, &baseline);

            // Draw background rectangle for label
            cv::Point bgTopLeft(labelPos.x, labelPos.y - textSize.height);
            cv::Point bgBottomRight(labelPos.x + textSize.width, labelPos.y + baseline);
            cv::rectangle(frame, bgTopLeft, bgBottomRight, bgColor, cv::FILLED);

            // Draw label text
            cv::putText(frame, labels[i], labelPos, fontFace, fontScale, textColor, thickness);
        }
    }
}

std::string Recorder::generateOutputPath(const std::string& eventType) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    std::stringstream ss;
    ss << m_config.outputDir << "/";
    ss << m_sourceId << "_";
    ss << eventType << "_";
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    ss << ".mp4";

    return ss.str();
}

bool Recorder::saveEventToDatabase(const std::string& videoPath, const std::string& eventType,
                                  double confidence, const std::string& metadata) {
    if (!m_dbManager) {
        LOG_INFO() << "[Recorder] No database manager available";
        return false;
    }

    EventRecord event(m_sourceId, eventType, videoPath, confidence);
    event.metadata = metadata;

    if (m_dbManager->insertEvent(event)) {
        LOG_INFO() << "[Recorder] Event saved to database: " << eventType
                  << " for camera " << m_sourceId;
        return true;
    } else {
        LOG_ERROR() << "[Recorder] Failed to save event to database: "
                  << m_dbManager->getErrorMessage();
        return false;
    }
}

void Recorder::updateConfig(const RecordingConfig& config) {
    setConfig(config);
}

RecordingConfig Recorder::getConfig() const {
    std::lock_guard<std::mutex> lock(m_recordingMutex);
    return m_config;
}

size_t Recorder::getBufferSize() const {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    return m_frameBuffer.size();
}

std::string Recorder::getCurrentRecordingPath() const {
    std::lock_guard<std::mutex> lock(m_recordingMutex);
    return m_currentOutputPath;
}
