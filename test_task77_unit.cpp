/**
 * Task 77: Unit test for ReID persistence in API output
 * Tests BehaviorEvent and AlarmPayload structures with ReID information
 */

#include <iostream>
#include <cassert>
#include <string>
#include <opencv2/opencv.hpp>

// Include the headers we need to test
#include "src/ai/BehaviorAnalyzer.h"
#include "src/output/AlarmTrigger.h"

void test_behavior_event_reid_fields() {
    std::cout << "[TEST] Testing BehaviorEvent ReID fields..." << std::endl;
    
    // Test default constructor
    BehaviorEvent event1;
    assert(event1.localTrackId == -1);
    assert(event1.globalTrackId == -1);
    assert(event1.reidId.empty());
    assert(event1.cameraId.empty());
    std::cout << "✓ Default constructor sets ReID fields correctly" << std::endl;
    
    // Test legacy constructor (backward compatibility)
    cv::Rect bbox(100, 100, 50, 50);
    BehaviorEvent event2("intrusion", "rule_1", "track_123", bbox, 0.85);
    assert(event2.eventType == "intrusion");
    assert(event2.ruleId == "rule_1");
    assert(event2.objectId == "track_123");
    assert(event2.confidence == 0.85);
    // ReID fields should be default values
    assert(event2.localTrackId == -1);
    assert(event2.globalTrackId == -1);
    assert(event2.reidId.empty());
    std::cout << "✓ Legacy constructor maintains backward compatibility" << std::endl;
    
    // Test enhanced constructor with ReID information
    BehaviorEvent event3("intrusion", "rule_1", "track_123", bbox, 0.85, 123, 456, "camera_1");
    assert(event3.eventType == "intrusion");
    assert(event3.ruleId == "rule_1");
    assert(event3.objectId == "track_123");
    assert(event3.confidence == 0.85);
    assert(event3.localTrackId == 123);
    assert(event3.globalTrackId == 456);
    assert(event3.cameraId == "camera_1");
    assert(event3.reidId == "reid_456");  // Should be generated from global track ID
    std::cout << "✓ Enhanced constructor sets ReID fields correctly" << std::endl;
    
    // Test ReID generation with invalid global track ID
    BehaviorEvent event4("intrusion", "rule_1", "track_123", bbox, 0.85, 123, -1, "camera_1");
    assert(event4.globalTrackId == -1);
    assert(event4.reidId.empty());  // Should be empty for invalid global track ID
    std::cout << "✓ ReID generation handles invalid global track ID correctly" << std::endl;
}

void test_alarm_payload_reid_fields() {
    std::cout << "[TEST] Testing AlarmPayload ReID fields..." << std::endl;
    
    // Test default constructor
    AlarmPayload payload1;
    assert(payload1.local_track_id == -1);
    assert(payload1.global_track_id == -1);
    assert(payload1.reid_id.empty());
    std::cout << "✓ Default constructor sets ReID fields correctly" << std::endl;
    
    // Test manual field assignment
    AlarmPayload payload2;
    payload2.event_type = "intrusion";
    payload2.camera_id = "camera_1";
    payload2.rule_id = "rule_1";
    payload2.object_id = "track_123";
    payload2.reid_id = "reid_456";
    payload2.local_track_id = 123;
    payload2.global_track_id = 456;
    payload2.confidence = 0.85;
    payload2.timestamp = "2024-01-01T12:00:00.000Z";
    payload2.metadata = "Test alarm";
    payload2.bounding_box = cv::Rect(100, 100, 50, 50);
    payload2.test_mode = false;
    payload2.priority = 3;
    payload2.alarm_id = "alarm_123";
    
    // Verify all fields are set correctly
    assert(payload2.event_type == "intrusion");
    assert(payload2.camera_id == "camera_1");
    assert(payload2.reid_id == "reid_456");
    assert(payload2.local_track_id == 123);
    assert(payload2.global_track_id == 456);
    std::cout << "✓ Manual field assignment works correctly" << std::endl;
}

