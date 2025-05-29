#include "BehaviorAnalyzer.h"
#include "../core/TaskManager.h"  // Task 77: For cross-camera tracking
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <chrono>
#include <regex>
#include <cstdio>

#include "../core/Logger.h"
using namespace AISecurityVision;
BehaviorAnalyzer::BehaviorAnalyzer()
    : m_minObjectSize(DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT),
      m_trackingTimeout(DEFAULT_TRACKING_TIMEOUT) {

    // Initialize ReID configuration
    m_reidConfig.enabled = true;
    m_reidConfig.similarityThreshold = DEFAULT_REID_SIMILARITY_THRESHOLD;
    m_reidConfig.maxMatches = 5;
    m_reidConfig.matchTimeout = DEFAULT_REID_MATCH_TIMEOUT;
    m_reidConfig.crossCameraEnabled = true;

    m_reidEnabled.store(true);
    m_reidSimilarityThreshold.store(DEFAULT_REID_SIMILARITY_THRESHOLD);
}

BehaviorAnalyzer::~BehaviorAnalyzer() {}

bool BehaviorAnalyzer::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a default intrusion rule for testing
    ROI defaultROI("default_roi", "Default Intrusion Zone", {
        cv::Point(100, 100),
        cv::Point(500, 100),
        cv::Point(500, 400),
        cv::Point(100, 400)
    });

    IntrusionRule defaultRule("default_intrusion", defaultROI, 5.0);
    m_intrusionRules[defaultRule.id] = defaultRule;
    m_rois[defaultROI.id] = defaultROI;

    LOG_INFO() << "[BehaviorAnalyzer] Initialized with default intrusion rule and ReID matching (threshold: "
              << m_reidSimilarityThreshold.load() << ")";
    return true;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::analyze(const cv::Mat& frame,
                                                   const std::vector<cv::Rect>& detections,
                                                   const std::vector<int>& trackIds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<BehaviorEvent> events;

    // Update object states with current detections
    updateObjectStates(detections, trackIds);

    // Check intrusion rules with priority-based conflict resolution
    auto intrusionEvents = checkIntrusionRulesWithPriority();
    events.insert(events.end(), intrusionEvents.begin(), intrusionEvents.end());

    // Cleanup old objects
    cleanupOldObjects();

    return events;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::analyzeWithReID(const cv::Mat& frame,
                                                           const std::vector<cv::Rect>& detections,
                                                           const std::vector<int>& trackIds,
                                                           const std::vector<std::vector<float>>& reidFeatures,
                                                           const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<BehaviorEvent> events;

    // Update object states with ReID features
    updateObjectStatesWithReID(detections, trackIds, reidFeatures, cameraId);

    // Check intrusion rules with priority-based conflict resolution
    auto intrusionEvents = checkIntrusionRulesWithPriority();
    events.insert(events.end(), intrusionEvents.begin(), intrusionEvents.end());

    // Cleanup old objects and expired ReID matches
    cleanupOldObjects();
    cleanupExpiredReIDMatches();

    return events;
}

void BehaviorAnalyzer::updateObjectStates(const std::vector<cv::Rect>& detections,
                                         const std::vector<int>& trackIds) {
    auto now = std::chrono::steady_clock::now();

    // Update existing objects and add new ones
    for (size_t i = 0; i < detections.size() && i < trackIds.size(); ++i) {
        const cv::Rect& bbox = detections[i];
        int trackId = trackIds[i];

        // Skip objects that are too small
        if (bbox.width < m_minObjectSize.width || bbox.height < m_minObjectSize.height) {
            continue;
        }

        cv::Point2f center(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);

        auto it = m_objectStates.find(trackId);
        if (it != m_objectStates.end()) {
            // Update existing object
            ObjectState& state = it->second;

            // Calculate velocity
            auto timeDiff = std::chrono::duration<double>(now - state.lastSeen).count();
            if (timeDiff > 0) {
                state.velocity.x = (center.x - state.position.x) / timeDiff;
                state.velocity.y = (center.y - state.position.y) / timeDiff;
            }

            state.position = center;
            state.lastSeen = now;
            state.trajectory.push_back(center);

            // Limit trajectory size
            if (state.trajectory.size() > 100) {
                state.trajectory.erase(state.trajectory.begin());
            }

            // Check ROI entry/exit
            for (const auto& roiPair : m_rois) {
                const ROI& roi = roiPair.second;
                if (!roi.enabled) continue;

                bool inROI = isObjectInROI(bbox, roi);
                auto roiEntryIt = state.roiEntryTimes.find(roi.id);

                if (inROI && roiEntryIt == state.roiEntryTimes.end()) {
                    // Object entered ROI
                    state.roiEntryTimes[roi.id] = now;
                } else if (!inROI && roiEntryIt != state.roiEntryTimes.end()) {
                    // Object left ROI
                    state.roiEntryTimes.erase(roiEntryIt);
                }
            }
        } else {
            // Create new object state
            ObjectState newState(trackId, center);
            m_objectStates[trackId] = newState;
        }
    }
}

void BehaviorAnalyzer::updateObjectStatesWithReID(const std::vector<cv::Rect>& detections,
                                                  const std::vector<int>& trackIds,
                                                  const std::vector<std::vector<float>>& reidFeatures,
                                                  const std::string& cameraId) {
    auto now = std::chrono::steady_clock::now();

    // Update existing objects and add new ones with ReID features
    for (size_t i = 0; i < detections.size() && i < trackIds.size(); ++i) {
        const cv::Rect& bbox = detections[i];
        int trackId = trackIds[i];

        // Skip objects that are too small
        if (bbox.width < m_minObjectSize.width || bbox.height < m_minObjectSize.height) {
            continue;
        }

        cv::Point2f center(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);

        auto it = m_objectStates.find(trackId);
        if (it != m_objectStates.end()) {
            // Update existing object
            ObjectState& state = it->second;

            // Calculate velocity
            auto timeDiff = std::chrono::duration<double>(now - state.lastSeen).count();
            if (timeDiff > 0) {
                state.velocity.x = (center.x - state.position.x) / timeDiff;
                state.velocity.y = (center.y - state.position.y) / timeDiff;
            }

            state.position = center;
            state.lastSeen = now;
            state.trajectory.push_back(center);
            state.cameraId = cameraId;

            // Update ReID features if available
            if (i < reidFeatures.size() && !reidFeatures[i].empty() && m_reidEnabled.load()) {
                updateReIDFeatures(state, reidFeatures[i]);

                // Find ReID matches with other objects
                auto matches = findReIDMatches(reidFeatures[i], trackId);
                state.reidMatches = matches;

                if (!matches.empty()) {
                    LOG_INFO() << "[BehaviorAnalyzer] Found " << matches.size()
                              << " ReID matches for track " << trackId
                              << " (threshold: " << m_reidSimilarityThreshold.load() << ")";
                }
            }

            // Limit trajectory size
            if (state.trajectory.size() > 100) {
                state.trajectory.erase(state.trajectory.begin());
            }

            // Check ROI entry/exit
            for (const auto& roiPair : m_rois) {
                const ROI& roi = roiPair.second;
                if (!roi.enabled) continue;

                bool inROI = isObjectInROI(bbox, roi);
                auto roiEntryIt = state.roiEntryTimes.find(roi.id);

                if (inROI && roiEntryIt == state.roiEntryTimes.end()) {
                    // Object entered ROI
                    state.roiEntryTimes[roi.id] = now;
                } else if (!inROI && roiEntryIt != state.roiEntryTimes.end()) {
                    // Object left ROI
                    state.roiEntryTimes.erase(roiEntryIt);
                }
            }
        } else {
            // Create new object state
            ObjectState newState(trackId, center);
            newState.cameraId = cameraId;

            // Set ReID features if available
            if (i < reidFeatures.size() && !reidFeatures[i].empty() && m_reidEnabled.load()) {
                newState.reidFeatures = reidFeatures[i];

                // Find ReID matches for new object
                auto matches = findReIDMatches(reidFeatures[i], trackId);
                newState.reidMatches = matches;

                if (!matches.empty()) {
                    LOG_INFO() << "[BehaviorAnalyzer] New track " << trackId
                              << " has " << matches.size() << " ReID matches";
                }
            }

            m_objectStates[trackId] = newState;
        }
    }
}

std::vector<BehaviorEvent> BehaviorAnalyzer::checkIntrusionRules() {
    std::vector<BehaviorEvent> events;
    auto now = std::chrono::steady_clock::now();

    for (const auto& rulePair : m_intrusionRules) {
        const IntrusionRule& rule = rulePair.second;
        if (!rule.enabled) continue;

        // Check each object against this rule
        for (const auto& statePair : m_objectStates) {
            const ObjectState& state = statePair.second;

            auto roiEntryIt = state.roiEntryTimes.find(rule.roi.id);
            if (roiEntryIt != state.roiEntryTimes.end()) {
                // Check if ROI is currently active based on time rules
                if (!isROIActiveNow(rule.roi)) {
                    continue; // ROI is not active during current time
                }

                // Object is in ROI, check duration
                double duration = std::chrono::duration<double>(now - roiEntryIt->second).count();

                if (duration >= rule.minDuration) {
                    // Generate intrusion event
                    cv::Rect bbox(
                        static_cast<int>(state.position.x - 25),
                        static_cast<int>(state.position.y - 25),
                        50, 50
                    );

                    // Task 77: Get global track ID from TaskManager for cross-camera persistence
                    TaskManager& taskManager = TaskManager::getInstance();
                    int globalTrackId = taskManager.getGlobalTrackId(m_cameraId, state.trackId);

                    // Create enhanced BehaviorEvent with ReID information
                    BehaviorEvent event("intrusion", rule.id, std::to_string(state.trackId),
                                      bbox, rule.confidence, state.trackId, globalTrackId, m_cameraId);
                    event.timestamp = generateTimestamp();
                    event.metadata = "Duration: " + std::to_string(duration) + "s, ROI: " + rule.roi.name;

                    events.push_back(event);

                    // Remove entry time to avoid duplicate events
                    const_cast<ObjectState&>(state).roiEntryTimes.erase(rule.roi.id);
                }
            }
        }
    }

    return events;
}

bool BehaviorAnalyzer::isPointInROI(const cv::Point2f& point, const ROI& roi) const {
    if (roi.polygon.size() < 3) return false;

    // Use OpenCV's pointPolygonTest
    return cv::pointPolygonTest(roi.polygon, point, false) >= 0;
}

bool BehaviorAnalyzer::isObjectInROI(const cv::Rect& bbox, const ROI& roi) const {
    // Check if object center is in ROI
    cv::Point2f center(bbox.x + bbox.width / 2.0f, bbox.y + bbox.height / 2.0f);
    return isPointInROI(center, roi);
}

void BehaviorAnalyzer::cleanupOldObjects() {
    auto now = std::chrono::steady_clock::now();

    auto it = m_objectStates.begin();
    while (it != m_objectStates.end()) {
        double timeSinceLastSeen = std::chrono::duration<double>(now - it->second.lastSeen).count();

        if (timeSinceLastSeen > m_trackingTimeout) {
            it = m_objectStates.erase(it);
        } else {
            ++it;
        }
    }
}

std::string BehaviorAnalyzer::generateTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

bool BehaviorAnalyzer::addIntrusionRule(const IntrusionRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_intrusionRules[rule.id] = rule;
    m_rois[rule.roi.id] = rule.roi;

    LOG_INFO() << "[BehaviorAnalyzer] Added intrusion rule: " << rule.id;
    return true;
}

bool BehaviorAnalyzer::removeIntrusionRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_intrusionRules.find(ruleId);
    if (it != m_intrusionRules.end()) {
        m_intrusionRules.erase(it);
        LOG_INFO() << "[BehaviorAnalyzer] Removed intrusion rule: " << ruleId;
        return true;
    }

    return false;
}

