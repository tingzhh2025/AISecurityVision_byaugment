#include "VideoPipeline.h"
#include "TaskManager.h"
#include "../video/FFmpegDecoder.h"
#include "../ai/YOLOv8Detector.h"
#include "../ai/YOLOv8RKNNDetector.h"
#include "../ai/ByteTracker.h"
#include "../ai/ReIDExtractor.h"
#include "../recognition/FaceRecognizer.h"
#include "../recognition/LicensePlateRecognizer.h"
#include "../ai/BehaviorAnalyzer.h"
#include "../output/Recorder.h"
#include "../output/Streamer.h"
#include "../output/AlarmTrigger.h"
// Person statistics extensions (optional)
#include "../ai/PersonFilter.h"
#include "../ai/AgeGenderAnalyzer.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <functional>

#include "../core/Logger.h"
using namespace AISecurityVision;
VideoPipeline::VideoPipeline(const VideoSource& source)
    : m_source(source) {
    LOG_INFO() << "[VideoPipeline] Creating pipeline for: " << source.id;

    // Initialize health monitoring timestamps
    auto now = std::chrono::steady_clock::now();
    m_lastFrameTime = now;
    m_lastHealthCheck = now;
    m_startTime = now;
}

VideoPipeline::~VideoPipeline() {
    stop();
}

