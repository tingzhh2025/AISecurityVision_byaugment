/**
 * Task 78: Multi-Camera Test Sequence Implementation
 */

#include "MultiCameraTestSequence.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <random>

#include "../core/Logger.h"
using namespace AISecurityVision;
MultiCameraTestSequence::MultiCameraTestSequence() {
    m_config.sequenceName = "default_test_sequence";
    m_config.cameraIds = {"camera_1", "camera_2", "camera_3"};
    m_config.duration = 60.0;
    m_config.objectCount = 3;
    m_config.transitionInterval = 15.0;
    m_config.validationThreshold = 0.9;
}

MultiCameraTestSequence::~MultiCameraTestSequence() {
    if (m_running) {
        stopTestMode();
    }
}

bool MultiCameraTestSequence::loadSequenceConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOG_ERROR() << "[MultiCameraTestSequence] Failed to open config file: " << configPath;
        return false;
    }

    // Simple JSON-like parsing for configuration
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("\"sequenceName\"") != std::string::npos) {
            size_t start = line.find("\"", line.find(":")) + 1;
            size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos) {
                m_config.sequenceName = line.substr(start, end - start);
            }
        } else if (line.find("\"duration\"") != std::string::npos) {
            size_t start = line.find(":") + 1;
            m_config.duration = std::stod(line.substr(start));
        } else if (line.find("\"objectCount\"") != std::string::npos) {
            size_t start = line.find(":") + 1;
            m_config.objectCount = std::stoi(line.substr(start));
        } else if (line.find("\"validationThreshold\"") != std::string::npos) {
            size_t start = line.find(":") + 1;
            m_config.validationThreshold = std::stod(line.substr(start));
        }
    }

    logEvent("Loaded test sequence configuration: " + m_config.sequenceName);
    return true;
}

void MultiCameraTestSequence::setConfig(const TestSequenceConfig& config) {
    m_config = config;
    logEvent("Test sequence configuration updated: " + config.sequenceName);
}

bool MultiCameraTestSequence::loadGroundTruth(const std::string& groundTruthPath) {
    std::ifstream file(groundTruthPath);
    if (!file.is_open()) {
        LOG_ERROR() << "[MultiCameraTestSequence] Failed to open ground truth file: " << groundTruthPath;
        return false;
    }

    m_groundTruth.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(iss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 8) {
            GroundTruthTrack track;
            track.objectId = std::stoi(tokens[0]);
            track.cameraId = tokens[1];
            track.timestamp = std::stod(tokens[2]);
            track.boundingBox = cv::Rect(std::stoi(tokens[3]), std::stoi(tokens[4]),
                                       std::stoi(tokens[5]), std::stoi(tokens[6]));
            track.confidence = std::stod(tokens[7]);

            // Parse ReID features if available
            for (size_t i = 8; i < tokens.size(); ++i) {
                track.reidFeatures.push_back(std::stof(tokens[i]));
            }

            m_groundTruth.push_back(track);
        }
    }

    logEvent("Loaded " + std::to_string(m_groundTruth.size()) + " ground truth tracks");
    return true;
}

void MultiCameraTestSequence::addGroundTruthTrack(const GroundTruthTrack& track) {
    m_groundTruth.push_back(track);
    if (m_detailedLogging) {
        logEvent("Added ground truth track: Object " + std::to_string(track.objectId) +
                " in " + track.cameraId + " at " + std::to_string(track.timestamp));
    }
}

void MultiCameraTestSequence::addTransitionEvent(const TransitionEvent& transition) {
    m_expectedTransitions.push_back(transition);
    if (m_detailedLogging) {
        logEvent("Added expected transition: Object " + std::to_string(transition.objectId) +
                " from " + transition.fromCamera + " to " + transition.toCamera);
    }
}

bool MultiCameraTestSequence::generateTestSequence() {
    logEvent("Generating test sequence: " + m_config.sequenceName);

    // Clear existing data
    m_groundTruth.clear();
    m_expectedTransitions.clear();

    // Generate ground truth tracks using factory
    auto groundTruth = TestSequenceFactory::generateLinearGroundTruth(m_config);
    for (const auto& track : groundTruth) {
        addGroundTruthTrack(track);
    }

    // Generate transition events
    auto transitions = TestSequenceFactory::generateTransitionEvents(m_config);
    for (const auto& transition : transitions) {
        addTransitionEvent(transition);
    }

    logEvent("Generated test sequence with " + std::to_string(m_groundTruth.size()) +
            " tracks and " + std::to_string(m_expectedTransitions.size()) + " transitions");

    return true;
}

bool MultiCameraTestSequence::startTestMode() {
    if (m_running) {
        LOG_ERROR() << "[MultiCameraTestSequence] Test mode already running";
        return false;
    }

    m_running = true;
    m_startTime = std::chrono::steady_clock::now();

    // Clear runtime tracking data
    m_detectedTracks.clear();
    m_recordedTransitions.clear();

    logEvent("Started test mode for sequence: " + m_config.sequenceName);
    logEvent("Expected duration: " + std::to_string(m_config.duration) + " seconds");
    logEvent("Validation threshold: " + std::to_string(m_config.validationThreshold * 100) + "%");

    return true;
}