std::vector<IntrusionRule> BehaviorAnalyzer::getIntrusionRules() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<IntrusionRule> rules;
    for (const auto& pair : m_intrusionRules) {
        rules.push_back(pair.second);
    }

    return rules;
}

bool BehaviorAnalyzer::updateIntrusionRule(const IntrusionRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_intrusionRules.find(rule.id);
    if (it != m_intrusionRules.end()) {
        m_intrusionRules[rule.id] = rule;
        m_rois[rule.roi.id] = rule.roi;
        return true;
    }

    return false;
}

bool BehaviorAnalyzer::addROI(const ROI& roi) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_rois[roi.id] = roi;
    LOG_INFO() << "[BehaviorAnalyzer] Added ROI: " << roi.id;
    return true;
}

bool BehaviorAnalyzer::removeROI(const std::string& roiId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_rois.find(roiId);
    if (it != m_rois.end()) {
        m_rois.erase(it);
        LOG_INFO() << "[BehaviorAnalyzer] Removed ROI: " << roiId;
        return true;
    }

    return false;
}

std::vector<ROI> BehaviorAnalyzer::getROIs() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ROI> rois;
    for (const auto& pair : m_rois) {
        rois.push_back(pair.second);
    }

    return rois;
}

std::vector<ROI> BehaviorAnalyzer::getActiveROIs() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ROI> activeROIs;
    for (const auto& pair : m_rois) {
        const ROI& roi = pair.second;

        // Only include ROIs that are enabled and currently active based on time rules
        if (roi.enabled && isROIActiveNow(roi)) {
            activeROIs.push_back(roi);
        }
    }

    return activeROIs;
}

