#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>

/**
 * @brief ROI (Region of Interest) definition for behavior analysis
 */
struct ROI {
    std::string id;
    std::string name;
    std::vector<cv::Point> polygon;
    bool enabled = true;
    int priority = 1;  // 1-5 scale

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
    std::string objectId;
    cv::Rect boundingBox;
    double confidence;
    std::string timestamp;
    std::string metadata;

    BehaviorEvent() : confidence(0.0) {}
    BehaviorEvent(const std::string& type, const std::string& rule,
                 const std::string& objId, const cv::Rect& bbox, double conf)
        : eventType(type), ruleId(rule), objectId(objId), boundingBox(bbox), confidence(conf) {}
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

    ObjectState() : trackId(-1), position(0, 0), velocity(0, 0) {}
    ObjectState(int id, const cv::Point2f& pos)
        : trackId(id), position(pos), velocity(0, 0) {
        firstSeen = lastSeen = std::chrono::steady_clock::now();
        trajectory.push_back(pos);
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

    // Rule management
    bool addIntrusionRule(const IntrusionRule& rule);
    bool removeIntrusionRule(const std::string& ruleId);
    std::vector<IntrusionRule> getIntrusionRules() const;
    bool updateIntrusionRule(const IntrusionRule& rule);

    // ROI management
    bool addROI(const ROI& roi);
    bool removeROI(const std::string& roiId);
    std::vector<ROI> getROIs() const;

    // Configuration
    void setMinObjectSize(int minWidth, int minHeight);
    void setTrackingTimeout(double timeoutSeconds);

    // Visualization
    void drawROIs(cv::Mat& frame) const;
    void drawObjectStates(cv::Mat& frame) const;

private:
    // Internal analysis methods
    void updateObjectStates(const std::vector<cv::Rect>& detections,
                           const std::vector<int>& trackIds);
    std::vector<BehaviorEvent> checkIntrusionRules();
    bool isPointInROI(const cv::Point2f& point, const ROI& roi) const;
    bool isObjectInROI(const cv::Rect& bbox, const ROI& roi) const;
    void cleanupOldObjects();
    std::string generateTimestamp() const;

    // Member variables
    std::map<std::string, IntrusionRule> m_intrusionRules;
    std::map<std::string, ROI> m_rois;
    std::map<int, ObjectState> m_objectStates;

    // Configuration
    cv::Size m_minObjectSize;
    double m_trackingTimeout;  // seconds

    // Thread safety
    mutable std::mutex m_mutex;

    // Constants
    static constexpr double DEFAULT_TRACKING_TIMEOUT = 30.0;  // seconds
    static constexpr int DEFAULT_MIN_WIDTH = 20;
    static constexpr int DEFAULT_MIN_HEIGHT = 20;
};
