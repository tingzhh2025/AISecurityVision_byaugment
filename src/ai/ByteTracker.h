#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

/**
 * @brief ByteTracker implementation for object tracking
 */
class ByteTracker {
public:
    ByteTracker();
    ~ByteTracker();
    
    bool initialize();
    std::vector<int> update(const std::vector<cv::Rect>& detections);

private:
    int m_nextId;
};