bool VideoPipeline::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        LOG_INFO() << "[VideoPipeline] Initializing pipeline: " << m_source.id;

        // Initialize decoder
        m_decoder = std::make_unique<FFmpegDecoder>();
        if (!m_decoder->initialize(m_source)) {
            handleError("Failed to initialize decoder");
            return false;
        }

        // Initialize AI modules - choose between optimized and standard detector
        if (m_optimizedDetectionEnabled.load()) {
            LOG_INFO() << "[VideoPipeline] Initializing RKNN YOLOv8 detector...";

            m_optimizedDetector = std::make_unique<AISecurityVision::YOLOv8RKNNDetector>();
            if (!m_optimizedDetector->initialize("models/yolov8n.rknn")) {
                LOG_ERROR() << "[VideoPipeline] Failed to initialize RKNN detector, falling back to standard detector";
                m_optimizedDetectionEnabled.store(false);

                // Fallback to standard detector - create RKNN detector directly
                m_detector = std::make_unique<AISecurityVision::YOLOv8RKNNDetector>();
                if (!m_detector || !m_detector->initialize("models/yolov8n.rknn")) {
                    handleError("Failed to initialize YOLOv8 detector");
                    return false;
                }
            } else {
                LOG_INFO() << "[VideoPipeline] RKNN YOLOv8 detector initialized successfully!";
                // Enable multi-core NPU for better performance
                auto rknnDetector = static_cast<AISecurityVision::YOLOv8RKNNDetector*>(m_optimizedDetector.get());
                rknnDetector->enableMultiCore(true);
                rknnDetector->setZeroCopyMode(true);
            }
        } else {
            m_detector = std::make_unique<AISecurityVision::YOLOv8RKNNDetector>();
            if (!m_detector || !m_detector->initialize("models/yolov8n.rknn")) {
                handleError("Failed to initialize YOLOv8 detector");
                return false;
            }
        }

        // Load saved detection categories from database
        try {
            DatabaseManager dbManager;
            if (dbManager.initialize()) {
                std::vector<std::string> savedCategories = dbManager.getDetectionCategories();
                LOG_INFO() << "[VideoPipeline] Retrieved " << savedCategories.size() << " saved detection categories";
                if (!savedCategories.empty()) {
                    LOG_INFO() << "[VideoPipeline] About to call updateDetectionCategories...";
                    updateDetectionCategoriesInternal(savedCategories);
                    LOG_INFO() << "[VideoPipeline] Loaded " << savedCategories.size()
                               << " saved detection categories for " << m_source.id;
                }
            }
        } catch (const std::exception& e) {
            LOG_WARN() << "[VideoPipeline] Failed to load saved detection categories: " << e.what();
        }

        LOG_INFO() << "[VideoPipeline] About to initialize ByteTracker...";
        m_tracker = std::make_unique<ByteTracker>();
        LOG_INFO() << "[VideoPipeline] ByteTracker object created, calling initialize()...";
        if (!m_tracker->initialize()) {
            handleError("Failed to initialize ByteTracker");
            return false;
        }
        LOG_INFO() << "[VideoPipeline] ByteTracker initialized successfully!";

        // Initialize ReID extractor
        LOG_INFO() << "[VideoPipeline] About to initialize ReIDExtractor...";
        m_reidExtractor = std::make_unique<ReIDExtractor>();
        LOG_INFO() << "[VideoPipeline] ReIDExtractor object created, calling initialize()...";
        if (!m_reidExtractor->initialize()) {
            handleError("Failed to initialize ReID extractor");
            return false;
        }
        LOG_INFO() << "[VideoPipeline] ReIDExtractor initialized successfully!";

        // Enable ReID tracking in ByteTracker
        m_tracker->enableReIDTracking(true);
        m_tracker->setReIDSimilarityThreshold(0.7f);
        m_tracker->setReIDWeight(0.3f);

        // Initialize recognition modules
        m_faceRecognizer = std::make_unique<FaceRecognizer>();
        if (!m_faceRecognizer->initialize()) {
            LOG_ERROR() << "[VideoPipeline] Warning: Face recognizer initialization failed";
        }

        m_plateRecognizer = std::make_unique<LicensePlateRecognizer>();
        if (!m_plateRecognizer->initialize()) {
            LOG_ERROR() << "[VideoPipeline] Warning: License plate recognizer initialization failed";
        }

        // Initialize behavior analyzer
        m_behaviorAnalyzer = std::make_unique<BehaviorAnalyzer>();
        if (!m_behaviorAnalyzer->initialize()) {
            handleError("Failed to initialize behavior analyzer");
            return false;
        }

        // Task 77: Set camera ID for cross-camera tracking
        m_behaviorAnalyzer->setCameraId(m_source.id);

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
        streamConfig.port = m_source.mjpeg_port; // Use configured MJPEG port
        streamConfig.enableOverlays = true;
        m_streamer->setConfig(streamConfig);

        LOG_INFO() << "[VideoPipeline] Configured MJPEG stream for " << m_source.id
                  << " on port " << streamConfig.port;

        if (!m_recorder->initialize(m_source.id) ||
            !m_streamer->initialize(m_source.id) ||
            !m_alarmTrigger->initialize()) {
            handleError("Failed to initialize output modules");
            return false;
        }

        // Enable streaming by default
        m_streamingEnabled.store(true);

        LOG_INFO() << "[VideoPipeline] MJPEG stream available at: " << m_streamer->getStreamUrl();

        LOG_INFO() << "[VideoPipeline] Pipeline initialized successfully: " << m_source.id;
        return true;

    } catch (const std::exception& e) {
        handleError("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

void VideoPipeline::start() {
    if (m_running.load()) {
        LOG_INFO() << "[VideoPipeline] Pipeline already running: " << m_source.id;
        return;
    }

    m_running.store(true);
    m_healthy.store(true);
    m_startTime = std::chrono::steady_clock::now();

    m_processingThread = std::thread(&VideoPipeline::processingThread, this);

    LOG_INFO() << "[VideoPipeline] Pipeline started: " << m_source.id;
}

void VideoPipeline::stop() {
    if (!m_running.load()) {
        return;
    }

    LOG_INFO() << "[VideoPipeline] Stopping pipeline: " << m_source.id;

    m_running.store(false);

    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    LOG_INFO() << "[VideoPipeline] Pipeline stopped: " << m_source.id;
}

bool VideoPipeline::isRunning() const {
    return m_running.load();
}

bool VideoPipeline::isHealthy() const {
    return m_healthy.load();
}

void VideoPipeline::processingThread() {
    LOG_INFO() << "[VideoPipeline] Processing thread started: " << m_source.id;

    cv::Mat frame;
    int64_t timestamp;
    int reconnectAttempts = 0;

    while (m_running.load()) {
        try {
            // Check stream health periodically
            checkStreamHealth();

            // Decode frame
            if (!m_decoder->getNextFrame(frame, timestamp)) {
                m_consecutiveErrors.fetch_add(1);

                if (shouldReconnect() && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
                    LOG_INFO() << "[VideoPipeline] Attempting reconnection: " << m_source.id
                              << " (attempt " << (reconnectAttempts + 1) << ")";

                    attemptReconnection();
                    reconnectAttempts++;
                    m_totalReconnects.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
                    continue;
                } else {
                    handleError("Failed to decode frame, max reconnect attempts reached");
                    break;
                }
            }

            // Reset reconnect counter and error count on successful frame
            reconnectAttempts = 0;
            m_consecutiveErrors.store(0);

            // Update health metrics
            updateHealthMetrics();

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

    LOG_INFO() << "[VideoPipeline] Processing thread stopped: " << m_source.id;
}

void VideoPipeline::processFrame(const cv::Mat& frame, int64_t timestamp) {
    if (frame.empty()) {
        return;
    }

    FrameResult result;
    result.frame = frame.clone();
    result.timestamp = timestamp;

    // Object detection - use optimized detector if available
    if (m_detectionEnabled.load()) {
        std::vector<AISecurityVision::Detection> detectionResults;

        if (m_optimizedDetectionEnabled.load() && m_optimizedDetector) {
            // Use optimized RKNN detector
            detectionResults = m_optimizedDetector->detectObjects(frame);
        } else if (m_detector) {
            // Use standard detector
            detectionResults = m_detector->detectObjects(frame);
        }

        // Extract bounding boxes and class information
        for (const auto& detection : detectionResults) {
            result.detections.push_back(detection.bbox);
            result.labels.push_back(detection.className);
        }

        // Extract confidences and class IDs for tracking
        std::vector<float> confidences;
        std::vector<int> classIds;
        for (const auto& detection : detectionResults) {
            confidences.push_back(detection.confidence);
            classIds.push_back(detection.classId);
        }

        // ReID feature extraction
        if (m_reidExtractor && !result.detections.empty()) {
            auto reidEmbeddings = m_reidExtractor->extractFeatures(
                frame, result.detections, {}, classIds, confidences);

            // Extract feature vectors for tracking
            std::vector<std::vector<float>> reidFeatures;
            for (const auto& embedding : reidEmbeddings) {
                reidFeatures.push_back(embedding.features);
                result.reidEmbeddings.push_back(embedding.features);
            }

            // Object tracking with ReID features
            if (m_tracker) {
                result.trackIds = m_tracker->updateWithReIDFeatures(
                    result.detections, confidences, classIds, reidFeatures);

                // Task 75: Report track updates to TaskManager for cross-camera tracking
                TaskManager& taskManager = TaskManager::getInstance();
                result.globalTrackIds.resize(result.trackIds.size(), -1);

                for (size_t i = 0; i < result.trackIds.size() && i < reidFeatures.size(); ++i) {
                    if (result.trackIds[i] >= 0 && !reidFeatures[i].empty()) {
                        // Report track update to TaskManager
                        taskManager.reportTrackUpdate(
                            m_source.id, result.trackIds[i], reidFeatures[i],
                            result.detections[i], classIds[i], confidences[i]);

                        // Get global track ID
                        result.globalTrackIds[i] = taskManager.getGlobalTrackId(m_source.id, result.trackIds[i]);
                    }
                }

                LOG_INFO() << "[VideoPipeline] Processed " << result.detections.size()
                          << " detections with " << reidEmbeddings.size()
                          << " ReID embeddings (dim="
                          << (reidEmbeddings.empty() ? 0 : reidEmbeddings[0].getDimension())
                          << "), global tracks: " << result.globalTrackIds.size();
            }
        } else {
            // Fallback to regular tracking without ReID
            if (m_tracker) {
                result.trackIds = m_tracker->updateWithClasses(result.detections, confidences, classIds);

                // Initialize global track IDs as empty for non-ReID tracking
                result.globalTrackIds.resize(result.trackIds.size(), -1);
            }
        }
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

        // Task 73: Include active ROIs for visualization
        result.activeROIs = m_behaviorAnalyzer->getActiveROIs();
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

    // Person statistics processing (optional, backward compatible)
    if (m_personStatsEnabled.load() && !result.detections.empty()) {
        processPersonStatistics(result);
    }
}

void VideoPipeline::processPersonStatistics(FrameResult& result) {
    try {
        // PersonFilter is a static utility class, no initialization needed

        if (!m_ageGenderAnalyzer) {
            m_ageGenderAnalyzer = std::make_unique<AISecurityVision::AgeGenderAnalyzer>();
            if (!m_ageGenderAnalyzer->initialize()) {
                LOG_WARN() << "[VideoPipeline] Failed to initialize AgeGenderAnalyzer, disabling person statistics";
                m_personStatsEnabled.store(false);
                return;
            }
        }

        // Convert YOLOv8 detections to Detection format for PersonFilter
        std::vector<AISecurityVision::Detection> detections;
        for (size_t i = 0; i < result.detections.size() && i < result.labels.size(); ++i) {
            AISecurityVision::Detection detection;
            detection.bbox = result.detections[i];
            detection.confidence = 0.8f;  // Default confidence if not available
            detection.className = result.labels[i];

            // Map class name to class ID (person = 0 in COCO)
            if (result.labels[i] == "person") {
                detection.classId = 0;
            } else {
                detection.classId = -1;  // Non-person class
            }

            detections.push_back(detection);
        }

        // Filter person detections
        auto persons = AISecurityVision::PersonFilter::filterPersons(
            detections, result.frame, result.trackIds, result.timestamp
        );

        if (persons.empty()) {
            // No persons detected, reset statistics
            result.personStats = FrameResult::PersonStats();
            return;
        }

        // Analyze age and gender
        auto attributes = m_ageGenderAnalyzer->analyze(persons);

        // Update person statistics
        result.personStats.total_persons = static_cast<int>(persons.size());
        result.personStats.male_count = 0;
        result.personStats.female_count = 0;
        result.personStats.child_count = 0;
        result.personStats.young_count = 0;
        result.personStats.middle_count = 0;
        result.personStats.senior_count = 0;

        // Clear previous data
        result.personStats.person_boxes.clear();
        result.personStats.person_genders.clear();
        result.personStats.person_ages.clear();

        // Process each person
        for (size_t i = 0; i < persons.size(); ++i) {
            const auto& person = persons[i];
            result.personStats.person_boxes.push_back(person.bbox);

            // Add attributes if available
            if (i < attributes.size() && attributes[i].isValid()) {
                const auto& attr = attributes[i];

                // Count by gender
                if (attr.gender == "male") {
                    result.personStats.male_count++;
                } else if (attr.gender == "female") {
                    result.personStats.female_count++;
                }

                // Count by age group
                if (attr.age_group == "child") {
                    result.personStats.child_count++;
                } else if (attr.age_group == "young") {
                    result.personStats.young_count++;
                } else if (attr.age_group == "middle") {
                    result.personStats.middle_count++;
                } else if (attr.age_group == "senior") {
                    result.personStats.senior_count++;
                }

                result.personStats.person_genders.push_back(attr.gender);
                result.personStats.person_ages.push_back(attr.age_group);
            } else {
                result.personStats.person_genders.push_back("unknown");
                result.personStats.person_ages.push_back("unknown");
            }
        }

        LOG_DEBUG() << "[VideoPipeline] Person statistics: "
                   << result.personStats.total_persons << " total, "
                   << result.personStats.male_count << " male, "
                   << result.personStats.female_count << " female";

        // Update current person statistics for API access
        {
            std::lock_guard<std::mutex> lock(m_personStatsMutex);
            // Convert FrameResult::PersonStats to VideoPipeline::PersonStats
            m_currentPersonStats.total_persons = result.personStats.total_persons;
            m_currentPersonStats.male_count = result.personStats.male_count;
            m_currentPersonStats.female_count = result.personStats.female_count;
            m_currentPersonStats.child_count = result.personStats.child_count;
            m_currentPersonStats.young_count = result.personStats.young_count;
            m_currentPersonStats.middle_count = result.personStats.middle_count;
            m_currentPersonStats.senior_count = result.personStats.senior_count;
            m_currentPersonStats.person_boxes = result.personStats.person_boxes;
            m_currentPersonStats.person_genders = result.personStats.person_genders;
            m_currentPersonStats.person_ages = result.personStats.person_ages;
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[VideoPipeline] Error in person statistics processing: " << e.what();
        result.personStats = FrameResult::PersonStats();  // Reset to default
    }
}

void VideoPipeline::handleError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = error;
    m_healthy.store(false);
    LOG_ERROR() << "[VideoPipeline] Error in " << m_source.id << ": " << error;
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

// AI Detection configuration methods
void VideoPipeline::setOptimizedDetectionEnabled(bool enabled) {
    m_optimizedDetectionEnabled.store(enabled);
    LOG_INFO() << "[VideoPipeline] Optimized detection "
              << (enabled ? "enabled" : "disabled")
              << " for pipeline: " << m_source.id;
}

bool VideoPipeline::isOptimizedDetectionEnabled() const {
    return m_optimizedDetectionEnabled.load();
}

void VideoPipeline::setDetectionThreads(int threads) {
    if (threads > 0 && threads <= 8) {  // Reasonable limits
        m_detectionThreads.store(threads);
        LOG_INFO() << "[VideoPipeline] Detection threads set to " << threads
                  << " for pipeline: " << m_source.id;
    }
}

int VideoPipeline::getDetectionThreads() const {
    return m_detectionThreads.load();
}

// Internal version without mutex lock (for use during initialization)
bool VideoPipeline::updateDetectionCategoriesInternal(const std::vector<std::string>& enabledCategories) {
    LOG_INFO() << "[VideoPipeline] updateDetectionCategoriesInternal called with " << enabledCategories.size() << " categories";

    bool success = true;

    // Update standard detector if available
    if (m_detector) {
        LOG_INFO() << "[VideoPipeline] Updating standard detector categories...";
        m_detector->setEnabledCategories(enabledCategories);
        LOG_INFO() << "[VideoPipeline] Updated standard detector categories for " << m_source.id;
    } else {
        LOG_INFO() << "[VideoPipeline] Standard detector not available";
    }

    // Update optimized detector if available
    if (m_optimizedDetector) {
        LOG_INFO() << "[VideoPipeline] Updating optimized detector categories...";
        m_optimizedDetector->setEnabledCategories(enabledCategories);
        LOG_INFO() << "[VideoPipeline] Updated optimized detector categories for " << m_source.id;
    } else {
        LOG_INFO() << "[VideoPipeline] Optimized detector not available";
    }

    if (!m_detector && !m_optimizedDetector) {
        LOG_WARN() << "[VideoPipeline] No detectors available to update for " << m_source.id;
        success = false;
    }

    return success;
}

// Detection category filtering implementation
bool VideoPipeline::updateDetectionCategories(const std::vector<std::string>& enabledCategories) {
    LOG_INFO() << "[VideoPipeline] updateDetectionCategories called with " << enabledCategories.size() << " categories";
    std::lock_guard<std::mutex> lock(m_mutex);
    LOG_INFO() << "[VideoPipeline] Acquired mutex lock in updateDetectionCategories";

    bool success = true;

    // Update standard detector if available
    if (m_detector) {
        LOG_INFO() << "[VideoPipeline] Updating standard detector categories...";
        m_detector->setEnabledCategories(enabledCategories);
        LOG_INFO() << "[VideoPipeline] Updated standard detector categories for " << m_source.id;
    } else {
        LOG_INFO() << "[VideoPipeline] Standard detector not available";
    }

    // Update optimized detector if available
    if (m_optimizedDetector) {
        LOG_INFO() << "[VideoPipeline] Updating optimized detector categories...";
        m_optimizedDetector->setEnabledCategories(enabledCategories);
        LOG_INFO() << "[VideoPipeline] Updated optimized detector categories for " << m_source.id;
    } else {
        LOG_INFO() << "[VideoPipeline] Optimized detector not available";
    }

    if (!m_detector && !m_optimizedDetector) {
        LOG_WARN() << "[VideoPipeline] No detectors available to update for " << m_source.id;
        success = false;
    }

    return success;
}

// Behavior analysis rule management implementation
bool VideoPipeline::addIntrusionRule(const IntrusionRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return false;
    }

    bool success = m_behaviorAnalyzer->addIntrusionRule(rule);
    if (success) {
        LOG_INFO() << "[VideoPipeline] Added intrusion rule: " << rule.id
                  << " to pipeline: " << m_source.id;
    }

    return success;
}

bool VideoPipeline::removeIntrusionRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return false;
    }

    bool success = m_behaviorAnalyzer->removeIntrusionRule(ruleId);
    if (success) {
        LOG_INFO() << "[VideoPipeline] Removed intrusion rule: " << ruleId
                  << " from pipeline: " << m_source.id;
    }

    return success;
}

bool VideoPipeline::updateIntrusionRule(const IntrusionRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return false;
    }

    bool success = m_behaviorAnalyzer->updateIntrusionRule(rule);
    if (success) {
        LOG_INFO() << "[VideoPipeline] Updated intrusion rule: " << rule.id
                  << " in pipeline: " << m_source.id;
    }

    return success;
}

std::vector<IntrusionRule> VideoPipeline::getIntrusionRules() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return {};
    }

    return m_behaviorAnalyzer->getIntrusionRules();
}

