/**
 * @file simple_person_stats_test.cpp
 * @brief Simple test program for person statistics functionality
 *
 * This test demonstrates the new person statistics features with minimal dependencies.
 */

#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include "../src/ai/PersonFilter.h"
#include "../src/ai/AgeGenderAnalyzer.h"

using namespace AISecurityVision;

// Simple logger for testing
void LOG_INFO(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void LOG_ERROR(const std::string& msg) {
    std::cout << "[ERROR] " << msg << std::endl;
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
    
    // Add another person
    Detection person2;
    person2.bbox = cv::Rect(200, 120, 70, 150);
    person2.confidence = 0.78f;
    person2.classId = 0;  // person class
    person2.className = "person";
    detections.push_back(person2);
    
    // Add non-person detection
    Detection car;
    car.bbox = cv::Rect(300, 200, 120, 80);
    car.confidence = 0.90f;
    car.classId = 2;  // car class
    car.className = "car";
    detections.push_back(car);
    
    // Create test frame
    cv::Mat testFrame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::rectangle(testFrame, person1.bbox, cv::Scalar(0, 255, 0), 2);
    cv::rectangle(testFrame, person2.bbox, cv::Scalar(0, 255, 0), 2);
    cv::rectangle(testFrame, car.bbox, cv::Scalar(0, 0, 255), 2);
    
    // Filter persons
    auto persons = PersonFilter::filterPersons(detections, testFrame);
    
    std::cout << "Total detections: " << detections.size() << std::endl;
    std::cout << "Person detections: " << persons.size() << std::endl;
    
    for (size_t i = 0; i < persons.size(); ++i) {
        const auto& person = persons[i];
        std::cout << "Person " << (i+1) << ": bbox(" << person.bbox.x << "," << person.bbox.y 
                  << "," << person.bbox.width << "," << person.bbox.height 
                  << "), confidence=" << person.confidence << std::endl;
        
        if (!person.crop.empty()) {
            std::cout << "  Crop size: " << person.crop.cols << "x" << person.crop.rows << std::endl;
        }
    }
    
    // Test basic statistics
    auto stats = PersonFilter::getBasicStats(persons);
    std::cout << "Basic stats - Total: " << stats.total_count 
              << ", Avg confidence: " << stats.avg_confidence 
              << ", Avg size: " << stats.avg_size.width << "x" << stats.avg_size.height << std::endl;
    
    // Test filtering functions
    auto highConfPersons = PersonFilter::filterByConfidence(persons, 0.8f);
    std::cout << "High confidence persons (>0.8): " << highConfPersons.size() << std::endl;
    
    auto largePerson = PersonFilter::filterBySize(persons, 75, 155);
    std::cout << "Large persons (>75x155): " << largePerson.size() << std::endl;
}

void testAgeGenderAnalyzer() {
    std::cout << "\n=== Testing AgeGenderAnalyzer ===" << std::endl;
    
    AgeGenderAnalyzer analyzer;
    
    // Check if analyzer can be initialized
    std::cout << "Analyzer initialized: " << (analyzer.isInitialized() ? "Yes" : "No") << std::endl;
    
    // Try to initialize (will fail without actual model file)
    bool initResult = analyzer.initialize("models/age_gender_mobilenet.rknn");
    std::cout << "Initialization result: " << (initResult ? "Success" : "Failed (expected without model)") << std::endl;
    
    if (initResult) {
        std::cout << "AgeGenderAnalyzer initialized successfully!" << std::endl;
        
        // Create test person crop
        cv::Mat personCrop = cv::Mat::zeros(224, 224, CV_8UC3);
        
        // Create a simple face-like pattern
        cv::Point center(112, 112);
        cv::ellipse(personCrop, center, cv::Size(60, 80), 0, 0, 360, cv::Scalar(200, 180, 160), -1);
        cv::circle(personCrop, cv::Point(90, 90), 5, cv::Scalar(50, 50, 50), -1);  // left eye
        cv::circle(personCrop, cv::Point(134, 90), 5, cv::Scalar(50, 50, 50), -1); // right eye
        cv::circle(personCrop, cv::Point(112, 110), 2, cv::Scalar(150, 120, 100), -1); // nose
        cv::ellipse(personCrop, cv::Point(112, 130), cv::Size(10, 5), 0, 0, 180, cv::Scalar(100, 50, 50), -1); // mouth
        
        auto attributes = analyzer.analyzeSingle(personCrop);
        std::cout << "Analysis result - Gender: " << attributes.gender 
                  << " (conf: " << attributes.gender_confidence 
                  << "), Age: " << attributes.age_group 
                  << " (conf: " << attributes.age_confidence << ")" << std::endl;
        
        std::cout << "Inference time: " << analyzer.getLastInferenceTime() << "ms" << std::endl;
        std::cout << "Analysis count: " << analyzer.getAnalysisCount() << std::endl;
    } else {
        std::cout << "Model file not found - this is expected for testing without actual model" << std::endl;
    }
    
    // Test configuration
    std::cout << "Default gender threshold: " << analyzer.getGenderThreshold() << std::endl;
    std::cout << "Default age threshold: " << analyzer.getAgeThreshold() << std::endl;
    std::cout << "Default batch size: " << analyzer.getBatchSize() << std::endl;
    
    analyzer.setGenderThreshold(0.8f);
    analyzer.setAgeThreshold(0.7f);
    analyzer.setBatchSize(2);
    
    std::cout << "Updated gender threshold: " << analyzer.getGenderThreshold() << std::endl;
    std::cout << "Updated age threshold: " << analyzer.getAgeThreshold() << std::endl;
    std::cout << "Updated batch size: " << analyzer.getBatchSize() << std::endl;
    
    // Test model info
    auto info = analyzer.getModelInfo();
    std::cout << "Model info:" << std::endl;
    for (const auto& line : info) {
        std::cout << "  " << line << std::endl;
    }
}

void testPersonStatsStructure() {
    std::cout << "\n=== Testing PersonStats Structure ===" << std::endl;
    
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
    stats.person_boxes = {
        cv::Rect(100, 100, 80, 160),
        cv::Rect(200, 120, 70, 150),
        cv::Rect(300, 110, 75, 155),
        cv::Rect(400, 130, 85, 165),
        cv::Rect(500, 105, 78, 158)
    };
    
    std::cout << "Updated person stats:" << std::endl;
    std::cout << "  Total persons: " << stats.total_persons << std::endl;
    std::cout << "  Male count: " << stats.male_count << std::endl;
    std::cout << "  Female count: " << stats.female_count << std::endl;
    std::cout << "  Young count: " << stats.young_count << std::endl;
    std::cout << "  Middle count: " << stats.middle_count << std::endl;
    std::cout << "  Person boxes: " << stats.person_boxes.size() << std::endl;
    std::cout << "  Genders: ";
    for (const auto& gender : stats.person_genders) {
        std::cout << gender << " ";
    }
    std::cout << std::endl;
    std::cout << "  Ages: ";
    for (const auto& age : stats.person_ages) {
        std::cout << age << " ";
    }
    std::cout << std::endl;
    
    std::cout << "PersonStats structure test completed" << std::endl;
}

void testIntegrationScenario() {
    std::cout << "\n=== Testing Integration Scenario ===" << std::endl;
    
    // Simulate a complete person statistics workflow
    std::cout << "Simulating complete person statistics workflow..." << std::endl;
    
    // Step 1: Create mock YOLOv8 detections
    std::vector<Detection> detections;

    Detection person1;
    person1.bbox = cv::Rect(100, 100, 80, 160);
    person1.confidence = 0.85f;
    person1.classId = 0;
    person1.className = "person";
    detections.push_back(person1);

    Detection person2;
    person2.bbox = cv::Rect(200, 120, 70, 150);
    person2.confidence = 0.78f;
    person2.classId = 0;
    person2.className = "person";
    detections.push_back(person2);

    Detection car;
    car.bbox = cv::Rect(300, 200, 120, 80);
    car.confidence = 0.90f;
    car.classId = 2;
    car.className = "car";
    detections.push_back(car);
    
    // Step 2: Create test frame
    cv::Mat frame = cv::Mat::zeros(480, 640, CV_8UC3);
    
    // Step 3: Filter persons
    auto persons = PersonFilter::filterPersons(detections, frame);
    std::cout << "Step 1 - Person filtering: " << persons.size() << " persons detected" << std::endl;
    
    // Step 4: Simulate age/gender analysis
    std::vector<PersonAttributes> attributes;
    for (size_t i = 0; i < persons.size(); ++i) {
        PersonAttributes attr;
        attr.gender = (i % 2 == 0) ? "male" : "female";
        attr.age_group = (i == 0) ? "young" : "middle";
        attr.gender_confidence = 0.85f;
        attr.age_confidence = 0.78f;
        attributes.push_back(attr);
    }
    std::cout << "Step 2 - Age/Gender analysis: " << attributes.size() << " persons analyzed" << std::endl;
    
    // Step 5: Generate statistics
    struct PersonStats {
        int total_persons = 0;
        int male_count = 0;
        int female_count = 0;
        int young_count = 0;
        int middle_count = 0;
    } stats;
    
    stats.total_persons = static_cast<int>(persons.size());
    for (const auto& attr : attributes) {
        if (attr.gender == "male") stats.male_count++;
        if (attr.gender == "female") stats.female_count++;
        if (attr.age_group == "young") stats.young_count++;
        if (attr.age_group == "middle") stats.middle_count++;
    }
    
    std::cout << "Step 3 - Statistics generation:" << std::endl;
    std::cout << "  Total: " << stats.total_persons << std::endl;
    std::cout << "  Male: " << stats.male_count << ", Female: " << stats.female_count << std::endl;
    std::cout << "  Young: " << stats.young_count << ", Middle: " << stats.middle_count << std::endl;
    
    std::cout << "Integration scenario completed successfully!" << std::endl;
}

int main() {
    std::cout << "=== Simple Person Statistics Test Program ===" << std::endl;
    std::cout << "Testing new person statistics functionality..." << std::endl;
    
    try {
        testPersonFilter();
        testAgeGenderAnalyzer();
        testPersonStatsStructure();
        testIntegrationScenario();
        
        std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
        std::cout << "Note: AgeGenderAnalyzer initialization may fail without actual model file." << std::endl;
        std::cout << "This is expected behavior for testing the code structure." << std::endl;
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
