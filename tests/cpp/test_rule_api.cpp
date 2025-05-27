#include "src/api/APIService.h"
#include "src/ai/BehaviorAnalyzer.h"
#include <iostream>
#include <cassert>

void testROISerialization() {
    std::cout << "Testing ROI serialization..." << std::endl;
    
    // Create a test ROI
    ROI roi;
    roi.id = "test_roi";
    roi.name = "Test Intrusion Zone";
    roi.polygon = {cv::Point(100, 100), cv::Point(500, 100), cv::Point(500, 400), cv::Point(100, 400)};
    roi.enabled = true;
    roi.priority = 3;
    
    // Create APIService instance
    APIService apiService(8080);
    
    // Test serialization
    std::string serialized = apiService.serializeROI(roi);
    std::cout << "Serialized ROI: " << serialized << std::endl;
    
    // Test deserialization
    ROI deserializedROI;
    bool success = apiService.deserializeROI(serialized, deserializedROI);
    
    assert(success);
    assert(deserializedROI.id == roi.id);
    assert(deserializedROI.name == roi.name);
    assert(deserializedROI.enabled == roi.enabled);
    assert(deserializedROI.priority == roi.priority);
    assert(deserializedROI.polygon.size() == roi.polygon.size());
    
    std::cout << "✅ ROI serialization test passed!" << std::endl;
}

void testIntrusionRuleSerialization() {
    std::cout << "Testing IntrusionRule serialization..." << std::endl;
    
    // Create a test rule
    ROI roi;
    roi.id = "test_roi";
    roi.name = "Test Zone";
    roi.polygon = {cv::Point(50, 50), cv::Point(200, 50), cv::Point(200, 200), cv::Point(50, 200)};
    roi.enabled = true;
    roi.priority = 2;
    
    IntrusionRule rule;
    rule.id = "test_rule";
    rule.roi = roi;
    rule.minDuration = 10.5;
    rule.confidence = 0.85;
    rule.enabled = true;
    
    // Create APIService instance
    APIService apiService(8080);
    
    // Test serialization
    std::string serialized = apiService.serializeIntrusionRule(rule);
    std::cout << "Serialized Rule: " << serialized << std::endl;
    
    // Test deserialization
    IntrusionRule deserializedRule;
    bool success = apiService.deserializeIntrusionRule(serialized, deserializedRule);
    
    assert(success);
    assert(deserializedRule.id == rule.id);
    assert(deserializedRule.minDuration == rule.minDuration);
    assert(deserializedRule.confidence == rule.confidence);
    assert(deserializedRule.enabled == rule.enabled);
    assert(deserializedRule.roi.id == rule.roi.id);
    assert(deserializedRule.roi.name == rule.roi.name);
    
    std::cout << "✅ IntrusionRule serialization test passed!" << std::endl;
}

void testPolygonValidation() {
    std::cout << "Testing polygon validation..." << std::endl;
    
    APIService apiService(8080);
    
    // Test valid polygon
    std::vector<cv::Point> validPolygon = {cv::Point(0, 0), cv::Point(100, 0), cv::Point(50, 100)};
    assert(apiService.validateROIPolygon(validPolygon));
    
    // Test invalid polygon (too few points)
    std::vector<cv::Point> invalidPolygon = {cv::Point(0, 0), cv::Point(100, 0)};
    assert(!apiService.validateROIPolygon(invalidPolygon));
    
    // Test invalid polygon (out of range coordinates)
    std::vector<cv::Point> outOfRangePolygon = {cv::Point(-10, 0), cv::Point(100, 0), cv::Point(50, 100)};
    assert(!apiService.validateROIPolygon(outOfRangePolygon));
    
    std::cout << "✅ Polygon validation test passed!" << std::endl;
}

void testAPIHandlers() {
    std::cout << "Testing API handlers..." << std::endl;
    
    APIService apiService(8080);
    
    // Test POST rules handler with valid JSON
    std::string validRuleJson = R"({
        "id": "new_rule",
        "roi": {
            "id": "new_roi",
            "name": "New Test Zone",
            "polygon": [{"x": 10, "y": 10}, {"x": 100, "y": 10}, {"x": 100, "y": 100}, {"x": 10, "y": 100}],
            "enabled": true,
            "priority": 1
        },
        "min_duration": 5.0,
        "confidence": 0.7,
        "enabled": true
    })";
    
    std::string response;
    apiService.handlePostRules(validRuleJson, response);
    std::cout << "POST Rules Response: " << response.substr(0, 200) << "..." << std::endl;
    
    // Test GET rules handler
    apiService.handleGetRules("", response);
    std::cout << "GET Rules Response: " << response.substr(0, 200) << "..." << std::endl;
    
    // Test GET specific rule handler
    apiService.handleGetRule("", response, "default_intrusion");
    std::cout << "GET Rule Response: " << response.substr(0, 200) << "..." << std::endl;
    
    std::cout << "✅ API handlers test completed!" << std::endl;
}

int main() {
    std::cout << "=== Behavior Rule API Test Suite ===" << std::endl;
    
    try {
        testROISerialization();
        testIntrusionRuleSerialization();
        testPolygonValidation();
        testAPIHandlers();
        
        std::cout << "\n🎉 All tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