bool VideoPipeline::addROI(const ROI& roi) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return false;
    }

    bool success = m_behaviorAnalyzer->addROI(roi);
    if (success) {
        LOG_INFO() << "[VideoPipeline] Added ROI: " << roi.id
                  << " to pipeline: " << m_source.id;
    }

    return success;
}

bool VideoPipeline::removeROI(const std::string& roiId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return false;
    }

    bool success = m_behaviorAnalyzer->removeROI(roiId);
    if (success) {
        LOG_INFO() << "[VideoPipeline] Removed ROI: " << roiId
                  << " from pipeline: " << m_source.id;
    }

    return success;
}

std::vector<ROI> VideoPipeline::getROIs() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_behaviorAnalyzer) {
        LOG_ERROR() << "[VideoPipeline] BehaviorAnalyzer not initialized";
        return {};
    }

    return m_behaviorAnalyzer->getROIs();
}

// Access methods
const VideoSource& VideoPipeline::getSource() const {
    return m_source;
}

std::chrono::steady_clock::time_point VideoPipeline::getStartTime() const {
    return m_startTime;
}

// Task 76: BehaviorAnalyzer access for ReID configuration
BehaviorAnalyzer* VideoPipeline::getBehaviorAnalyzer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_behaviorAnalyzer.get();
}

