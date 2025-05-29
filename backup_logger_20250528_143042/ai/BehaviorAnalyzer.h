#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <atomic>

/**
 * @brief ROI (Region of Interest) definition for behavior analysis
 */
struct ROI {
    std::string id;
    std::string name;
    std::vector<cv::Point> polygon;
    bool enabled = true;
    int priority = 1;  // 1-5 scale
    std::string start_time;  // ISO 8601 time format (HH:MM or HH:MM:SS)
    std::string end_time;    // ISO 8601 time format (HH:MM or HH:MM:SS)

    ROI() = default;
    ROI(const std::string& roiId, const std::string& roiName,
        const std::vector<cv::Point>& roiPolygon)
        : id(roiId), name(roiName), polygon(roiPolygon) {}
};

/**
 * @brief Intrusion detection rule configuration
 */
struct IntrusionRule {
    std::string id;
    ROI roi;
    double minDuration = 5.0;  // seconds
    double confidence = 0.7;
    bool enabled = true;

    IntrusionRule() = default;
    IntrusionRule(const std::string& ruleId, const ROI& ruleRoi, double duration = 5.0)
        : id(ruleId), roi(ruleRoi), minDuration(duration) {}
};

/**
 * @brief Detected behavior event
 */
struct BehaviorEvent {
    std::string eventType;
    std::string ruleId;
    std::string objectId;        // Local track ID (for backward compatibility)
    std::string reidId;          // Task 77: Global ReID track ID for cross-camera persistence
    int localTrackId = -1;       // Task 77: Local track ID as integer
    int globalTrackId = -1;      // Task 77: Global track ID as integer
    std::string cameraId;        // Task 77: Camera ID for cross-camera tracking
    cv::Rect boundingBox;
    double confidence;
    std::string timestamp;
    std::string metadata;

    BehaviorEvent() : confidence(0.0) {}
    BehaviorEvent(const std::string& type, const std::string& rule,
                 const std::string& objId, const cv::Rect& bbox, double conf)
        : eventType(type), ruleId(rule), objectId(objId), boundingBox(bbox), confidence(conf) {}

    // Task 77: Enhanced constructor with ReID information
    BehaviorEvent(const std::string& type, const std::string& rule,
                 const std::string& objId, const cv::Rect& bbox, double conf,
                 int localId, int globalId, const std::string& camId)
        : eventType(type), ruleId(rule), objectId(objId), boundingBox(bbox), confidence(conf),
          localTrackId(localId), globalTrackId(globalId), cameraId(camId) {
        // Generate ReID string from global track ID
        if (globalId >= 0) {
            reidId = "reid_" + std::to_string(globalId);
        }
    }
};

/**
 * @brief ReID matching result for cross-camera tracking
 */
struct ReIDMatchResult {
    int trackId;
    float similarity;
    std::string cameraId;
    bool isValid;

    ReIDMatchResult() : trackId(-1), similarity(0.0f), isValid(false) {}
    ReIDMatchResult(int id, float sim, const std::string& camera)
        : trackId(id), similarity(sim), cameraId(camera), isValid(true) {}
};

/**
 * @brief ReID configuration for behavior analysis
 */
struct ReIDConfig {
    bool enabled = true;
    float similarityThreshold = 0.7f;  // Configurable threshold (0.5-0.95)
    int maxMatches = 5;                // Maximum number of matches to consider
    double matchTimeout = 30.0;       // Timeout for ReID matches (seconds)
    bool crossCameraEnabled = true;   // Enable cross-camera matching

    bool isValidThreshold(float threshold) const {
        return threshold >= 0.5f && threshold <= 0.95f;
    }
};

/**
 * @brief Object tracking state for behavior analysis
 */
struct ObjectState {
    int trackId;
    cv::Point2f position;
    cv::Point2f velocity;
    std::chrono::steady_clock::time_point firstSeen;
    std::chrono::steady_clock::time_point lastSeen;
    std::vector<cv::Point2f> trajectory;
    std::map<std::string, std::chrono::steady_clock::time_point> roiEntryTimes;

    // ReID features for cross-camera tracking
    std::vector<float> reidFeatures;
    std::string cameraId;
    int globalTrackId = -1;
    std::vector<ReIDMatchResult> reidMatches;

    ObjectState() : trackId(-1), position(0, 0), velocity(0, 0) {}
    ObjectState(int id, const cv::Point2f& pos)
        : trackId(id), position(pos), velocity(0, 0) {
        firstSeen = lastSeen = std::chrono::steady_clock::now();
        trajectory.push_back(pos);
    }

    bool hasValidReIDFeatures() const {
        return !reidFeatures.empty();
    }
};

/**
 * @brief Behavior analysis engine with configurable rules
 *
 * This class provides:
 * - Intrusion detection with ROI polygons
 * - Object tracking state management
 * - Configurable behavior rules
 * - Event generation and filtering
 */
class BehaviorAnalyzer {
public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();

    // Initialization
    bool initialize();
    bool loadRulesFromJson(const std::string& jsonPath);

    // Main analysis function
    std::vector<BehaviorEvent> analyze(const cv::Mat& frame,
                                     const std::vector<cv::Rect>& detections,
                                     const std::vector<int>& trackIds);