void BehaviorAnalyzer::setMinObjectSize(int minWidth, int minHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minObjectSize = cv::Size(minWidth, minHeight);
}

void BehaviorAnalyzer::setTrackingTimeout(double timeoutSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trackingTimeout = timeoutSeconds;
}

void BehaviorAnalyzer::drawROIs(cv::Mat& frame) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& roiPair : m_rois) {
        const ROI& roi = roiPair.second;
        if (!roi.enabled || roi.polygon.size() < 3) continue;

        // Choose color based on priority
        cv::Scalar color;
        switch (roi.priority) {
            case 1: color = cv::Scalar(0, 255, 0); break;    // Green - Low priority
            case 2: color = cv::Scalar(0, 255, 255); break;  // Yellow - Medium-low
            case 3: color = cv::Scalar(0, 165, 255); break;  // Orange - Medium
            case 4: color = cv::Scalar(0, 100, 255); break;  // Red-orange - High
            case 5: color = cv::Scalar(0, 0, 255); break;    // Red - Critical
            default: color = cv::Scalar(255, 255, 255); break; // White - Default
        }

        // Draw polygon
        std::vector<std::vector<cv::Point>> polygons = {roi.polygon};
        cv::polylines(frame, polygons, true, color, 2);

        // Fill with semi-transparent color
        cv::Mat overlay = frame.clone();
        cv::fillPoly(overlay, polygons, color);
        cv::addWeighted(frame, 0.8, overlay, 0.2, 0, frame);

        // Draw ROI name
        if (!roi.name.empty() && !roi.polygon.empty()) {
            cv::Point textPos = roi.polygon[0];
            textPos.y -= 10;

            cv::putText(frame, roi.name, textPos, cv::FONT_HERSHEY_SIMPLEX,
                       0.6, color, 2);
        }
    }
}