// VideoSource implementation
bool VideoSource::isValid() const {
    if (id.empty() || url.empty()) {
        return false;
    }

    if (protocol != "rtsp" && protocol != "onvif" && protocol != "gb28181" &&
        protocol != "rtmp" && protocol != "http" && protocol != "file") {
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

// Streaming configuration methods
bool VideoPipeline::configureStreaming(const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        LOG_ERROR() << "[VideoPipeline] Streamer not initialized";
        return false;
    }

    try {
        // Update streamer configuration
        m_streamer->setConfig(config);

        // Restart streaming if it was running
        if (m_streamingEnabled.load()) {
            m_streamer->stopServer();
            m_streamer->stopRtmpStream();

            if (config.protocol == StreamProtocol::MJPEG) {
                if (!m_streamer->startServer()) {
                    LOG_ERROR() << "[VideoPipeline] Failed to restart MJPEG server";
                    return false;
                }
            } else if (config.protocol == StreamProtocol::RTMP) {
                if (!m_streamer->startRtmpStream()) {
                    LOG_ERROR() << "[VideoPipeline] Failed to restart RTMP stream";
                    return false;
                }
            }
        }

        LOG_INFO() << "[VideoPipeline] Streaming configured for " << m_source.id
                  << " - " << (config.protocol == StreamProtocol::MJPEG ? "MJPEG" : "RTMP")
                  << " " << config.width << "x" << config.height << "@" << config.fps << "fps";

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[VideoPipeline] Failed to configure streaming: " << e.what();
        return false;
    }
}

StreamConfig VideoPipeline::getStreamConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        return StreamConfig{}; // Return default config
    }

    return m_streamer->getConfig();
}

