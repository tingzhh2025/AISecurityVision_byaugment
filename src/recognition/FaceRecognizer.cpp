#include "FaceRecognizer.h"
#include <iostream>

FaceRecognizer::FaceRecognizer() {}
FaceRecognizer::~FaceRecognizer() {}

bool FaceRecognizer::initialize() {
    std::cout << "[FaceRecognizer] Initialized (stub)" << std::endl;
    return true;
}

std::vector<std::string> FaceRecognizer::recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections) {
    return std::vector<std::string>();
}