void MultiCameraTestSequence::stopTestMode() {
    if (!m_running) {
        return;
    }

    m_running = false;

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - m_startTime);

    logEvent("Stopped test mode after " + std::to_string(duration.count()) + " seconds");
}

void MultiCameraTestSequence::recordDetection(const std::string& cameraId, int trackId,
                                             int globalTrackId, double timestamp,
                                             const cv::Rect& bbox) {
    if (!m_running) return;

    // Create detection record
    GroundTruthTrack detection;
    detection.objectId = globalTrackId;  // Use global track ID as object ID
    detection.cameraId = cameraId;
    detection.timestamp = timestamp;
    detection.boundingBox = bbox;
    detection.confidence = 1.0;  // Assume detected objects have high confidence

    m_detectedTracks[cameraId].push_back(detection);

    if (m_detailedLogging) {
        logEvent("Recorded detection: Global ID " + std::to_string(globalTrackId) +
                " in " + cameraId + " at " + std::to_string(timestamp));
    }
}

void MultiCameraTestSequence::recordTransition(const std::string& fromCamera,
                                              const std::string& toCamera,
                                              int localTrackId, int globalTrackId,
                                              double timestamp) {
    if (!m_running) return;

    TransitionEvent transition;
    transition.objectId = globalTrackId;
    transition.fromCamera = fromCamera;
    transition.toCamera = toCamera;
    transition.transitionTime = timestamp;
    transition.expectedDelay = 2.0;  // 2 second expected delay

    std::string key = fromCamera + "_to_" + toCamera;
    m_recordedTransitions[key].push_back(transition);

    logTransition(transition, true);
}

