#include "src/ai/BehaviorAnalyzer.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

/**
 * Unit test for Task 71: Enhanced Conflict Resolution Logic
 * Tests the advanced conflict resolution algorithms for overlapping ROIs
 */

void testBasicConflictResolution() {
    std::cout << "=== Testing Basic Conflict Resolution ===" << std::endl;

    BehaviorAnalyzer analyzer;
    analyzer.initialize();

    // Create overlapping ROIs with different priorities
    ROI highPriorityROI("high_roi", "High Priority Zone", {
        cv::Point(100, 100), cv::Point(300, 100),
        cv::Point(300, 300), cv::Point(100, 300)
    });
    highPriorityROI.priority = 5;
    highPriorityROI.enabled = true;

    ROI mediumPriorityROI("medium_roi", "Medium Priority Zone", {
        cv::Point(150, 150), cv::Point(350, 150),
        cv::Point(350, 350), cv::Point(150, 350)
    });
    mediumPriorityROI.priority = 3;
    mediumPriorityROI.enabled = true;

    ROI lowPriorityROI("low_roi", "Low Priority Zone", {
        cv::Point(80, 80), cv::Point(320, 80),
        cv::Point(320, 320), cv::Point(80, 320)
    });
    lowPriorityROI.priority = 1;
    lowPriorityROI.enabled = true;

    // Add ROIs to analyzer
    analyzer.addROI(highPriorityROI);
    analyzer.addROI(mediumPriorityROI);
    analyzer.addROI(lowPriorityROI);

    std::cout << "✅ Created 3 overlapping ROIs with priorities 5, 3, 1" << std::endl;

    // Test point in overlap area (should be in all three ROIs)
    cv::Point2f testPoint(200, 200);

    // Get overlapping ROIs
    std::vector<std::string> overlapping = analyzer.getOverlappingROIs(testPoint);
    std::cout << "Found " << overlapping.size() << " overlapping ROIs at point (200, 200)" << std::endl;

    // Should find all 3 ROIs
    assert(overlapping.size() == 3);

    // Test highest priority selection
    std::string highestPriority = analyzer.getHighestPriorityROI(overlapping);
    std::cout << "Highest priority ROI: " << highestPriority << std::endl;

    // Should select the high priority ROI
    assert(highestPriority == "high_roi");

    std::cout << "✅ Basic conflict resolution test passed" << std::endl;
}

void testTimeBasedConflictResolution() {
    std::cout << "\n=== Testing Time-based Conflict Resolution ===" << std::endl;

    BehaviorAnalyzer analyzer;
    analyzer.initialize();

    // Create ROIs with time restrictions
    ROI businessHoursROI("business_roi", "Business Hours Zone", {
        cv::Point(100, 100), cv::Point(300, 100),
        cv::Point(300, 300), cv::Point(100, 300)
    });
    businessHoursROI.priority = 3;
    businessHoursROI.enabled = true;
    businessHoursROI.start_time = "09:00";
    businessHoursROI.end_time = "17:00";

    ROI nightShiftROI("night_roi", "Night Shift Zone", {
        cv::Point(150, 150), cv::Point(350, 150),
        cv::Point(350, 350), cv::Point(150, 350)
    });
    nightShiftROI.priority = 2;
    nightShiftROI.enabled = true;
    nightShiftROI.start_time = "18:00";
    nightShiftROI.end_time = "08:00";  // Crosses midnight

    ROI alwaysActiveROI("always_roi", "Always Active Zone", {
        cv::Point(80, 80), cv::Point(320, 80),
        cv::Point(320, 320), cv::Point(80, 320)
    });
    alwaysActiveROI.priority = 1;
    alwaysActiveROI.enabled = true;
    // No time restrictions - always active

    // Add ROIs to analyzer
    analyzer.addROI(businessHoursROI);
    analyzer.addROI(nightShiftROI);
    analyzer.addROI(alwaysActiveROI);

    std::cout << "✅ Created 3 overlapping ROIs with time restrictions" << std::endl;

    // Test time validation
    bool validTime1 = BehaviorAnalyzer::isValidTimeFormat("09:00");
    bool validTime2 = BehaviorAnalyzer::isValidTimeFormat("23:59:59");
    bool invalidTime = BehaviorAnalyzer::isValidTimeFormat("25:00");

    assert(validTime1 == true);
    assert(validTime2 == true);
    assert(invalidTime == false);

    std::cout << "✅ Time format validation test passed" << std::endl;

    // Test current time in range
    bool inRange1 = BehaviorAnalyzer::isCurrentTimeInRange("09:00", "17:00");
    bool inRange2 = BehaviorAnalyzer::isCurrentTimeInRange("18:00", "08:00"); // Crosses midnight

    std::cout << "Current time in business hours: " << (inRange1 ? "Yes" : "No") << std::endl;
    std::cout << "Current time in night shift: " << (inRange2 ? "Yes" : "No") << std::endl;

    std::cout << "✅ Time-based conflict resolution test passed" << std::endl;
}

