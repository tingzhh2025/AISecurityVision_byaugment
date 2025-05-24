#include "BehaviorAnalyzer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

BehaviorAnalyzer::BehaviorAnalyzer()
    : m_minObjectSize(DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT),
      m_trackingTimeout(DEFAULT_TRACKING_TIMEOUT) {
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

    std::cout << "[BehaviorAnalyzer] Initialized with default intrusion rule" << std::endl;
    return true;
}

std::vector<BehaviorEvent> BehaviorAnalyzer::analyze(const cv::Mat& frame,
                                                   const std::vector<cv::Rect>& detections,
                                                   const std::vector<int>& trackIds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<BehaviorEvent> events;

    // Update object states with current detections
    updateObjectStates(detections, trackIds);

    // Check intrusion rules
    auto intrusionEvents = checkIntrusionRules();
    events.insert(events.end(), intrusionEvents.begin(), intrusionEvents.end());

    // Cleanup old objects
    cleanupOldObjects();

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
                // Object is in ROI, check duration
                double duration = std::chrono::duration<double>(now - roiEntryIt->second).count();

                if (duration >= rule.minDuration) {
                    // Generate intrusion event
                    cv::Rect bbox(
                        static_cast<int>(state.position.x - 25),
                        static_cast<int>(state.position.y - 25),
                        50, 50
                    );

                    BehaviorEvent event("intrusion", rule.id, std::to_string(state.trackId),
                                      bbox, rule.confidence);
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

    std::cout << "[BehaviorAnalyzer] Added intrusion rule: " << rule.id << std::endl;
    return true;
}

bool BehaviorAnalyzer::removeIntrusionRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_intrusionRules.find(ruleId);
    if (it != m_intrusionRules.end()) {
        m_intrusionRules.erase(it);
        std::cout << "[BehaviorAnalyzer] Removed intrusion rule: " << ruleId << std::endl;
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
    std::cout << "[BehaviorAnalyzer] Added ROI: " << roi.id << std::endl;
    return true;
}

bool BehaviorAnalyzer::removeROI(const std::string& roiId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_rois.find(roiId);
    if (it != m_rois.end()) {
        m_rois.erase(it);
        std::cout << "[BehaviorAnalyzer] Removed ROI: " << roiId << std::endl;
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
    std::cout << "[BehaviorAnalyzer] JSON rule loading not yet implemented: " << jsonPath << std::endl;
    return true;
}
