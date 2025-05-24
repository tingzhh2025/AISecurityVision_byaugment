#include "BehaviorAnalyzer.h"
#include <iostream>

BehaviorAnalyzer::BehaviorAnalyzer() {}

BehaviorAnalyzer::~BehaviorAnalyzer() {}

bool BehaviorAnalyzer::initialize() {
    std::cout << "[BehaviorAnalyzer] Initialized (stub)" << std::endl;
    return true;
}

std::vector<std::string> BehaviorAnalyzer::analyze(const cv::Mat& frame, 
                                                 const std::vector<cv::Rect>& detections,
                                                 const std::vector<int>& trackIds) {
    std::vector<std::string> events;
    // Stub implementation - no events detected
    return events;
}