    // Enhanced analysis function with ReID features
    std::vector<BehaviorEvent> analyzeWithReID(const cv::Mat& frame,
                                              const std::vector<cv::Rect>& detections,
                                              const std::vector<int>& trackIds,
                                              const std::vector<std::vector<float>>& reidFeatures,
                                              const std::string& cameraId = "");

    // Rule management
    bool addIntrusionRule(const IntrusionRule& rule);
    bool removeIntrusionRule(const std::string& ruleId);
    std::vector<IntrusionRule> getIntrusionRules() const;
    bool updateIntrusionRule(const IntrusionRule& rule);

    // ROI management
    bool addROI(const ROI& roi);
    bool removeROI(const std::string& roiId);
    std::vector<ROI> getROIs() const;
    std::vector<ROI> getActiveROIs() const;  // Task 73: Get only currently active ROIs

    // Configuration
    void setMinObjectSize(int minWidth, int minHeight);
    void setTrackingTimeout(double timeoutSeconds);

    // ReID configuration
    void setReIDConfig(const ReIDConfig& config);
    ReIDConfig getReIDConfig() const;
    void setReIDSimilarityThreshold(float threshold);
    float getReIDSimilarityThreshold() const;
    void setReIDEnabled(bool enabled);
    bool isReIDEnabled() const;

    // Task 77: Camera ID management for cross-camera tracking
    void setCameraId(const std::string& cameraId) { m_cameraId = cameraId; }
    std::string getCameraId() const { return m_cameraId; }

    // Visualization
    void drawROIs(cv::Mat& frame) const;
    void drawObjectStates(cv::Mat& frame) const;

    // Time-based validation
    static bool isValidTimeFormat(const std::string& timeStr);
    static bool isCurrentTimeInRange(const std::string& startTime, const std::string& endTime);
    bool isROIActiveNow(const ROI& roi) const;

private:
    // Internal analysis methods
    void updateObjectStates(const std::vector<cv::Rect>& detections,
                           const std::vector<int>& trackIds);
    void updateObjectStatesWithReID(const std::vector<cv::Rect>& detections,
                                   const std::vector<int>& trackIds,
                                   const std::vector<std::vector<float>>& reidFeatures,
                                   const std::string& cameraId);
    std::vector<BehaviorEvent> checkIntrusionRules();
    std::vector<BehaviorEvent> checkIntrusionRulesWithPriority();
    std::vector<std::string> getOverlappingROIs(const cv::Point2f& point) const;
    std::string getHighestPriorityROI(const std::vector<std::string>& roiIds) const;
    bool isPointInROI(const cv::Point2f& point, const ROI& roi) const;
    bool isObjectInROI(const cv::Rect& bbox, const ROI& roi) const;
    void cleanupOldObjects();
    std::string generateTimestamp() const;

    // ReID matching methods
    std::vector<ReIDMatchResult> findReIDMatches(const std::vector<float>& features,
                                                int excludeTrackId = -1) const;
    float computeReIDSimilarity(const std::vector<float>& features1,
                               const std::vector<float>& features2) const;
    bool isValidReIDMatch(const ReIDMatchResult& match) const;
    void updateReIDFeatures(ObjectState& state, const std::vector<float>& newFeatures);
    void cleanupExpiredReIDMatches();

    // Enhanced conflict resolution methods
    struct ConflictResolutionResult {
        std::string selectedROIId;
        std::vector<std::string> conflictingROIs;
        std::string resolutionReason;
        int selectedPriority;
        bool timeBasedResolution;
    };

    ConflictResolutionResult resolveROIConflicts(const cv::Point2f& point) const;
    std::vector<std::string> getActiveOverlappingROIs(const cv::Point2f& point) const;
    bool compareROIPriority(const std::string& roi1Id, const std::string& roi2Id) const;
    std::string formatConflictMetadata(const ConflictResolutionResult& result) const;

    // Member variables
    std::map<std::string, IntrusionRule> m_intrusionRules;
    std::map<std::string, ROI> m_rois;
    std::map<int, ObjectState> m_objectStates;

    // Configuration
    cv::Size m_minObjectSize;
    double m_trackingTimeout;  // seconds

    // ReID configuration
    ReIDConfig m_reidConfig;
    std::atomic<bool> m_reidEnabled{true};
    std::atomic<float> m_reidSimilarityThreshold{0.7f};

    // Task 77: Camera ID for cross-camera tracking
    std::string m_cameraId;

    // Thread safety
    mutable std::mutex m_mutex;

    // Constants
    static constexpr double DEFAULT_TRACKING_TIMEOUT = 30.0;  // seconds
    static constexpr int DEFAULT_MIN_WIDTH = 20;
    static constexpr int DEFAULT_MIN_HEIGHT = 20;
    static constexpr float DEFAULT_REID_SIMILARITY_THRESHOLD = 0.7f;
    static constexpr float MIN_REID_SIMILARITY_THRESHOLD = 0.5f;
    static constexpr float MAX_REID_SIMILARITY_THRESHOLD = 0.95f;
    static constexpr double DEFAULT_REID_MATCH_TIMEOUT = 30.0;  // seconds
};
