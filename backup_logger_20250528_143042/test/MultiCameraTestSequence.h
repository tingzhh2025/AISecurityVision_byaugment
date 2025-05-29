/**
 * Task 78: Multi-Camera Test Sequence Generator and Validator
 * Implements test video sequences with known object transitions between camera views
 */

#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <opencv2/opencv.hpp>

/**
 * @brief Ground truth tracking data for validation
 */
struct GroundTruthTrack {
    int objectId;                           // Unique object identifier
    std::string cameraId;                   // Camera where object appears
    double timestamp;                       // Timestamp of appearance
    cv::Rect boundingBox;                   // Object bounding box
    std::vector<float> reidFeatures;        // Expected ReID features
    double confidence;                      // Detection confidence
    
    GroundTruthTrack() : objectId(-1), timestamp(0.0), confidence(0.0) {}
    GroundTruthTrack(int id, const std::string& camera, double time, 
                    const cv::Rect& bbox, const std::vector<float>& features, double conf)
        : objectId(id), cameraId(camera), timestamp(time), boundingBox(bbox), 
          reidFeatures(features), confidence(conf) {}
};

/**
 * @brief Cross-camera transition event for validation
 */
struct TransitionEvent {
    int objectId;                           // Object identifier
    std::string fromCamera;                 // Source camera
    std::string toCamera;                   // Destination camera
    double transitionTime;                  // Time of transition
    double expectedDelay;                   // Expected detection delay
    bool validated = false;                 // Whether transition was validated
    
    TransitionEvent() : objectId(-1), transitionTime(0.0), expectedDelay(0.0) {}
    TransitionEvent(int id, const std::string& from, const std::string& to, 
                   double time, double delay)
        : objectId(id), fromCamera(from), toCamera(to), 
          transitionTime(time), expectedDelay(delay) {}
};

/**
 * @brief Test sequence configuration
 */
struct TestSequenceConfig {
    std::string sequenceName;               // Name of test sequence
    std::vector<std::string> cameraIds;     // Participating cameras
    double duration;                        // Total sequence duration (seconds)
    int objectCount;                        // Number of objects to track
    double transitionInterval;              // Time between transitions
    std::string outputPath;                 // Path for test results
    bool enableLogging = true;              // Enable detailed logging
    double validationThreshold = 0.9;      // 90% consistency requirement
    
    TestSequenceConfig() : duration(60.0), objectCount(5), transitionInterval(10.0), 
                          validationThreshold(0.9) {}
};

/**
 * @brief Validation results for test sequence
 */
struct ValidationResults {
    int totalTransitions = 0;               // Total expected transitions
    int successfulTransitions = 0;          // Successfully tracked transitions
    int failedTransitions = 0;              // Failed transitions
    double successRate = 0.0;               // Success rate percentage
    std::vector<std::string> failureReasons; // Reasons for failures
    std::map<std::string, int> cameraStats; // Per-camera statistics
    double averageLatency = 0.0;            // Average transition latency
    std::string detailedReport;             // Detailed validation report
    
    bool meetsThreshold(double threshold) const {
        return successRate >= threshold;
    }
};

/**
 * @brief Multi-Camera Test Sequence Generator and Validator
 */
class MultiCameraTestSequence {
public:
    MultiCameraTestSequence();
    ~MultiCameraTestSequence();

    // Configuration
    bool loadSequenceConfig(const std::string& configPath);
    void setConfig(const TestSequenceConfig& config);
    TestSequenceConfig getConfig() const { return m_config; }

    // Ground truth management
    bool loadGroundTruth(const std::string& groundTruthPath);
    void addGroundTruthTrack(const GroundTruthTrack& track);
    void addTransitionEvent(const TransitionEvent& transition);

    // Test sequence execution
    bool generateTestSequence();
    bool startTestMode();
    void stopTestMode();
    bool isRunning() const { return m_running; }

    // Validation
    void recordDetection(const std::string& cameraId, int trackId, int globalTrackId, 
                        double timestamp, const cv::Rect& bbox);
    void recordTransition(const std::string& fromCamera, const std::string& toCamera,
                         int localTrackId, int globalTrackId, double timestamp);
    
    ValidationResults validateSequence();
    bool exportResults(const std::string& outputPath);

    // Logging and monitoring
    void enableDetailedLogging(bool enable) { m_detailedLogging = enable; }
    std::string getValidationReport() const;
    void printStatistics() const;

private:
    TestSequenceConfig m_config;
    std::vector<GroundTruthTrack> m_groundTruth;
    std::vector<TransitionEvent> m_expectedTransitions;
    
    // Runtime tracking
    std::map<std::string, std::vector<GroundTruthTrack>> m_detectedTracks;
    std::map<std::string, std::vector<TransitionEvent>> m_recordedTransitions;
    
    bool m_running = false;
    bool m_detailedLogging = true;
    std::chrono::steady_clock::time_point m_startTime;
    
    // Validation helpers
    bool validateTrackConsistency(int objectId);
    bool validateTransitionTiming(const TransitionEvent& expected, 
                                 const TransitionEvent& actual);
    double calculateSuccessRate() const;
    std::string generateDetailedReport() const;
    
    // Logging
    void logEvent(const std::string& message);
    void logTransition(const TransitionEvent& transition, bool success);
    void logValidationResult(const ValidationResults& results);
};

/**
 * @brief Test sequence factory for common scenarios
 */
class TestSequenceFactory {
public:
    // Predefined test sequences
    static TestSequenceConfig createLinearTransitionSequence(
        const std::vector<std::string>& cameras, double duration = 60.0);
    
    static TestSequenceConfig createCrossoverSequence(
        const std::vector<std::string>& cameras, double duration = 90.0);
    
    static TestSequenceConfig createMultiObjectSequence(
        const std::vector<std::string>& cameras, int objectCount = 5, double duration = 120.0);
    
    static TestSequenceConfig createStressTestSequence(
        const std::vector<std::string>& cameras, double duration = 300.0);

    // Ground truth generators
    static std::vector<GroundTruthTrack> generateLinearGroundTruth(
        const TestSequenceConfig& config);
    
    static std::vector<TransitionEvent> generateTransitionEvents(
        const TestSequenceConfig& config);
};
