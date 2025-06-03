/**
 * @file PersonFilter.cpp
 * @brief Person Detection Filter Implementation
 *
 * This file implements person-specific filtering from YOLOv8 detection results.
 * Designed as a non-intrusive extension to the existing AI vision system.
 */

#include "PersonFilter.h"
#include "../core/Logger.h"
#include <algorithm>
#include <numeric>

namespace AISecurityVision {

std::vector<PersonDetection> PersonFilter::filterPersons(
    const std::vector<Detection>& detections,
    const cv::Mat& frame,
    const std::vector<int>& trackIds,
    int64_t timestamp
) {
    std::vector<PersonDetection> persons;
    
    if (frame.empty()) {
        LOG_WARN() << "[PersonFilter] Empty frame provided";
        return persons;
    }
    
    for (size_t i = 0; i < detections.size(); ++i) {
        const auto& detection = detections[i];
        
        // Filter only person class (COCO class 0)
        if (!isPerson(detection)) {
            continue;
        }
        
        // Create person detection
        PersonDetection person(detection.bbox, detection.confidence);
        person.timestamp = timestamp;
        
        // Assign track ID if available
        if (i < trackIds.size()) {
            person.trackId = trackIds[i];
        }
        
        // Extract person crop
        person.crop = extractPersonCrop(frame, detection.bbox);
        
        // Validate crop
        if (isValidCrop(person.crop)) {
            persons.push_back(person);
        } else {
            LOG_DEBUG() << "[PersonFilter] Invalid crop for person detection at ("
                       << detection.bbox.x << ", " << detection.bbox.y << ")";
        }
    }
    
    LOG_DEBUG() << "[PersonFilter] Filtered " << persons.size() 
               << " person detections from " << detections.size() << " total detections";
    
    return persons;
}

cv::Mat PersonFilter::extractPersonCrop(
    const cv::Mat& frame,
    const cv::Rect& bbox,
    float padding
) {
    if (frame.empty() || bbox.area() <= 0) {
        return cv::Mat();
    }

    // Clamp padding to reasonable range
    padding = std::max(0.0f, std::min(padding, MAX_PADDING));

    // Expand bounding box with padding
    cv::Rect expandedBbox = expandBbox(bbox, frame.size(), padding);

    // Extract crop
    cv::Mat crop = frame(expandedBbox).clone();

    // Ensure crop is large enough for age/gender analysis
    if (!crop.empty() && (crop.cols < 64 || crop.rows < 64)) {
        cv::resize(crop, crop, cv::Size(std::max(64, crop.cols), std::max(64, crop.rows)));
        LOG_DEBUG() << "[PersonFilter] Resized small crop to " << crop.cols << "x" << crop.rows;
    }

    // Fix RGA alignment: ensure width is 16-aligned for RGB888
    if (!crop.empty()) {
        int alignedWidth = ((crop.cols + 15) / 16) * 16;
        if (alignedWidth != crop.cols) {
            cv::Mat aligned;
            cv::resize(crop, aligned, cv::Size(alignedWidth, crop.rows));
            crop = aligned;
            LOG_DEBUG() << "[PersonFilter] Aligned crop width from " << expandedBbox.width
                       << " to " << alignedWidth << " for RGA compatibility";
        }
    }

    return crop;
}

bool PersonFilter::isPerson(const Detection& detection) {
    return detection.classId == PERSON_CLASS_ID && 
           detection.confidence >= DEFAULT_CONFIDENCE_THRESHOLD;
}

std::vector<PersonDetection> PersonFilter::filterByConfidence(
    const std::vector<PersonDetection>& persons,
    float threshold
) {
    std::vector<PersonDetection> filtered;
    
    std::copy_if(persons.begin(), persons.end(), std::back_inserter(filtered),
                 [threshold](const PersonDetection& person) {
                     return person.confidence >= threshold;
                 });
    
    return filtered;
}

std::vector<PersonDetection> PersonFilter::filterBySize(
    const std::vector<PersonDetection>& persons,
    int minWidth,
    int minHeight
) {
    std::vector<PersonDetection> filtered;
    
    std::copy_if(persons.begin(), persons.end(), std::back_inserter(filtered),
                 [minWidth, minHeight](const PersonDetection& person) {
                     return person.bbox.width >= minWidth && 
                            person.bbox.height >= minHeight;
                 });
    
    return filtered;
}

PersonFilter::PersonStats PersonFilter::getBasicStats(
    const std::vector<PersonDetection>& persons
) {
    PersonStats stats;
    
    if (persons.empty()) {
        return stats;
    }
    
    stats.total_count = static_cast<int>(persons.size());
    
    // Calculate average confidence
    float totalConfidence = std::accumulate(persons.begin(), persons.end(), 0.0f,
                                          [](float sum, const PersonDetection& person) {
                                              return sum + person.confidence;
                                          });
    stats.avg_confidence = totalConfidence / persons.size();
    
    // Calculate average size
    int totalWidth = 0, totalHeight = 0;
    int trackedCount = 0;
    
    for (const auto& person : persons) {
        totalWidth += person.bbox.width;
        totalHeight += person.bbox.height;
        
        if (person.trackId >= 0) {
            trackedCount++;
        }
    }
    
    stats.avg_size.width = totalWidth / persons.size();
    stats.avg_size.height = totalHeight / persons.size();
    stats.tracked_count = trackedCount;
    
    return stats;
}

cv::Rect PersonFilter::expandBbox(const cv::Rect& bbox, const cv::Size& frameSize, float padding) {
    int padX = static_cast<int>(bbox.width * padding);
    int padY = static_cast<int>(bbox.height * padding);
    
    cv::Rect expanded;
    expanded.x = std::max(0, bbox.x - padX);
    expanded.y = std::max(0, bbox.y - padY);
    expanded.width = std::min(frameSize.width - expanded.x, bbox.width + 2 * padX);
    expanded.height = std::min(frameSize.height - expanded.y, bbox.height + 2 * padY);
    
    return expanded;
}

bool PersonFilter::isValidCrop(const cv::Mat& crop) {
    return !crop.empty() && 
           crop.cols >= MIN_PERSON_WIDTH && 
           crop.rows >= MIN_PERSON_HEIGHT;
}

} // namespace AISecurityVision