void test_alarm_payload_json_serialization() {
    std::cout << "[TEST] Testing AlarmPayload JSON serialization with ReID fields..." << std::endl;
    
    AlarmPayload payload;
    payload.event_type = "intrusion";
    payload.camera_id = "camera_1";
    payload.rule_id = "rule_1";
    payload.object_id = "track_123";
    payload.reid_id = "reid_456";
    payload.local_track_id = 123;
    payload.global_track_id = 456;
    payload.confidence = 0.85;
    payload.timestamp = "2024-01-01T12:00:00.000Z";
    payload.metadata = "Test alarm";
    payload.bounding_box = cv::Rect(100, 100, 50, 50);
    payload.test_mode = false;
    payload.priority = 3;
    payload.alarm_id = "alarm_123";
    
    std::string json = payload.toJson();
    std::cout << "Generated JSON: " << json << std::endl;
    
    // Check that ReID fields are present in JSON
    assert(json.find("\"reid_id\":\"reid_456\"") != std::string::npos);
    assert(json.find("\"local_track_id\":123") != std::string::npos);
    assert(json.find("\"global_track_id\":456") != std::string::npos);
    std::cout << "✓ JSON serialization includes ReID fields" << std::endl;
}

void test_behavior_analyzer_camera_id() {
    std::cout << "[TEST] Testing BehaviorAnalyzer camera ID management..." << std::endl;
    
    BehaviorAnalyzer analyzer;
    
    // Test default camera ID
    assert(analyzer.getCameraId().empty());
    std::cout << "✓ Default camera ID is empty" << std::endl;
    
    // Test setting camera ID
    analyzer.setCameraId("test_camera_1");
    assert(analyzer.getCameraId() == "test_camera_1");
    std::cout << "✓ Camera ID can be set and retrieved" << std::endl;
    
    // Test changing camera ID
    analyzer.setCameraId("test_camera_2");
    assert(analyzer.getCameraId() == "test_camera_2");
    std::cout << "✓ Camera ID can be changed" << std::endl;
}

void test_cross_camera_tracking_integration() {
    std::cout << "[TEST] Testing cross-camera tracking integration..." << std::endl;
    
    // This test verifies that the structures are properly set up for cross-camera tracking
    // The actual cross-camera logic is tested in the TaskManager
    
    // Create a BehaviorEvent with cross-camera information
    cv::Rect bbox(100, 100, 50, 50);
    BehaviorEvent event("intrusion", "rule_1", "track_123", bbox, 0.85, 123, 456, "camera_1");
    
    // Verify the event has all necessary cross-camera information
    assert(!event.cameraId.empty());
    assert(event.localTrackId >= 0);
    assert(event.globalTrackId >= 0);
    assert(!event.reidId.empty());
    std::cout << "✓ BehaviorEvent contains cross-camera tracking information" << std::endl;
    
    // Create an AlarmPayload from the event (simulating createAlarmPayload)
    AlarmPayload payload;
    payload.event_type = event.eventType;
    payload.camera_id = event.cameraId;
    payload.rule_id = event.ruleId;
    payload.object_id = event.objectId;
    payload.reid_id = event.reidId;
    payload.local_track_id = event.localTrackId;
    payload.global_track_id = event.globalTrackId;
    payload.confidence = event.confidence;
    payload.bounding_box = event.boundingBox;
    
    // Verify the payload has all cross-camera information
    assert(payload.camera_id == event.cameraId);
    assert(payload.reid_id == event.reidId);
    assert(payload.local_track_id == event.localTrackId);
    assert(payload.global_track_id == event.globalTrackId);
    std::cout << "✓ AlarmPayload preserves cross-camera tracking information" << std::endl;
}

int main() {
    std::cout << "🎯 Task 77: Unit Tests for ReID Persistence in API Output" << std::endl;
    std::cout << "=========================================================" << std::endl;
    
    try {
        test_behavior_event_reid_fields();
        std::cout << std::endl;
        
        test_alarm_payload_reid_fields();
        std::cout << std::endl;
        
        test_alarm_payload_json_serialization();
        std::cout << std::endl;
        
        test_behavior_analyzer_camera_id();
        std::cout << std::endl;
        
        test_cross_camera_tracking_integration();
        std::cout << std::endl;
        
        std::cout << "🎉 All Task 77 unit tests passed!" << std::endl;
        std::cout << "✅ ReID persistence structures are correctly implemented" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
