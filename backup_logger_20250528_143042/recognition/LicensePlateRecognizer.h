#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class LicensePlateRecognizer {
public:
    LicensePlateRecognizer();
    ~LicensePlateRecognizer();
    
    bool initialize();
    std::vector<std::string> recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections);
};
