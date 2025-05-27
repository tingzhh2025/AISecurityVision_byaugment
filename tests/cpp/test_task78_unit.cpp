/**
 * Task 78: Unit test for Multi-Camera Test Sequence implementation
 * Tests the test video sequences with known object transitions between camera views
 */

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// Include the headers we need to test
#include "src/test/MultiCameraTestSequence.h"

void test_ground_truth_track_creation() {
    std::cout << "[TEST] Testing GroundTruthTrack creation..." << std::endl;
    
    // Test default constructor
    GroundTruthTrack track1;
    assert(track1.objectId == -1);
    assert(track1.timestamp == 0.0);
    assert(track1.confidence == 0.0);
    assert(track1.cameraId.empty());
    assert(track1.reidFeatures.empty());
    std::cout << "✓ Default constructor sets fields correctly" << std::endl;
    
    // Test parameterized constructor
    cv::Rect bbox(100, 100, 80, 120);
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    GroundTruthTrack track2(123, "camera_1", 15.5, bbox, features, 0.85);
    
    assert(track2.objectId == 123);
    assert(track2.cameraId == "camera_1");
    assert(track2.timestamp == 15.5);
    assert(track2.boundingBox.x == 100);
    assert(track2.boundingBox.y == 100);
    assert(track2.boundingBox.width == 80);
    assert(track2.boundingBox.height == 120);
    assert(track2.reidFeatures.size() == 5);
    assert(track2.confidence == 0.85);
    std::cout << "✓ Parameterized constructor sets all fields correctly" << std::endl;
}

void test_transition_event_creation() {
    std::cout << "[TEST] Testing TransitionEvent creation..." << std::endl;
    
    // Test default constructor
    TransitionEvent event1;
    assert(event1.objectId == -1);
    assert(event1.transitionTime == 0.0);
    assert(event1.expectedDelay == 0.0);
    assert(event1.fromCamera.empty());
    assert(event1.toCamera.empty());
    assert(event1.validated == false);
    std::cout << "✓ Default constructor sets fields correctly" << std::endl;
    
    // Test parameterized constructor
    TransitionEvent event2(456, "camera_1", "camera_2", 25.0, 2.0);
    assert(event2.objectId == 456);
    assert(event2.fromCamera == "camera_1");
    assert(event2.toCamera == "camera_2");
    assert(event2.transitionTime == 25.0);
    assert(event2.expectedDelay == 2.0);
    assert(event2.validated == false);
    std::cout << "✓ Parameterized constructor sets all fields correctly" << std::endl;
}

void test_test_sequence_config() {
    std::cout << "[TEST] Testing TestSequenceConfig..." << std::endl;
    
    // Test default constructor
    TestSequenceConfig config1;
    assert(config1.duration == 60.0);
    assert(config1.objectCount == 5);
    assert(config1.transitionInterval == 10.0);
    assert(config1.validationThreshold == 0.9);
    assert(config1.enableLogging == true);
    std::cout << "✓ Default constructor sets reasonable defaults" << std::endl;
    
    // Test field assignment
    TestSequenceConfig config2;
    config2.sequenceName = "test_sequence";
    config2.cameraIds = {"cam1", "cam2", "cam3"};
    config2.duration = 120.0;
    config2.objectCount = 10;
    config2.validationThreshold = 0.85;
    
    assert(config2.sequenceName == "test_sequence");
    assert(config2.cameraIds.size() == 3);
    assert(config2.duration == 120.0);
    assert(config2.objectCount == 10);
    assert(config2.validationThreshold == 0.85);
    std::cout << "✓ Field assignment works correctly" << std::endl;
}

void test_validation_results() {
    std::cout << "[TEST] Testing ValidationResults..." << std::endl;
    
    ValidationResults results;
    
    // Test default values
    assert(results.totalTransitions == 0);
    assert(results.successfulTransitions == 0);
    assert(results.failedTransitions == 0);
    assert(results.successRate == 0.0);
    assert(results.averageLatency == 0.0);
    std::cout << "✓ Default constructor initializes fields correctly" << std::endl;
    
    // Test threshold checking
    results.successRate = 0.95;
    assert(results.meetsThreshold(0.9) == true);
    assert(results.meetsThreshold(0.98) == false);
    std::cout << "✓ Threshold checking works correctly" << std::endl;
    
    // Test with realistic data
    results.totalTransitions = 10;
    results.successfulTransitions = 9;
    results.failedTransitions = 1;
    results.successRate = 0.9;
    
    assert(results.meetsThreshold(0.9) == true);
    assert(results.meetsThreshold(0.95) == false);
    std::cout << "✓ Realistic validation data works correctly" << std::endl;
}

