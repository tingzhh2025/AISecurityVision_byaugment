#include "LicensePlateRecognizer.h"
#include <iostream>

#include "../core/Logger.h"
using namespace AISecurityVision;
LicensePlateRecognizer::LicensePlateRecognizer() {}
LicensePlateRecognizer::~LicensePlateRecognizer() {}

bool LicensePlateRecognizer::initialize() {
    LOG_INFO() << "[LicensePlateRecognizer] Initialized (stub)";
    return true;
}

std::vector<std::string> LicensePlateRecognizer::recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections) {
    return std::vector<std::string>();
}