void BehaviorAnalyzer::drawObjectStates(cv::Mat& frame) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& statePair : m_objectStates) {
        const ObjectState& state = statePair.second;

        // Draw object position
        cv::circle(frame, state.position, 5, cv::Scalar(255, 0, 0), -1);

        // Draw track ID
        std::string trackText = "ID:" + std::to_string(state.trackId);
        cv::Point textPos(state.position.x + 10, state.position.y - 10);
        cv::putText(frame, trackText, textPos, cv::FONT_HERSHEY_SIMPLEX,
                   0.5, cv::Scalar(255, 255, 255), 1);

        // Draw trajectory
        if (state.trajectory.size() > 1) {
            for (size_t i = 1; i < state.trajectory.size(); ++i) {
                cv::line(frame, state.trajectory[i-1], state.trajectory[i],
                        cv::Scalar(0, 255, 255), 1);
            }
        }

        // Draw velocity vector
        if (std::abs(state.velocity.x) > 1 || std::abs(state.velocity.y) > 1) {
            cv::Point2f endPoint = state.position + state.velocity * 10; // Scale for visibility
            cv::arrowedLine(frame, state.position, endPoint,
                           cv::Scalar(255, 0, 255), 2);
        }
    }
}

bool BehaviorAnalyzer::loadRulesFromJson(const std::string& jsonPath) {
    // TODO: Implement JSON loading for rules
    // For now, just return true as a placeholder
    LOG_INFO() << "[BehaviorAnalyzer] JSON rule loading not yet implemented: " << jsonPath;
    return true;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::checkIntrusionRulesWithPriority() {
    std::vector<BehaviorEvent> events;
    auto now = std::chrono::steady_clock::now();

    // Track which objects have already generated events to avoid duplicates
    std::set<int> processedObjects;

    for (const auto& statePair : m_objectStates) {
        const ObjectState& state = statePair.second;

        if (processedObjects.find(state.trackId) != processedObjects.end()) {
            continue; // Already processed this object
        }

        // Use enhanced conflict resolution
        ConflictResolutionResult conflictResult = resolveROIConflicts(state.position);

        if (conflictResult.selectedROIId.empty()) {
            continue; // No valid ROI found after conflict resolution
        }

        // Find the intrusion rule for the selected ROI
        IntrusionRule* activeRule = nullptr;
        for (auto& rulePair : m_intrusionRules) {
            if (rulePair.second.roi.id == conflictResult.selectedROIId && rulePair.second.enabled) {
                activeRule = &rulePair.second;
                break;
            }
        }

        if (!activeRule) {
            continue; // No active rule for this ROI
        }

        // Check if object has been in this ROI long enough
        auto roiEntryIt = state.roiEntryTimes.find(activeRule->roi.id);
        if (roiEntryIt != state.roiEntryTimes.end()) {
            double duration = std::chrono::duration<double>(now - roiEntryIt->second).count();

            if (duration >= activeRule->minDuration) {
                // Generate intrusion event for selected ROI
                cv::Rect bbox(
                    static_cast<int>(state.position.x - 25),
                    static_cast<int>(state.position.y - 25),
                    50, 50
                );

                // Task 77: Get global track ID from TaskManager for cross-camera persistence
                TaskManager& taskManager = TaskManager::getInstance();
                int globalTrackId = taskManager.getGlobalTrackId(m_cameraId, state.trackId);

                // Create enhanced BehaviorEvent with ReID information
                BehaviorEvent event("intrusion", activeRule->id, std::to_string(state.trackId),
                                  bbox, activeRule->confidence, state.trackId, globalTrackId, m_cameraId);
                event.timestamp = generateTimestamp();

                // Enhanced metadata with detailed conflict resolution information
                std::ostringstream metadata;
                metadata << "Duration: " << std::fixed << std::setprecision(1) << duration << "s"
                         << ", ROI: " << activeRule->roi.name
                         << ", Priority: " << activeRule->roi.priority
                         << ", " << formatConflictMetadata(conflictResult);

                event.metadata = metadata.str();
                events.push_back(event);

                // Mark this object as processed and remove entry time to avoid duplicate events
                processedObjects.insert(state.trackId);
                const_cast<ObjectState&>(state).roiEntryTimes.erase(activeRule->roi.id);

                LOG_INFO() << "[BehaviorAnalyzer] Enhanced conflict-resolved intrusion event: "
                          << "Object " << state.trackId
                          << " in ROI " << activeRule->roi.name
                          << " (Priority " << activeRule->roi.priority << ")"
                          << " for " << duration << "s"
                          << " - " << conflictResult.resolutionReason;
            }
        }
    }

    return events;
}

std::vector<std::string> BehaviorAnalyzer::getOverlappingROIs(const cv::Point2f& point) const {
    std::vector<std::string> overlappingROIs;

    for (const auto& roiPair : m_rois) {
        const ROI& roi = roiPair.second;
        if (roi.enabled && isROIActiveNow(roi) && isPointInROI(point, roi)) {
            overlappingROIs.push_back(roi.id);
        }
    }

    return overlappingROIs;
}

std::string BehaviorAnalyzer::getHighestPriorityROI(const std::vector<std::string>& roiIds) const {
    if (roiIds.empty()) {
        return "";
    }

    std::string highestPriorityROIId = roiIds[0];
    int highestPriority = 0;

    for (const std::string& roiId : roiIds) {
        auto roiIt = m_rois.find(roiId);
        if (roiIt != m_rois.end()) {
            const ROI& roi = roiIt->second;
            if (roi.priority > highestPriority) {
                highestPriority = roi.priority;
                highestPriorityROIId = roiId;
            }
        }
    }

    return highestPriorityROIId;
}

// Time-based validation methods
bool BehaviorAnalyzer::isValidTimeFormat(const std::string& timeStr) {
    if (timeStr.empty()) {
        return true; // Empty time is valid (no time restriction)
    }

    // Check for HH:MM format
    std::regex timeRegex1(R"(^([0-1]?[0-9]|2[0-3]):([0-5][0-9])$)");
    if (std::regex_match(timeStr, timeRegex1)) {
        return true;
    }

    // Check for HH:MM:SS format
    std::regex timeRegex2(R"(^([0-1]?[0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9])$)");
    if (std::regex_match(timeStr, timeRegex2)) {
        return true;
    }

    return false;
}

bool BehaviorAnalyzer::isCurrentTimeInRange(const std::string& startTime, const std::string& endTime) {
    if (startTime.empty() || endTime.empty()) {
        return true; // No time restriction if either time is empty
    }

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    // Parse start time
    int startHour, startMin, startSec = 0;
    if (sscanf(startTime.c_str(), "%d:%d:%d", &startHour, &startMin, &startSec) < 2) {
        return true; // Invalid format, allow by default
    }

    // Parse end time
    int endHour, endMin, endSec = 0;
    if (sscanf(endTime.c_str(), "%d:%d:%d", &endHour, &endMin, &endSec) < 2) {
        return true; // Invalid format, allow by default
    }

    // Convert current time to seconds since midnight
    int currentSeconds = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
    int startSeconds = startHour * 3600 + startMin * 60 + startSec;
    int endSeconds = endHour * 3600 + endMin * 60 + endSec;

    // Handle cases where end time is next day (e.g., 22:00-06:00)
    if (endSeconds <= startSeconds) {
        // Time range crosses midnight
        return (currentSeconds >= startSeconds) || (currentSeconds <= endSeconds);
    } else {
        // Normal time range within same day
        return (currentSeconds >= startSeconds) && (currentSeconds <= endSeconds);
    }
}

bool BehaviorAnalyzer::isROIActiveNow(const ROI& roi) const {
    if (!roi.enabled) {
        return false;
    }

    return isCurrentTimeInRange(roi.start_time, roi.end_time);
}

// Enhanced conflict resolution methods implementation

BehaviorAnalyzer::ConflictResolutionResult BehaviorAnalyzer::resolveROIConflicts(const cv::Point2f& point) const {
    ConflictResolutionResult result;

    // Get all active overlapping ROIs (considering both enabled status and time rules)
    std::vector<std::string> activeROIs = getActiveOverlappingROIs(point);

    if (activeROIs.empty()) {
        result.resolutionReason = "No active ROIs found";
        return result;
    }

    result.conflictingROIs = activeROIs;

    if (activeROIs.size() == 1) {
        // No conflict - single ROI
        result.selectedROIId = activeROIs[0];
        result.resolutionReason = "Single active ROI";

        auto roiIt = m_rois.find(result.selectedROIId);
        if (roiIt != m_rois.end()) {
            result.selectedPriority = roiIt->second.priority;
        }

        return result;
    }

    // Multiple ROIs - resolve conflicts
    std::string highestPriorityROI = activeROIs[0];
    int highestPriority = 0;
    bool hasTimeBasedConflict = false;

    // Find ROI with highest priority
    for (const std::string& roiId : activeROIs) {
        auto roiIt = m_rois.find(roiId);
        if (roiIt != m_rois.end()) {
            const ROI& roi = roiIt->second;

            // Check if this ROI has time restrictions
            if (!roi.start_time.empty() || !roi.end_time.empty()) {
                hasTimeBasedConflict = true;
            }

            if (roi.priority > highestPriority) {
                highestPriority = roi.priority;
                highestPriorityROI = roiId;
            }
        }
    }

    result.selectedROIId = highestPriorityROI;
    result.selectedPriority = highestPriority;
    result.timeBasedResolution = hasTimeBasedConflict;

    // Generate detailed resolution reason
    std::ostringstream reason;
    reason << "Conflict resolved: " << activeROIs.size() << " overlapping ROIs, "
           << "selected priority " << highestPriority;

    if (hasTimeBasedConflict) {
        reason << " (time-based filtering applied)";
    }

    result.resolutionReason = reason.str();

    return result;
}

std::vector<std::string> BehaviorAnalyzer::getActiveOverlappingROIs(const cv::Point2f& point) const {
    std::vector<std::string> activeROIs;

    for (const auto& roiPair : m_rois) {
        const ROI& roi = roiPair.second;

        // Check if ROI is enabled
        if (!roi.enabled) {
            continue;
        }

        // Check if ROI is currently active based on time rules
        if (!isROIActiveNow(roi)) {
            continue;
        }

        // Check if point is within ROI polygon
        if (isPointInROI(point, roi)) {
            activeROIs.push_back(roi.id);
        }
    }

    return activeROIs;
}

bool BehaviorAnalyzer::compareROIPriority(const std::string& roi1Id, const std::string& roi2Id) const {
    auto roi1It = m_rois.find(roi1Id);
    auto roi2It = m_rois.find(roi2Id);

    if (roi1It == m_rois.end() || roi2It == m_rois.end()) {
        return false;
    }

    const ROI& roi1 = roi1It->second;
    const ROI& roi2 = roi2It->second;

    // Higher priority wins
    if (roi1.priority != roi2.priority) {
        return roi1.priority > roi2.priority;
    }

    // If priorities are equal, prefer ROI with time restrictions (more specific)
    bool roi1HasTime = !roi1.start_time.empty() || !roi1.end_time.empty();
    bool roi2HasTime = !roi2.start_time.empty() || !roi2.end_time.empty();

    if (roi1HasTime != roi2HasTime) {
        return roi1HasTime; // Prefer ROI with time restrictions
    }

    // If still equal, use lexicographic order for consistency
    return roi1.id < roi2.id;
}

std::string BehaviorAnalyzer::formatConflictMetadata(const ConflictResolutionResult& result) const {
    std::ostringstream metadata;

    metadata << "Conflicts: " << result.conflictingROIs.size() << " ROIs";

    if (result.conflictingROIs.size() > 1) {
        metadata << " [";
        for (size_t i = 0; i < result.conflictingROIs.size(); ++i) {
            if (i > 0) metadata << ", ";

            auto roiIt = m_rois.find(result.conflictingROIs[i]);
            if (roiIt != m_rois.end()) {
                metadata << roiIt->second.name << "(P" << roiIt->second.priority << ")";
            } else {
                metadata << result.conflictingROIs[i];
            }
        }
        metadata << "]";

        metadata << ", Resolution: " << result.resolutionReason;

        if (result.timeBasedResolution) {
            metadata << ", Time-filtered";
        }
    }

    return metadata.str();
}

// ReID matching methods implementation

std::vector<ReIDMatchResult> BehaviorAnalyzer::findReIDMatches(const std::vector<float>& features,
                                                              int excludeTrackId) const {
    std::vector<ReIDMatchResult> matches;

    if (features.empty() || !m_reidEnabled.load()) {
        return matches;
    }

    float threshold = m_reidSimilarityThreshold.load();

    // Search through all existing object states for matches
    for (const auto& statePair : m_objectStates) {
        const ObjectState& state = statePair.second;

        // Skip the same track and objects without ReID features
        if (state.trackId == excludeTrackId || !state.hasValidReIDFeatures()) {
            continue;
        }

        // Compute similarity
        float similarity = computeReIDSimilarity(features, state.reidFeatures);

        if (similarity >= threshold) {
            ReIDMatchResult match(state.trackId, similarity, state.cameraId);
            if (isValidReIDMatch(match)) {
                matches.push_back(match);
            }
        }
    }

    // Sort matches by similarity (highest first)
    std::sort(matches.begin(), matches.end(),
              [](const ReIDMatchResult& a, const ReIDMatchResult& b) {
                  return a.similarity > b.similarity;
              });

    // Limit to maximum number of matches
    if (matches.size() > static_cast<size_t>(m_reidConfig.maxMatches)) {
        matches.resize(m_reidConfig.maxMatches);
    }

    return matches;
}

float BehaviorAnalyzer::computeReIDSimilarity(const std::vector<float>& features1,
                                             const std::vector<float>& features2) const {
    if (features1.empty() || features2.empty() || features1.size() != features2.size()) {
        return 0.0f;
    }

    // Compute cosine similarity
    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < features1.size(); ++i) {
        dotProduct += features1[i] * features2[i];
        norm1 += features1[i] * features1[i];
        norm2 += features2[i] * features2[i];
    }

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

bool BehaviorAnalyzer::isValidReIDMatch(const ReIDMatchResult& match) const {
    if (!match.isValid) {
        return false;
    }

    // Check if similarity meets threshold
    float threshold = m_reidSimilarityThreshold.load();
    if (match.similarity < threshold) {
        return false;
    }

    // Additional validation can be added here
    // For example, temporal constraints, spatial constraints, etc.

    return true;
}

void BehaviorAnalyzer::updateReIDFeatures(ObjectState& state, const std::vector<float>& newFeatures) {
    if (newFeatures.empty()) {
        return;
    }

    if (state.reidFeatures.empty()) {
        // First time setting features
        state.reidFeatures = newFeatures;
    } else {
        // Update using exponential moving average (similar to TaskManager)
        const float alpha = 0.3f; // Learning rate

        if (state.reidFeatures.size() == newFeatures.size()) {
            for (size_t i = 0; i < state.reidFeatures.size(); ++i) {
                state.reidFeatures[i] = alpha * newFeatures[i] + (1.0f - alpha) * state.reidFeatures[i];
            }
        } else {
            // Size mismatch, replace entirely
            state.reidFeatures = newFeatures;
        }
    }
}

void BehaviorAnalyzer::cleanupExpiredReIDMatches() {
    auto now = std::chrono::steady_clock::now();
    double timeout = m_reidConfig.matchTimeout;

    for (auto& statePair : m_objectStates) {
        ObjectState& state = statePair.second;

        // Remove expired matches
        auto it = state.reidMatches.begin();
        while (it != state.reidMatches.end()) {
            // For now, we don't have timestamp in ReIDMatchResult
            // This could be enhanced to track match timestamps
            ++it;
        }

        // Clean up old ReID features if object hasn't been seen for a while
        double timeSinceLastSeen = std::chrono::duration<double>(now - state.lastSeen).count();
        if (timeSinceLastSeen > timeout) {
            state.reidFeatures.clear();
            state.reidMatches.clear();
        }
    }
}

// ReID configuration methods implementation

void BehaviorAnalyzer::setReIDConfig(const ReIDConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate threshold
    if (!config.isValidThreshold(config.similarityThreshold)) {
        LOG_INFO() << "[BehaviorAnalyzer] Invalid ReID similarity threshold: "
                  << config.similarityThreshold << " (valid range: "
                  << MIN_REID_SIMILARITY_THRESHOLD << "-" << MAX_REID_SIMILARITY_THRESHOLD << ")";
        return;
    }

    m_reidConfig = config;
    m_reidEnabled.store(config.enabled);
    m_reidSimilarityThreshold.store(config.similarityThreshold);

    LOG_INFO() << "[BehaviorAnalyzer] ReID config updated: enabled=" << config.enabled
              << ", threshold=" << config.similarityThreshold
              << ", maxMatches=" << config.maxMatches
              << ", timeout=" << config.matchTimeout << "s";
}

ReIDConfig BehaviorAnalyzer::getReIDConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_reidConfig;
}

void BehaviorAnalyzer::setReIDSimilarityThreshold(float threshold) {
    if (threshold < MIN_REID_SIMILARITY_THRESHOLD || threshold > MAX_REID_SIMILARITY_THRESHOLD) {
        LOG_INFO() << "[BehaviorAnalyzer] Invalid ReID similarity threshold: " << threshold
                  << " (valid range: " << MIN_REID_SIMILARITY_THRESHOLD
                  << "-" << MAX_REID_SIMILARITY_THRESHOLD << ")";
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_reidConfig.similarityThreshold = threshold;
    m_reidSimilarityThreshold.store(threshold);

    LOG_INFO() << "[BehaviorAnalyzer] ReID similarity threshold set to " << threshold;
}

float BehaviorAnalyzer::getReIDSimilarityThreshold() const {
    return m_reidSimilarityThreshold.load();
}

void BehaviorAnalyzer::setReIDEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reidConfig.enabled = enabled;
    m_reidEnabled.store(enabled);

    LOG_INFO() << "[BehaviorAnalyzer] ReID matching " << (enabled ? "enabled" : "disabled");
}

bool BehaviorAnalyzer::isReIDEnabled() const {
    return m_reidEnabled.load();
}
