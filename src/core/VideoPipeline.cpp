#include "VideoPipeline.h"
#include "TaskManager.h"
#include "../video/FFmpegDecoder.h"
#include "../ai/YOLOv8Detector.h"
#include "../ai/ByteTracker.h"
#include "../recognition/FaceRecognizer.h"
#include "../recognition/LicensePlateRecognizer.h"
#include "../ai/BehaviorAnalyzer.h"
#include "../output/Recorder.h"
#include "../output/Streamer.h"
#include "../output/AlarmTrigger.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <functional>

VideoPipeline::VideoPipeline(const VideoSource& source)
    : m_source(source) {
    std::cout << "[VideoPipeline] Creating pipeline for: " << source.id << std::endl;
}

VideoPipeline::~VideoPipeline() {
    stop();
}

bool VideoPipeline::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        std::cout << "[VideoPipeline] Initializing pipeline: " << m_source.id << std::endl;

        // Initialize decoder
        m_decoder = std::make_unique<FFmpegDecoder>();
        if (!m_decoder->initialize(m_source)) {
            handleError("Failed to initialize decoder");
            return false;
        }

        // Initialize AI modules
        m_detector = std::make_unique<YOLOv8Detector>();
        if (!m_detector->initialize()) {
            handleError("Failed to initialize YOLOv8 detector");
            return false;
        }

        m_tracker = std::make_unique<ByteTracker>();
        if (!m_tracker->initialize()) {
            handleError("Failed to initialize ByteTracker");
            return false;
        }

        // Initialize recognition modules
        m_faceRecognizer = std::make_unique<FaceRecognizer>();
        if (!m_faceRecognizer->initialize()) {
            std::cout << "[VideoPipeline] Warning: Face recognizer initialization failed" << std::endl;
        }

        m_plateRecognizer = std::make_unique<LicensePlateRecognizer>();
        if (!m_plateRecognizer->initialize()) {
            std::cout << "[VideoPipeline] Warning: License plate recognizer initialization failed" << std::endl;
        }

        // Initialize behavior analyzer
        m_behaviorAnalyzer = std::make_unique<BehaviorAnalyzer>();
        if (!m_behaviorAnalyzer->initialize()) {
            handleError("Failed to initialize behavior analyzer");
            return false;
        }

        // Initialize output modules
        m_recorder = std::make_unique<Recorder>();
        m_streamer = std::make_unique<Streamer>();
        m_alarmTrigger = std::make_unique<AlarmTrigger>();

        // Configure streamer with appropriate settings
        StreamConfig streamConfig;
        streamConfig.width = 640;
        streamConfig.height = 480;
        streamConfig.fps = 15;
        streamConfig.quality = 80;
        streamConfig.port = 8000 + std::hash<std::string>{}(m_source.id) % 1000; // Unique port per source
        streamConfig.enableOverlays = true;
        m_streamer->setConfig(streamConfig);

        if (!m_recorder->initialize(m_source.id) ||
            !m_streamer->initialize(m_source.id) ||
            !m_alarmTrigger->initialize()) {
            handleError("Failed to initialize output modules");
            return false;
        }

        // Enable streaming by default
        m_streamingEnabled.store(true);

        std::cout << "[VideoPipeline] MJPEG stream available at: " << m_streamer->getStreamUrl() << std::endl;

        std::cout << "[VideoPipeline] Pipeline initialized successfully: " << m_source.id << std::endl;
        return true;

    } catch (const std::exception& e) {
        handleError("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

void VideoPipeline::start() {
    if (m_running.load()) {
        std::cout << "[VideoPipeline] Pipeline already running: " << m_source.id << std::endl;
        return;
    }

    m_running.store(true);
    m_healthy.store(true);
    m_startTime = std::chrono::steady_clock::now();

    m_processingThread = std::thread(&VideoPipeline::processingThread, this);

    std::cout << "[VideoPipeline] Pipeline started: " << m_source.id << std::endl;
}

void VideoPipeline::stop() {
    if (!m_running.load()) {
        return;
    }

    std::cout << "[VideoPipeline] Stopping pipeline: " << m_source.id << std::endl;

    m_running.store(false);

    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    std::cout << "[VideoPipeline] Pipeline stopped: " << m_source.id << std::endl;
}

bool VideoPipeline::isRunning() const {
    return m_running.load();
}

bool VideoPipeline::isHealthy() const {
    return m_healthy.load();
}

void VideoPipeline::processingThread() {
    std::cout << "[VideoPipeline] Processing thread started: " << m_source.id << std::endl;

    cv::Mat frame;
    int64_t timestamp;
    int reconnectAttempts = 0;

    while (m_running.load()) {
        try {
            // Decode frame
            if (!m_decoder->getNextFrame(frame, timestamp)) {
                if (shouldReconnect() && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
                    std::cout << "[VideoPipeline] Attempting reconnection: " << m_source.id
                              << " (attempt " << (reconnectAttempts + 1) << ")" << std::endl;

                    attemptReconnection();
                    reconnectAttempts++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                    continue;
                } else {
                    handleError("Failed to decode frame, max reconnect attempts reached");
                    break;
                }
            }

            // Reset reconnect counter on successful frame
            reconnectAttempts = 0;

            // Process frame through pipeline
            processFrame(frame, timestamp);

            // Update statistics
            m_processedFrames.fetch_add(1);

            // Calculate frame rate
            auto now = std::chrono::steady_clock::now();
            if (m_lastFrameTime.time_since_epoch().count() > 0) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime);
                if (duration.count() > 0) {
                    m_frameRate.store(1000.0 / duration.count());
                }
            }
            m_lastFrameTime = now;

        } catch (const std::exception& e) {
            handleError("Exception in processing thread: " + std::string(e.what()));
            m_droppedFrames.fetch_add(1);
        }
    }

    std::cout << "[VideoPipeline] Processing thread stopped: " << m_source.id << std::endl;
}

