#include "ByteTracker.h"
#include <iostream>

ByteTracker::ByteTracker() : m_nextId(1) {}

ByteTracker::~ByteTracker() {}

bool ByteTracker::initialize() {
    std::cout << "[ByteTracker] Initialized (stub)" << std::endl;
    return true;
}

std::vector<int> ByteTracker::update(const std::vector<cv::Rect>& detections) {
    std::vector<int> trackIds;
    for (size_t i = 0; i < detections.size(); ++i) {
        trackIds.push_back(m_nextId++);
    }
    return trackIds;
}
