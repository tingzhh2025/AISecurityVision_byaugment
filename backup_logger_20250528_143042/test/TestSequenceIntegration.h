/**
 * Task 78: Test Sequence Integration with TaskManager
 * Integrates multi-camera test sequences with the main system for validation
 */

#pragma once

#include "MultiCameraTestSequence.h"
#include "../core/TaskManager.h"
#include <memory>
#include <thread>
#include <atomic>

/**
 * @brief Integration class for running test sequences with the main system
 */
class TestSequenceIntegration {
public:
    TestSequenceIntegration();
    ~TestSequenceIntegration();

    // Test sequence management
    bool loadTestSequence(const std::string& configPath);
    bool createTestSequence(const TestSequenceConfig& config);
    void setTestSequence(std::shared_ptr<MultiCameraTestSequence> sequence);

    // System integration
    bool setupTestCameras();
    bool startTestExecution();
    void stopTestExecution();
    bool isRunning() const { return m_running.load(); }

    // Monitoring and validation
    void enableRealtimeMonitoring(bool enable) { m_realtimeMonitoring = enable; }
    ValidationResults getValidationResults();
    bool exportTestResults(const std::string& outputPath);

    // Event callbacks for TaskManager integration
    void onDetectionEvent(const std::string& cameraId, int trackId, int globalTrackId,
                         double timestamp, const cv::Rect& bbox);
    void onTransitionEvent(const std::string& fromCamera, const std::string& toCamera,
                          int localTrackId, int globalTrackId, double timestamp);

    // Test configuration
    void setValidationThreshold(double threshold);
    void setTestDuration(double duration);
    void enableDetailedLogging(bool enable);

private:
    std::shared_ptr<MultiCameraTestSequence> m_testSequence;
    TaskManager* m_taskManager;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_realtimeMonitoring{true};
    std::thread m_monitoringThread;
    
    // Test cameras
    std::vector<VideoSource> m_testCameras;
    std::vector<std::string> m_activeCameraIds;
    
    // Monitoring
    void monitoringLoop();
    void checkSystemHealth();
    void validateRealtime();
    
    // Camera setup helpers
    VideoSource createTestCamera(const std::string& cameraId, int index);
    bool addTestCameraToSystem(const VideoSource& camera);
    void removeTestCameras();
    
    // Logging
    void logIntegrationEvent(const std::string& message);
};

/**
 * @brief Test sequence runner for automated testing
 */
class TestSequenceRunner {
public:
    TestSequenceRunner();
    ~TestSequenceRunner();

    // Predefined test scenarios
    bool runLinearTransitionTest(const std::vector<std::string>& cameras, double duration = 60.0);
    bool runCrossoverTest(const std::vector<std::string>& cameras, double duration = 90.0);
    bool runMultiObjectTest(const std::vector<std::string>& cameras, int objectCount = 5, double duration = 120.0);
    bool runStressTest(const std::vector<std::string>& cameras, double duration = 300.0);

    // Comprehensive test suite
    bool runFullTestSuite();
    bool runQuickValidationTest();

    // Results management
    std::vector<ValidationResults> getAllResults() const { return m_allResults; }
    bool exportAllResults(const std::string& outputDir);
    void printSummaryReport();

    // Configuration
    void setGlobalValidationThreshold(double threshold) { m_globalThreshold = threshold; }
    void enableVerboseLogging(bool enable) { m_verboseLogging = enable; }

private:
    std::vector<ValidationResults> m_allResults;
    double m_globalThreshold = 0.9;
    bool m_verboseLogging = true;
    
    // Test execution helpers
    bool executeTestSequence(const TestSequenceConfig& config, const std::string& testName);
    void logTestResult(const std::string& testName, const ValidationResults& results);
    bool meetsGlobalThreshold(const ValidationResults& results);
};

/**
 * @brief Test data generator for creating realistic test scenarios
 */
class TestDataGenerator {
public:
    // Video sequence generation
    static bool generateTestVideoSequence(const std::string& outputPath,
                                         const TestSequenceConfig& config);
    
    // Ground truth data generation
    static bool generateGroundTruthFile(const std::string& outputPath,
                                       const std::vector<GroundTruthTrack>& tracks);
    
    // Configuration file generation
    static bool generateTestConfig(const std::string& outputPath,
                                  const TestSequenceConfig& config);
    
    // Mock camera stream generation
    static bool generateMockCameraStreams(const std::vector<std::string>& cameraIds,
                                         const std::string& outputDir);

private:
    // Helper functions for data generation
    static cv::Mat generateTestFrame(int width, int height, int frameNumber);
    static void drawTestObject(cv::Mat& frame, const cv::Rect& bbox, int objectId);
    static std::vector<float> generateConsistentReIDFeatures(int objectId, int seed = 42);
};