bool VideoPipeline::startStreaming() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        LOG_ERROR() << "[VideoPipeline] Streamer not initialized";
        return false;
    }

    if (m_streamingEnabled.load()) {
        LOG_INFO() << "[VideoPipeline] Streaming already enabled for " << m_source.id;
        return true;
    }

    try {
        StreamConfig config = m_streamer->getConfig();

        bool success = false;
        if (config.protocol == StreamProtocol::MJPEG) {
            success = m_streamer->startServer();
        } else if (config.protocol == StreamProtocol::RTMP) {
            success = m_streamer->startRtmpStream();
        }

        if (success) {
            m_streamingEnabled.store(true);
            LOG_INFO() << "[VideoPipeline] Streaming started for " << m_source.id
                      << " at " << m_streamer->getStreamUrl();
        }

        return success;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[VideoPipeline] Failed to start streaming: " << e.what();
        return false;
    }
}

bool VideoPipeline::stopStreaming() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        LOG_ERROR() << "[VideoPipeline] Streamer not initialized";
        return false;
    }

    if (!m_streamingEnabled.load()) {
        LOG_INFO() << "[VideoPipeline] Streaming already disabled for " << m_source.id;
        return true;
    }

    try {
        m_streamer->stopServer();
        m_streamer->stopRtmpStream();
        m_streamingEnabled.store(false);

        LOG_INFO() << "[VideoPipeline] Streaming stopped for " << m_source.id;
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[VideoPipeline] Failed to stop streaming: " << e.what();
        return false;
    }
}

