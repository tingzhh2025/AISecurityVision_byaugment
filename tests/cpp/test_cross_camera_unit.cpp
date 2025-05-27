#include "src/core/TaskManager.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cassert>

/**
 * Unit test for Task 75: Cross-Camera Tracking functionality
 * Tests the TaskManager's cross-camera tracking logic
 */

void testCrossCameraTrackCreation() {
    std::cout << "[TEST] Testing CrossCameraTrack creation..." << std::endl;
    
    std::vector<float> testFeatures = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    cv::Rect testBbox(100, 100, 50, 100);
    
    CrossCameraTrack track(1, "camera_1", 10, testFeatures, testBbox, 0, 0.8f);
    
    assert(track.globalTrackId == 1);
    assert(track.primaryCameraId == "camera_1");
    assert(track.reidFeatures.size() == 5);
    assert(track.hasCamera("camera_1"));
    assert(track.getLocalTrackId("camera_1") == 10);
    assert(!track.isExpired(30.0));
    
    std::cout << "[PASS] CrossCameraTrack creation test passed" << std::endl;
}

void testCrossCameraTrackUpdate() {
    std::cout << "[TEST] Testing CrossCameraTrack update..." << std::endl;
    
    std::vector<float> initialFeatures = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    std::vector<float> updateFeatures = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    cv::Rect initialBbox(100, 100, 50, 100);
    cv::Rect updateBbox(110, 105, 55, 105);
    
    CrossCameraTrack track(1, "camera_1", 10, initialFeatures, initialBbox, 0, 0.8f);
    
    // Update with new camera
    track.updateTrack("camera_2", 20, updateFeatures, updateBbox, 0.9f);
    
    assert(track.hasCamera("camera_1"));
    assert(track.hasCamera("camera_2"));
    assert(track.getLocalTrackId("camera_2") == 20);
    assert(track.confidence == 0.9f);
    
    std::cout << "[PASS] CrossCameraTrack update test passed" << std::endl;
}

void testTaskManagerCrossCameraTracking() {
    std::cout << "[TEST] Testing TaskManager cross-camera tracking..." << std::endl;
    
    TaskManager& manager = TaskManager::getInstance();
    
    // Configure cross-camera tracking
    manager.setCrossCameraTrackingEnabled(true);
    manager.setReIDSimilarityThreshold(0.7f);
    manager.setMaxTrackAge(30.0);
    
    assert(manager.isCrossCameraTrackingEnabled());
    assert(manager.getReIDSimilarityThreshold() == 0.7f);
    assert(manager.getMaxTrackAge() == 30.0);
    
    // Test track reporting
    std::vector<float> features1 = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    std::vector<float> features2 = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f}; // Similar features
    std::vector<float> features3 = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f}; // Different features
    
    cv::Rect bbox1(100, 100, 50, 100);
    cv::Rect bbox2(200, 200, 55, 105);
    cv::Rect bbox3(300, 300, 60, 110);
    
    // Report first track from camera 1
    manager.reportTrackUpdate("camera_1", 1, features1, bbox1, 0, 0.8f);
    int globalId1 = manager.getGlobalTrackId("camera_1", 1);
    assert(globalId1 > 0);
    
    // Report similar track from camera 2 (should match)
    manager.reportTrackUpdate("camera_2", 1, features2, bbox2, 0, 0.9f);
    int globalId2 = manager.getGlobalTrackId("camera_2", 1);
    
    // Report different track from camera 3 (should not match)
    manager.reportTrackUpdate("camera_3", 1, features3, bbox3, 0, 0.7f);
    int globalId3 = manager.getGlobalTrackId("camera_3", 1);
    
    // Check statistics
    size_t globalTrackCount = manager.getGlobalTrackCount();
    size_t activeTrackCount = manager.getActiveCrossCameraTrackCount();
    
    std::cout << "Global tracks: " << globalTrackCount << std::endl;
    std::cout << "Active tracks: " << activeTrackCount << std::endl;
    std::cout << "Global ID 1: " << globalId1 << std::endl;
    std::cout << "Global ID 2: " << globalId2 << std::endl;
    std::cout << "Global ID 3: " << globalId3 << std::endl;
    
    assert(globalTrackCount >= 1);
    assert(activeTrackCount >= 1);
    
    std::cout << "[PASS] TaskManager cross-camera tracking test passed" << std::endl;
}

void testReIDMatching() {
    std::cout << "[TEST] Testing ReID matching..." << std::endl;
    
    TaskManager& manager = TaskManager::getInstance();
    
    // Reset tracking state
    manager.resetCrossCameraTrackingStats();
    
    std::vector<float> queryFeatures = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    
    // Find matches
    auto matches = manager.findReIDMatches(queryFeatures, "camera_1");
    
    std::cout << "Found " << matches.size() << " ReID matches" << std::endl;
    
    for (const auto& match : matches) {
        std::cout << "Match: Global ID " << match.globalTrackId 
                  << ", Similarity: " << match.similarity
                  << ", Camera: " << match.matchedCameraId
                  << ", Local ID: " << match.matchedLocalTrackId << std::endl;
    }
    
    std::cout << "[PASS] ReID matching test passed" << std::endl;
}

void testCrossCameraTrackExpiration() {
    std::cout << "[TEST] Testing cross-camera track expiration..." << std::endl;
    
    std::vector<float> testFeatures = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    cv::Rect testBbox(100, 100, 50, 100);
    
    CrossCameraTrack track(999, "test_camera", 999, testFeatures, testBbox, 0, 0.8f);
    
    // Track should not be expired initially
    assert(!track.isExpired(1.0));
    
    // Wait a bit and test expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    assert(track.isExpired(1.0)); // Should be expired after 1 second
    
    std::cout << "[PASS] Cross-camera track expiration test passed" << std::endl;
}

void testCrossCameraConfiguration() {
    std::cout << "[TEST] Testing cross-camera configuration..." << std::endl;
    
    TaskManager& manager = TaskManager::getInstance();
    
    // Test configuration changes
    manager.setCrossCameraTrackingEnabled(false);
    assert(!manager.isCrossCameraTrackingEnabled());
    
    manager.setCrossCameraTrackingEnabled(true);
    assert(manager.isCrossCameraTrackingEnabled());
    
    manager.setReIDSimilarityThreshold(0.85f);
    assert(manager.getReIDSimilarityThreshold() == 0.85f);
    
    manager.setMaxTrackAge(60.0);
    assert(manager.getMaxTrackAge() == 60.0);
    
    std::cout << "[PASS] Cross-camera configuration test passed" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Task 75: Cross-Camera Tracking Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        testCrossCameraTrackCreation();
        testCrossCameraTrackUpdate();
        testTaskManagerCrossCameraTracking();
        testReIDMatching();
        testCrossCameraTrackExpiration();
        testCrossCameraConfiguration();
        
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "✅ All Task 75 unit tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