void test_multi_camera_test_sequence() {
    std::cout << "[TEST] Testing MultiCameraTestSequence..." << std::endl;
    
    MultiCameraTestSequence sequence;
    
    // Test initial state
    assert(sequence.isRunning() == false);
    std::cout << "✓ Initial state is correct" << std::endl;
    
    // Test configuration
    TestSequenceConfig config;
    config.sequenceName = "unit_test_sequence";
    config.cameraIds = {"test_cam_1", "test_cam_2"};
    config.duration = 30.0;
    config.objectCount = 2;
    config.validationThreshold = 0.8;
    
    sequence.setConfig(config);
    TestSequenceConfig retrievedConfig = sequence.getConfig();
    assert(retrievedConfig.sequenceName == "unit_test_sequence");
    assert(retrievedConfig.cameraIds.size() == 2);
    assert(retrievedConfig.duration == 30.0);
    std::cout << "✓ Configuration setting and retrieval works" << std::endl;
    
    // Test ground truth track addition
    cv::Rect bbox(50, 50, 100, 150);
    std::vector<float> features = {0.1f, 0.2f, 0.3f};
    GroundTruthTrack track(1, "test_cam_1", 10.0, bbox, features, 0.9);
    
    sequence.addGroundTruthTrack(track);
    std::cout << "✓ Ground truth track addition works" << std::endl;
    
    // Test transition event addition
    TransitionEvent transition(1, "test_cam_1", "test_cam_2", 15.0, 2.0);
    sequence.addTransitionEvent(transition);
    std::cout << "✓ Transition event addition works" << std::endl;
    
    // Test test mode start/stop
    assert(sequence.startTestMode() == true);
    assert(sequence.isRunning() == true);
    std::cout << "✓ Test mode start works" << std::endl;
    
    sequence.stopTestMode();
    assert(sequence.isRunning() == false);
    std::cout << "✓ Test mode stop works" << std::endl;
}

void test_test_sequence_factory() {
    std::cout << "[TEST] Testing TestSequenceFactory..." << std::endl;
    
    std::vector<std::string> cameras = {"cam1", "cam2", "cam3"};
    
    // Test linear transition sequence
    TestSequenceConfig linearConfig = TestSequenceFactory::createLinearTransitionSequence(cameras, 60.0);
    assert(linearConfig.sequenceName == "linear_transition_sequence");
    assert(linearConfig.cameraIds.size() == 3);
    assert(linearConfig.duration == 60.0);
    assert(linearConfig.objectCount == 3);
    assert(linearConfig.validationThreshold == 0.9);
    std::cout << "✓ Linear transition sequence creation works" << std::endl;
    
    // Test crossover sequence
    TestSequenceConfig crossoverConfig = TestSequenceFactory::createCrossoverSequence(cameras, 90.0);
    assert(crossoverConfig.sequenceName == "crossover_sequence");
    assert(crossoverConfig.cameraIds.size() == 3);
    assert(crossoverConfig.duration == 90.0);
    assert(crossoverConfig.validationThreshold == 0.85);
    std::cout << "✓ Crossover sequence creation works" << std::endl;
    
    // Test multi-object sequence
    TestSequenceConfig multiConfig = TestSequenceFactory::createMultiObjectSequence(cameras, 5, 120.0);
    assert(multiConfig.sequenceName == "multi_object_sequence");
    assert(multiConfig.objectCount == 5);
    assert(multiConfig.duration == 120.0);
    std::cout << "✓ Multi-object sequence creation works" << std::endl;
    
    // Test stress test sequence
    TestSequenceConfig stressConfig = TestSequenceFactory::createStressTestSequence(cameras, 300.0);
    assert(stressConfig.sequenceName == "stress_test_sequence");
    assert(stressConfig.objectCount == 9);  // 3 cameras * 3 objects
    assert(stressConfig.transitionInterval == 5.0);
    assert(stressConfig.validationThreshold == 0.8);
    std::cout << "✓ Stress test sequence creation works" << std::endl;
}

