#include "LicensePlateRecognizer.h"
#include <iostream>

LicensePlateRecognizer::LicensePlateRecognizer() {}
LicensePlateRecognizer::~LicensePlateRecognizer() {}

bool LicensePlateRecognizer::initialize() {
    std::cout << "[LicensePlateRecognizer] Initialized (stub)" << std::endl;
    return true;
}

std::vector<std::string> LicensePlateRecognizer::recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections) {
    return std::vector<std::string>();
}