bool VideoPipeline::isStreamingEnabled() const {
    return m_streamingEnabled.load();
}

std::string VideoPipeline::getStreamUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        return "";
    }

    return m_streamer->getStreamUrl();
}

size_t VideoPipeline::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        return 0;
    }

    return m_streamer->getConnectedClients();
}

double VideoPipeline::getStreamFps() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_streamer) {
        return 0.0;
    }

    return m_streamer->getStreamFps();
}

void VideoPipeline::updateHealthMetrics() {
    auto now = std::chrono::steady_clock::now();

    // Calculate frame interval
    if (m_lastFrameTime.time_since_epoch().count() > 0) {
        auto interval = std::chrono::duration<double>(now - m_lastFrameTime).count();

        // Update average frame interval using exponential moving average
        double currentAvg = m_avgFrameInterval.load();
        double alpha = 0.1; // Smoothing factor
        double newAvg = (currentAvg == 0.0) ? interval : (alpha * interval + (1.0 - alpha) * currentAvg);
        m_avgFrameInterval.store(newAvg);

        // Update frame rate
        if (newAvg > 0.0) {
            m_frameRate.store(1.0 / newAvg);
        }
    }

    m_lastFrameTime = now;
}

void VideoPipeline::checkStreamHealth() {
    auto now = std::chrono::steady_clock::now();

    // Only check health at specified intervals
    auto timeSinceLastCheck = std::chrono::duration<double>(now - m_lastHealthCheck).count();
    if (timeSinceLastCheck < HEALTH_CHECK_INTERVAL_S) {
        return;
    }

    m_lastHealthCheck = now;

    // Check for frame timeout
    auto timeSinceLastFrame = std::chrono::duration<double>(now - m_lastFrameTime).count();
    bool frameTimeout = timeSinceLastFrame > FRAME_TIMEOUT_S;

    // Check consecutive errors
    bool tooManyErrors = m_consecutiveErrors.load() > MAX_CONSECUTIVE_ERRORS;

    // Check frame rate stability
    double currentFrameRate = m_frameRate.load();
    double expectedFrameRate = 25.0; // Default expected frame rate
    bool frameRateStable = (currentFrameRate >= expectedFrameRate * STABLE_FRAME_RATE_THRESHOLD);

    // Update stream stability
    bool wasStable = m_streamStable.load();
    bool isStable = !frameTimeout && !tooManyErrors && frameRateStable;
    m_streamStable.store(isStable);

    // Log health status changes
    if (wasStable != isStable) {
        if (isStable) {
            LOG_INFO() << "[VideoPipeline] Stream " << m_source.id << " is now STABLE";
            m_healthy.store(true);
        } else {
            LOG_INFO() << "[VideoPipeline] Stream " << m_source.id << " is now UNSTABLE";
            LOG_INFO() << "  - Frame timeout: " << (frameTimeout ? "YES" : "NO")
                      << " (last frame: " << timeSinceLastFrame << "s ago)";
            LOG_ERROR() << "  - Too many errors: " << (tooManyErrors ? "YES" : "NO")
                      << " (consecutive: " << m_consecutiveErrors.load() << ")";
            LOG_INFO() << "  - Frame rate stable: " << (frameRateStable ? "YES" : "NO")
                      << " (current: " << currentFrameRate << " fps)";
            m_healthy.store(false);
        }
    }

    // Trigger reconnection if stream is unhealthy for too long
    if (!isStable && tooManyErrors) {
        LOG_INFO() << "[VideoPipeline] Stream " << m_source.id
                  << " requires reconnection due to health issues";
        // The main processing loop will handle reconnection
    }
}

