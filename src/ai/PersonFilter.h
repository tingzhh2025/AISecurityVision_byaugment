/**
 * @file PersonFilter.h
 * @brief Person Detection Filter for Enhanced Statistics
 *
 * This file implements person-specific filtering from YOLOv8 detection results.
 * Designed as a non-intrusive extension to the existing AI vision system.
 */

#ifndef PERSON_FILTER_H
#define PERSON_FILTER_H

#include <vector>
#include <opencv2/opencv.hpp>
#include "YOLOv8Detector.h"

namespace AISecurityVision {

/**
 * @brief Person detection result structure
 */
struct PersonDetection {
    cv::Rect bbox;              // Bounding box
    float confidence;           // Detection confidence
    int trackId = -1;          // Associated track ID (if available)
    cv::Mat crop;              // Person crop image for further analysis
    int64_t timestamp;         // Detection timestamp
    
    PersonDetection() : confidence(0.0f), timestamp(0) {}
    
    PersonDetection(const cv::Rect& box, float conf, int track = -1) 
        : bbox(box), confidence(conf), trackId(track), timestamp(0) {}
};

/**
 * @brief Person filter utility class
 *
 * This class provides static methods to filter person detections from
 * YOLOv8 results without modifying any existing code. It's designed
 * as a pure utility class for backward compatibility.
 */
class PersonFilter {
public:
    /**
     * @brief Filter person detections from YOLOv8 results
     * @param detections YOLOv8 detection results
     * @param frame Original frame for crop extraction
     * @param trackIds Optional track IDs (if tracking is enabled)
     * @param timestamp Frame timestamp
     * @return Vector of person detections with crops
     */
    static std::vector<PersonDetection> filterPersons(
        const std::vector<Detection>& detections,
        const cv::Mat& frame,
        const std::vector<int>& trackIds = {},
        int64_t timestamp = 0
    );
    
    /**
     * @brief Extract person crop with padding
     * @param frame Original frame
     * @param bbox Person bounding box
     * @param padding Padding ratio (default: 0.1 = 10%)
     * @return Cropped and padded person image
     */
    static cv::Mat extractPersonCrop(
        const cv::Mat& frame,
        const cv::Rect& bbox,
        float padding = 0.1f
    );
    
    /**
     * @brief Check if detection is a person (COCO class 0)
     * @param detection YOLOv8 detection result
     * @return true if detection is person class
     */
    static bool isPerson(const Detection& detection);
    
    /**
     * @brief Filter person detections by confidence threshold
     * @param persons Person detection results
     * @param threshold Minimum confidence threshold
     * @return Filtered person detections
     */
    static std::vector<PersonDetection> filterByConfidence(
        const std::vector<PersonDetection>& persons,
        float threshold = 0.5f
    );
    
    /**
     * @brief Filter person detections by minimum size
     * @param persons Person detection results
     * @param minWidth Minimum width in pixels
     * @param minHeight Minimum height in pixels
     * @return Filtered person detections
     */
    static std::vector<PersonDetection> filterBySize(
        const std::vector<PersonDetection>& persons,
        int minWidth = 50,
        int minHeight = 100
    );
    
    /**
     * @brief Get person detection statistics
     * @param persons Person detection results
     * @return Basic statistics structure
     */
    struct PersonStats {
        int total_count = 0;
        float avg_confidence = 0.0f;
        cv::Size avg_size{0, 0};
        int tracked_count = 0;  // Number with valid track IDs
    };
    
    static PersonStats getBasicStats(const std::vector<PersonDetection>& persons);

private:
    // Utility methods
    static cv::Rect expandBbox(const cv::Rect& bbox, const cv::Size& frameSize, float padding);
    static bool isValidCrop(const cv::Mat& crop);
    
    // Constants
    static constexpr int PERSON_CLASS_ID = 0;  // COCO person class
    static constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.5f;
    static constexpr int MIN_PERSON_WIDTH = 30;
    static constexpr int MIN_PERSON_HEIGHT = 60;
    static constexpr float MAX_PADDING = 0.3f;  // Maximum 30% padding
};

} // namespace AISecurityVision

#endif // PERSON_FILTER_H
