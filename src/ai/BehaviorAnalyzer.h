#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief Behavior analysis engine
 */
class BehaviorAnalyzer {
public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();
    
    bool initialize();
    std::vector<std::string> analyze(const cv::Mat& frame, 
                                   const std::vector<cv::Rect>& detections,
                                   const std::vector<int>& trackIds);
};
