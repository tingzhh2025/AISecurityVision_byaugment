/**
 * @file YOLOv8DetectorFactory.h
 * @brief Factory for creating YOLOv8 detector instances based on available backends
 */

#ifndef YOLOV8_DETECTOR_FACTORY_H
#define YOLOV8_DETECTOR_FACTORY_H

#include <memory>
#include <string>
#include "YOLOv8Detector.h"

namespace AISecurityVision {

/**
 * @brief Factory class for creating YOLOv8 detector instances
 */
class YOLOv8DetectorFactory {
public:
    /**
     * @brief Create a detector instance based on available backends
     * @param preferredBackend Preferred backend (optional)
     * @return Unique pointer to detector instance
     */
    static std::unique_ptr<YOLOv8Detector> createDetector(
        InferenceBackend preferredBackend = InferenceBackend::CPU);
    
    /**
     * @brief Get list of available backends on this system
     * @return Vector of available backend types
     */
    static std::vector<InferenceBackend> getAvailableBackends();
    
    /**
     * @brief Check if a specific backend is available
     * @param backend Backend to check
     * @return true if available, false otherwise
     */
    static bool isBackendAvailable(InferenceBackend backend);
    
    /**
     * @brief Get backend name string
     * @param backend Backend type
     * @return Backend name as string
     */
    static std::string getBackendName(InferenceBackend backend);
    
    /**
     * @brief Get system information for debugging
     * @return System info as string
     */
    static std::string getSystemInfo();

private:
    // Platform detection helpers
    static bool isRockchipPlatform();
    static bool hasCUDASupport();
    static bool hasTensorRTSupport();
    static bool hasRKNNSupport();
};

} // namespace AISecurityVision

#endif // YOLOV8_DETECTOR_FACTORY_H