bool VideoPipeline::isStreamStable() const {
    return m_streamStable.load();
}

// Person statistics configuration methods
void VideoPipeline::setPersonStatsEnabled(bool enabled) {
    m_personStatsEnabled.store(enabled);
    LOG_INFO() << "[VideoPipeline] Person statistics "
               << (enabled ? "enabled" : "disabled")
               << " for pipeline: " << m_source.id;
}

bool VideoPipeline::isPersonStatsEnabled() const {
    return m_personStatsEnabled.load();
}

void VideoPipeline::setPersonStatsConfig(float genderThreshold, float ageThreshold, int batchSize, bool enableCaching) {
    m_genderThreshold.store(genderThreshold);
    m_ageThreshold.store(ageThreshold);
    m_batchSize.store(batchSize);
    m_enableCaching.store(enableCaching);

    LOG_INFO() << "[VideoPipeline] Person statistics config updated for pipeline: " << m_source.id
               << " (gender_threshold=" << genderThreshold
               << ", age_threshold=" << ageThreshold
               << ", batch_size=" << batchSize
               << ", enable_caching=" << enableCaching << ")";

    // If AgeGenderAnalyzer is already initialized, update its configuration
    if (m_ageGenderAnalyzer) {
        // Note: AgeGenderAnalyzer configuration update would be implemented here
        // For now, we just log the configuration change
        LOG_DEBUG() << "[VideoPipeline] AgeGenderAnalyzer configuration will be updated on next analysis";
    }
}

VideoPipeline::PersonStats VideoPipeline::getCurrentPersonStats() const {
    std::lock_guard<std::mutex> lock(m_personStatsMutex);
    return m_currentPersonStats;
}