ValidationResults MultiCameraTestSequence::validateSequence() {
    ValidationResults results;

    logEvent("Starting sequence validation...");

    // Count total expected transitions
    results.totalTransitions = static_cast<int>(m_expectedTransitions.size());

    // Validate each expected transition
    for (const auto& expected : m_expectedTransitions) {
        bool found = false;
        std::string key = expected.fromCamera + "_to_" + expected.toCamera;

        if (m_recordedTransitions.find(key) != m_recordedTransitions.end()) {
            for (const auto& recorded : m_recordedTransitions[key]) {
                if (recorded.objectId == expected.objectId &&
                    validateTransitionTiming(expected, recorded)) {
                    results.successfulTransitions++;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            results.failedTransitions++;
            results.failureReasons.push_back(
                "Missing transition for object " + std::to_string(expected.objectId) +
                " from " + expected.fromCamera + " to " + expected.toCamera);
        }
    }

    // Calculate success rate
    if (results.totalTransitions > 0) {
        results.successRate = static_cast<double>(results.successfulTransitions) /
                             results.totalTransitions;
    }

    // Generate detailed report
    results.detailedReport = generateDetailedReport();

    logValidationResult(results);

    return results;
}

bool MultiCameraTestSequence::validateTransitionTiming(const TransitionEvent& expected,
                                                      const TransitionEvent& actual) {
    double timeDiff = std::abs(actual.transitionTime - expected.transitionTime);
    return timeDiff <= expected.expectedDelay;
}

double MultiCameraTestSequence::calculateSuccessRate() const {
    if (m_expectedTransitions.empty()) return 0.0;

    int successful = 0;
    for (const auto& transition : m_expectedTransitions) {
        if (transition.validated) {
            successful++;
        }
    }

    return static_cast<double>(successful) / m_expectedTransitions.size();
}

std::string MultiCameraTestSequence::generateDetailedReport() const {
    std::ostringstream report;

    report << "=== Multi-Camera Test Sequence Validation Report ===" << std::endl;
    report << "Sequence: " << m_config.sequenceName << std::endl;
    report << "Duration: " << m_config.duration << " seconds" << std::endl;
    report << "Cameras: ";
    for (const auto& camera : m_config.cameraIds) {
        report << camera << " ";
    }
    report << std::endl << std::endl;

    report << "Ground Truth Summary:" << std::endl;
    report << "- Total tracks: " << m_groundTruth.size() << std::endl;
    report << "- Expected transitions: " << m_expectedTransitions.size() << std::endl;
    report << std::endl;

    report << "Detection Summary:" << std::endl;
    for (const auto& pair : m_detectedTracks) {
        report << "- " << pair.first << ": " << pair.second.size() << " detections" << std::endl;
    }
    report << std::endl;

    report << "Transition Summary:" << std::endl;
    for (const auto& pair : m_recordedTransitions) {
        report << "- " << pair.first << ": " << pair.second.size() << " transitions" << std::endl;
    }

    return report.str();
}

void MultiCameraTestSequence::logEvent(const std::string& message) {
    if (!m_detailedLogging) return;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    LOG_INFO() << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] [MultiCameraTest] " << message;
}

void MultiCameraTestSequence::logTransition(const TransitionEvent& transition, bool success) {
    std::string status = success ? "SUCCESS" : "FAILED";
    logEvent("Transition " + status + ": Object " + std::to_string(transition.objectId) +
            " from " + transition.fromCamera + " to " + transition.toCamera +
            " at " + std::to_string(transition.transitionTime));
}

void MultiCameraTestSequence::logValidationResult(const ValidationResults& results) {
    logEvent("=== Validation Results ===");
    logEvent("Total transitions: " + std::to_string(results.totalTransitions));
    logEvent("Successful: " + std::to_string(results.successfulTransitions));
    logEvent("Failed: " + std::to_string(results.failedTransitions));
    logEvent("Success rate: " + std::to_string(results.successRate * 100) + "%");
    logEvent("Threshold met: " + std::string(results.meetsThreshold(m_config.validationThreshold) ? "YES" : "NO"));
}

// TestSequenceFactory Implementation

TestSequenceConfig TestSequenceFactory::createLinearTransitionSequence(
    const std::vector<std::string>& cameras, double duration) {

    TestSequenceConfig config;
    config.sequenceName = "linear_transition_sequence";
    config.cameraIds = cameras;
    config.duration = duration;
    config.objectCount = 3;
    config.transitionInterval = duration / (cameras.size() * config.objectCount);
    config.validationThreshold = 0.9;

    return config;
}

TestSequenceConfig TestSequenceFactory::createCrossoverSequence(
    const std::vector<std::string>& cameras, double duration) {

    TestSequenceConfig config;
    config.sequenceName = "crossover_sequence";
    config.cameraIds = cameras;
    config.duration = duration;
    config.objectCount = cameras.size();
    config.transitionInterval = duration / (cameras.size() * 2);
    config.validationThreshold = 0.85;  // Slightly lower for complex crossover

    return config;
}

TestSequenceConfig TestSequenceFactory::createMultiObjectSequence(
    const std::vector<std::string>& cameras, int objectCount, double duration) {

    TestSequenceConfig config;
    config.sequenceName = "multi_object_sequence";
    config.cameraIds = cameras;
    config.duration = duration;
    config.objectCount = objectCount;
    config.transitionInterval = duration / (objectCount * cameras.size());
    config.validationThreshold = 0.9;

    return config;
}

TestSequenceConfig TestSequenceFactory::createStressTestSequence(
    const std::vector<std::string>& cameras, double duration) {

    TestSequenceConfig config;
    config.sequenceName = "stress_test_sequence";
    config.cameraIds = cameras;
    config.duration = duration;
    config.objectCount = cameras.size() * 3;  // 3 objects per camera
    config.transitionInterval = 5.0;  // Rapid transitions every 5 seconds
    config.validationThreshold = 0.8;  // Lower threshold for stress test

    return config;
}

std::vector<GroundTruthTrack> TestSequenceFactory::generateLinearGroundTruth(
    const TestSequenceConfig& config) {

    std::vector<GroundTruthTrack> tracks;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> featureDist(0.0, 1.0);

    double timeStep = config.transitionInterval;
    int objectId = 1;

    for (int obj = 0; obj < config.objectCount; ++obj) {
        double currentTime = obj * timeStep;

        for (size_t camIdx = 0; camIdx < config.cameraIds.size(); ++camIdx) {
            GroundTruthTrack track;
            track.objectId = objectId;
            track.cameraId = config.cameraIds[camIdx];
            track.timestamp = currentTime + (camIdx * timeStep);

            // Generate consistent bounding box
            int baseX = 100 + (obj * 50);
            int baseY = 100 + (camIdx * 30);
            track.boundingBox = cv::Rect(baseX, baseY, 80, 120);

            // Generate consistent ReID features for same object
            track.reidFeatures.resize(128);
            for (int i = 0; i < 128; ++i) {
                track.reidFeatures[i] = static_cast<float>(featureDist(gen) + obj * 0.1);
            }

            track.confidence = 0.85 + (featureDist(gen) * 0.1);
            tracks.push_back(track);
        }

        objectId++;
    }

    return tracks;
}

std::vector<TransitionEvent> TestSequenceFactory::generateTransitionEvents(
    const TestSequenceConfig& config) {

    std::vector<TransitionEvent> transitions;
    double timeStep = config.transitionInterval;
    int objectId = 1;

    for (int obj = 0; obj < config.objectCount; ++obj) {
        double currentTime = obj * timeStep;

        for (size_t camIdx = 0; camIdx < config.cameraIds.size() - 1; ++camIdx) {
            TransitionEvent transition;
            transition.objectId = objectId;
            transition.fromCamera = config.cameraIds[camIdx];
            transition.toCamera = config.cameraIds[camIdx + 1];
            transition.transitionTime = currentTime + (camIdx * timeStep) + (timeStep / 2);
            transition.expectedDelay = 2.0;  // 2 second tolerance

            transitions.push_back(transition);
        }

        objectId++;
    }

    return transitions;
}