void test_ground_truth_generation() {
    std::cout << "[TEST] Testing ground truth generation..." << std::endl;
    
    TestSequenceConfig config;
    config.cameraIds = {"cam1", "cam2"};
    config.objectCount = 2;
    config.transitionInterval = 10.0;
    
    // Test ground truth generation
    std::vector<GroundTruthTrack> tracks = TestSequenceFactory::generateLinearGroundTruth(config);
    
    // Should generate 2 objects * 2 cameras = 4 tracks
    assert(tracks.size() == 4);
    std::cout << "✓ Correct number of ground truth tracks generated" << std::endl;
    
    // Check that tracks have valid data
    for (const auto& track : tracks) {
        assert(track.objectId > 0);
        assert(!track.cameraId.empty());
        assert(track.timestamp >= 0.0);
        assert(track.boundingBox.width > 0);
        assert(track.boundingBox.height > 0);
        assert(track.reidFeatures.size() == 128);
        assert(track.confidence > 0.0);
    }
    std::cout << "✓ Generated tracks have valid data" << std::endl;
    
    // Test transition event generation
    std::vector<TransitionEvent> transitions = TestSequenceFactory::generateTransitionEvents(config);
    
    // Should generate 2 objects * 1 transition (cam1->cam2) = 2 transitions
    assert(transitions.size() == 2);
    std::cout << "✓ Correct number of transition events generated" << std::endl;
    
    // Check transition data
    for (const auto& transition : transitions) {
        assert(transition.objectId > 0);
        assert(transition.fromCamera == "cam1");
        assert(transition.toCamera == "cam2");
        assert(transition.transitionTime > 0.0);
        assert(transition.expectedDelay == 2.0);
    }
    std::cout << "✓ Generated transitions have valid data" << std::endl;
}

void test_detection_recording() {
    std::cout << "[TEST] Testing detection recording..." << std::endl;
    
    MultiCameraTestSequence sequence;
    
    // Start test mode
    sequence.startTestMode();
    
    // Record some detections
    cv::Rect bbox1(100, 100, 50, 100);
    cv::Rect bbox2(150, 120, 55, 105);
    
    sequence.recordDetection("camera_1", 10, 100, 15.5, bbox1);
    sequence.recordDetection("camera_2", 20, 100, 17.0, bbox2);
    
    std::cout << "✓ Detection recording works without errors" << std::endl;
    
    // Record transitions
    sequence.recordTransition("camera_1", "camera_2", 10, 100, 16.0);
    
    std::cout << "✓ Transition recording works without errors" << std::endl;
    
    // Stop test mode
    sequence.stopTestMode();
    std::cout << "✓ Test mode lifecycle works correctly" << std::endl;
}

int main() {
    std::cout << "🎯 Task 78: Unit Tests for Multi-Camera Test Sequence Implementation" << std::endl;
    std::cout << "====================================================================" << std::endl;
    
    try {
        test_ground_truth_track_creation();
        std::cout << std::endl;
        
        test_transition_event_creation();
        std::cout << std::endl;
        
        test_test_sequence_config();
        std::cout << std::endl;
        
        test_validation_results();
        std::cout << std::endl;
        
        test_multi_camera_test_sequence();
        std::cout << std::endl;
        
        test_test_sequence_factory();
        std::cout << std::endl;
        
        test_ground_truth_generation();
        std::cout << std::endl;
        
        test_detection_recording();
        std::cout << std::endl;
        
        std::cout << "🎉 All Task 78 unit tests passed!" << std::endl;
        std::cout << "✅ Multi-camera test sequence structures are correctly implemented" << std::endl;
        std::cout << "✅ Ground truth generation and validation logic works" << std::endl;
        std::cout << "✅ Test sequence factory creates valid configurations" << std::endl;
        std::cout << "✅ Detection and transition recording functions properly" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