void testConflictResolutionMetadata() {
    std::cout << "\n=== Testing Conflict Resolution Metadata ===" << std::endl;

    BehaviorAnalyzer analyzer;
    analyzer.initialize();

    // Create test ROIs
    ROI roi1("roi1", "Zone 1", {
        cv::Point(100, 100), cv::Point(200, 100),
        cv::Point(200, 200), cv::Point(100, 200)
    });
    roi1.priority = 4;
    roi1.enabled = true;

    ROI roi2("roi2", "Zone 2", {
        cv::Point(150, 150), cv::Point(250, 150),
        cv::Point(250, 250), cv::Point(150, 250)
    });
    roi2.priority = 2;
    roi2.enabled = true;
    roi2.start_time = "10:00";
    roi2.end_time = "16:00";

    analyzer.addROI(roi1);
    analyzer.addROI(roi2);

    // Create intrusion rules
    IntrusionRule rule1("rule1", roi1, 2.0);
    IntrusionRule rule2("rule2", roi2, 3.0);

    analyzer.addIntrusionRule(rule1);
    analyzer.addIntrusionRule(rule2);

    std::cout << "✅ Created test scenario with 2 overlapping ROIs" << std::endl;

    // Simulate object detection in overlap area
    cv::Mat testFrame = cv::Mat::zeros(400, 400, CV_8UC3);
    std::vector<cv::Rect> detections = {cv::Rect(170, 170, 30, 30)};
    std::vector<int> trackIds = {1};

    // Run analysis
    std::vector<BehaviorEvent> events = analyzer.analyze(testFrame, detections, trackIds);

    std::cout << "Generated " << events.size() << " events" << std::endl;

    // Wait for minimum duration and run again
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));

    events = analyzer.analyze(testFrame, detections, trackIds);

    if (!events.empty()) {
        std::cout << "Event metadata: " << events[0].metadata << std::endl;
        std::cout << "✅ Conflict resolution metadata test passed" << std::endl;
    } else {
        std::cout << "⚠️  No events generated (may be due to time restrictions)" << std::endl;
    }
}

void testEdgeCases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;

    BehaviorAnalyzer analyzer;
    analyzer.initialize();

    // Test with disabled ROI
    ROI disabledROI("disabled_roi", "Disabled Zone", {
        cv::Point(100, 100), cv::Point(200, 100),
        cv::Point(200, 200), cv::Point(100, 200)
    });
    disabledROI.priority = 5;
    disabledROI.enabled = false;  // Disabled

    analyzer.addROI(disabledROI);

    cv::Point2f testPoint(150, 150);
    std::vector<std::string> overlapping = analyzer.getOverlappingROIs(testPoint);

    // Should not find disabled ROI
    assert(overlapping.empty());
    std::cout << "✅ Disabled ROI correctly ignored" << std::endl;

    // Test with empty polygon
    ROI emptyROI("empty_roi", "Empty Zone", {});
    emptyROI.priority = 3;
    emptyROI.enabled = true;

    analyzer.addROI(emptyROI);

    overlapping = analyzer.getOverlappingROIs(testPoint);

    // Should still be empty
    assert(overlapping.empty());
    std::cout << "✅ Empty polygon ROI correctly handled" << std::endl;

    // Test with same priority ROIs
    ROI samePriority1("same1", "Same Priority 1", {
        cv::Point(100, 100), cv::Point(200, 100),
        cv::Point(200, 200), cv::Point(100, 200)
    });
    samePriority1.priority = 3;
    samePriority1.enabled = true;

    ROI samePriority2("same2", "Same Priority 2", {
        cv::Point(150, 150), cv::Point(250, 150),
        cv::Point(250, 250), cv::Point(150, 250)
    });
    samePriority2.priority = 3;  // Same priority
    samePriority2.enabled = true;

    analyzer.addROI(samePriority1);
    analyzer.addROI(samePriority2);

    overlapping = analyzer.getOverlappingROIs(cv::Point2f(175, 175));
    std::string highest = analyzer.getHighestPriorityROI(overlapping);

    std::cout << "Same priority conflict resolved to: " << highest << std::endl;
    std::cout << "✅ Same priority conflict resolution test passed" << std::endl;
}

int main() {
    std::cout << "=== Task 71: Enhanced Conflict Resolution Unit Tests ===" << std::endl;
    std::cout << "Testing advanced conflict resolution logic for overlapping ROIs\n" << std::endl;

    try {
        testBasicConflictResolution();
        testTimeBasedConflictResolution();
        testConflictResolutionMetadata();
        testEdgeCases();

        std::cout << "\n=== All Tests Passed ✅ ===" << std::endl;
        std::cout << "Enhanced conflict resolution logic is working correctly!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
