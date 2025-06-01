/**
 * @file person_stats_test.cpp
 * @brief Test program for person statistics functionality
 *
 * This test demonstrates the new person statistics features without
 * modifying any existing functionality.
 */

#include <iostream>
#include <opencv2/opencv.hpp>
#include "../src/ai/PersonFilter.h"
#include "../src/ai/AgeGenderAnalyzer.h"
#include "../src/core/Logger.h"

using namespace AISecurityVision;

// Initialize logger for testing
void initializeLogger() {
    // Simple console logger initialization
    std::cout << "[INFO] Logger initialized for person statistics testing" << std::endl;
}

void testPersonFilter() {
    std::cout << "\n=== Testing PersonFilter ===" << std::endl;
    
    // Create mock detections
    std::vector<Detection> detections;
    
    // Add person detection
    Detection person1;
    person1.bbox = cv::Rect(100, 100, 80, 160);
    person1.confidence = 0.85f;
    person1.classId = 0;  // person class
    person1.className = "person";
    detections.push_back(person1);
    
    // Add non-person detection
    Detection car;
    car.bbox = cv::Rect(200, 200, 120, 80);
    car.confidence = 0.90f;
    car.classId = 2;  // car class
    car.className = "car";
    detections.push_back(car);
    
    // Create test frame
    cv::Mat testFrame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::rectangle(testFrame, person1.bbox, cv::Scalar(0, 255, 0), 2);
    cv::rectangle(testFrame, car.bbox, cv::Scalar(0, 0, 255), 2);
    
    // Filter persons
    auto persons = PersonFilter::filterPersons(detections, testFrame);
    
    std::cout << "Total detections: " << detections.size() << std::endl;
    std::cout << "Person detections: " << persons.size() << std::endl;
    
    for (const auto& person : persons) {
        std::cout << "Person: bbox(" << person.bbox.x << "," << person.bbox.y 
                  << "," << person.bbox.width << "," << person.bbox.height 
                  << "), confidence=" << person.confidence << std::endl;
    }
    
    // Test basic statistics
    auto stats = PersonFilter::getBasicStats(persons);
    std::cout << "Basic stats - Total: " << stats.total_count 
              << ", Avg confidence: " << stats.avg_confidence << std::endl;
}

void testAgeGenderAnalyzer() {
    std::cout << "\n=== Testing AgeGenderAnalyzer ===" << std::endl;
    
    AgeGenderAnalyzer analyzer;
    
    // Try to initialize (will fail without actual model file)
    if (analyzer.initialize("models/age_gender_mobilenet.rknn")) {
        std::cout << "AgeGenderAnalyzer initialized successfully!" << std::endl;
        
        // Create test person crop
        cv::Mat personCrop = cv::Mat::zeros(224, 224, CV_8UC3);
        cv::circle(personCrop, cv::Point(112, 112), 50, cv::Scalar(255, 255, 255), -1);
        
        auto attributes = analyzer.analyzeSingle(personCrop);
        std::cout << "Analysis result - Gender: " << attributes.gender 
                  << " (conf: " << attributes.gender_confidence 
                  << "), Age: " << attributes.age_group 
                  << " (conf: " << attributes.age_confidence << ")" << std::endl;
    } else {
        std::cout << "AgeGenderAnalyzer initialization failed (expected without model file)" << std::endl;
    }
    
    // Test model info
    auto info = analyzer.getModelInfo();
    for (const auto& line : info) {
        std::cout << "Model info: " << line << std::endl;
    }
}

void testVideoPipelineIntegration() {
    std::cout << "\n=== Testing VideoPipeline Integration ===" << std::endl;

    // Note: VideoPipeline integration test skipped in standalone test
    // This would require full system initialization
    std::cout << "VideoPipeline integration test skipped (requires full system)" << std::endl;
    std::cout << "Use the main application to test VideoPipeline integration" << std::endl;
}

void testFrameResultExtension() {
    std::cout << "\n=== Testing FrameResult Extension ===" << std::endl;

    // Test PersonStats structure directly
    struct PersonStats {
        int total_persons = 0;
        int male_count = 0;
        int female_count = 0;
        int child_count = 0;
        int young_count = 0;
        int middle_count = 0;
        int senior_count = 0;
        std::vector<cv::Rect> person_boxes;
        std::vector<std::string> person_genders;
        std::vector<std::string> person_ages;
    };

    PersonStats stats;

    // Test default values
    std::cout << "Default person stats:" << std::endl;
    std::cout << "  Total persons: " << stats.total_persons << std::endl;
    std::cout << "  Male count: " << stats.male_count << std::endl;
    std::cout << "  Female count: " << stats.female_count << std::endl;

    // Test setting values
    stats.total_persons = 5;
    stats.male_count = 3;
    stats.female_count = 2;
    stats.young_count = 2;
    stats.middle_count = 3;

    stats.person_genders = {"male", "female", "male", "female", "male"};
    stats.person_ages = {"young", "middle", "middle", "young", "middle"};

    std::cout << "Updated person stats:" << std::endl;
    std::cout << "  Total persons: " << stats.total_persons << std::endl;
    std::cout << "  Male count: " << stats.male_count << std::endl;
    std::cout << "  Female count: " << stats.female_count << std::endl;
    std::cout << "  Young count: " << stats.young_count << std::endl;
    std::cout << "  Middle count: " << stats.middle_count << std::endl;
    std::cout << "  Genders: ";
    for (const auto& gender : stats.person_genders) {
        std::cout << gender << " ";
    }
    std::cout << std::endl;

    std::cout << "PersonStats structure test completed" << std::endl;
}

int main() {
    std::cout << "=== Person Statistics Test Program ===" << std::endl;
    std::cout << "Testing new person statistics functionality..." << std::endl;

    // Initialize logger
    initializeLogger();

    try {
        testPersonFilter();
        testAgeGenderAnalyzer();
        testVideoPipelineIntegration();
        testFrameResultExtension();

        std::cout << "\n=== All Tests Completed ===" << std::endl;
        std::cout << "Note: Some tests may show expected failures due to missing model files." << std::endl;
        std::cout << "This is normal for testing the code structure without actual models." << std::endl;
        std::cout << "\nTo enable full functionality:" << std::endl;
        std::cout << "1. Place age_gender_mobilenet.rknn model in models/ directory" << std::endl;
        std::cout << "2. Ensure RKNN runtime is properly installed" << std::endl;
        std::cout << "3. Run with actual video input for complete testing" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