void VideoPipeline::processFrame(const cv::Mat& frame, int64_t timestamp) {
    if (frame.empty()) {
        return;
    }

    FrameResult result;
    result.frame = frame.clone();
    result.timestamp = timestamp;

    // Object detection
    if (m_detectionEnabled.load() && m_detector) {
        result.detections = m_detector->detect(frame);
    }

    // Object tracking
    if (m_tracker && !result.detections.empty()) {
        result.trackIds = m_tracker->update(result.detections);
    }

    // Face recognition
    if (m_faceRecognizer) {
        result.faceIds = m_faceRecognizer->recognize(frame, result.detections);
    }

    // License plate recognition
    if (m_plateRecognizer) {
        result.plateNumbers = m_plateRecognizer->recognize(frame, result.detections);
    }

    // Behavior analysis
    if (m_behaviorAnalyzer) {
        result.events = m_behaviorAnalyzer->analyze(frame, result.detections, result.trackIds);
        result.hasAlarm = !result.events.empty();
    }

    // Output processing
    if (m_recordingEnabled.load() && m_recorder) {
        m_recorder->processFrame(result);
    }

    if (m_streamingEnabled.load() && m_streamer) {
        m_streamer->processFrame(result);
    }

    if (result.hasAlarm && m_alarmTrigger) {
        m_alarmTrigger->triggerAlarm(result);
    }
}

void VideoPipeline::handleError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = error;
    m_healthy.store(false);
    std::cerr << "[VideoPipeline] Error in " << m_source.id << ": " << error << std::endl;
}

bool VideoPipeline::shouldReconnect() const {
    // Implement reconnection logic based on error type
    return true;
}

void VideoPipeline::attemptReconnection() {
    if (m_decoder) {
        m_decoder->reconnect();
    }
}

// Getters
double VideoPipeline::getFrameRate() const {
    return m_frameRate.load();
}

size_t VideoPipeline::getProcessedFrames() const {
    return m_processedFrames.load();
}

size_t VideoPipeline::getDroppedFrames() const {
    return m_droppedFrames.load();
}

std::string VideoPipeline::getLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// Configuration setters
void VideoPipeline::setDetectionEnabled(bool enabled) {
    m_detectionEnabled.store(enabled);
}

void VideoPipeline::setRecordingEnabled(bool enabled) {
    m_recordingEnabled.store(enabled);
}

void VideoPipeline::setStreamingEnabled(bool enabled) {
    m_streamingEnabled.store(enabled);
}

// Access methods
const VideoSource& VideoPipeline::getSource() const {
    return m_source;
}

std::chrono::steady_clock::time_point VideoPipeline::getStartTime() const {
    return m_startTime;
}

// VideoSource implementation
bool VideoSource::isValid() const {
    if (id.empty() || url.empty()) {
        return false;
    }

    if (protocol != "rtsp" && protocol != "onvif" && protocol != "gb28181") {
        return false;
    }

    if (width <= 0 || height <= 0 || fps <= 0) {
        return false;
    }

    return true;
}

std::string VideoSource::toString() const {
    std::ostringstream oss;
    oss << "VideoSource{id=" << id
        << ", protocol=" << protocol
        << ", url=" << url
        << ", resolution=" << width << "x" << height
        << ", fps=" << fps
        << ", enabled=" << (enabled ? "true" : "false") << "}";
    return oss.str();
}
