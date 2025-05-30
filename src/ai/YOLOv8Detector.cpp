/**
 * @file YOLOv8Detector.cpp
 * @brief YOLOv8 Object Detection Base Implementation
 *
 * This file implements the base functionality for YOLOv8 object detection.
 * Based on the reference implementation from /userdata/source/source/yolov8_rknn
 */

#include "YOLOv8Detector.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <numeric>

namespace AISecurityVision {

YOLOv8Detector::YOLOv8Detector() {
    initializeDefaultClassNames();
}

YOLOv8Detector::~YOLOv8Detector() {
    // Note: Don't call cleanup() here as it's pure virtual
    // Derived classes should handle cleanup in their own destructors
}

double YOLOv8Detector::getAverageInferenceTime() const {
    if (m_inferenceTimes.empty()) {
        return 0.0;
    }

    double sum = std::accumulate(m_inferenceTimes.begin(), m_inferenceTimes.end(), 0.0);
    return sum / m_inferenceTimes.size();
}

bool YOLOv8Detector::loadClassNames(const std::string& labelPath) {
    std::ifstream file(labelPath);
    if (!file.is_open()) {
        LOG_ERROR() << "[YOLOv8Detector] Failed to open label file: " << labelPath;
        return false;
    }

    m_classNames.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing whitespace
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            m_classNames.push_back(line);
        }
    }

    LOG_INFO() << "[YOLOv8Detector] Loaded " << m_classNames.size() << " class names from " << labelPath;
    return true;
}

void YOLOv8Detector::initializeDefaultClassNames() {
    // COCO dataset class names (80 classes)
    m_classNames = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
        "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
        "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
        "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
        "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
        "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
        "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
        "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
        "toothbrush"
    };

    LOG_INFO() << "[YOLOv8Detector] Initialized with " << m_classNames.size() << " COCO class names";
}

} // namespace AISecurityVision