#pragma once

#include "BaseController.h"
#include <string>
#include <map>

namespace AISecurityVision {

/**
 * @brief Controller for system statistics API endpoints (NEW - Phase 3)
 * 
 * Handles all statistics-related functionality including:
 * - System performance statistics
 * - Detection statistics
 * - Camera statistics
 * - Historical data analysis
 */
class StatisticsController : public BaseController {
public:
    StatisticsController() = default;
    ~StatisticsController() override = default;

    /**
     * @brief System statistics structure
     */
    struct SystemStats {
        // Performance metrics
        double cpu_usage;
        double memory_usage;
        double disk_usage;
        double gpu_usage;
        
        // System metrics
        int active_cameras;
        int total_detections_today;
        int total_recordings_today;
        double avg_detection_time;
        
        // Network metrics
        double network_throughput;
        int active_connections;
        
        // Uptime
        int uptime_seconds;
        std::string last_restart;
    };

    /**
     * @brief Detection statistics structure
     */
    struct DetectionStats {
        std::map<std::string, int> detections_by_class;
        std::map<std::string, int> detections_by_camera;
        std::map<std::string, int> detections_by_hour;
        int total_detections;
        double avg_confidence;
        double avg_processing_time;
    };

    // Statistics endpoints
    void handleGetStatistics(const std::string& request, std::string& response);
    void handleGetSystemStats(const std::string& request, std::string& response);
    void handleGetDetectionStats(const std::string& request, std::string& response);
    void handleGetCameraStats(const std::string& request, std::string& response);

private:
    std::string getControllerName() const override { return "StatisticsController"; }

    // Helper methods
    SystemStats collectSystemStats();
    DetectionStats collectDetectionStats();
    std::map<std::string, int> getCameraStats();
    
    // JSON serialization
    std::string serializeSystemStats(const SystemStats& stats);
    std::string serializeDetectionStats(const DetectionStats& stats);
    std::string serializeCameraStats(const std::map<std::string, int>& stats);
    std::string serializeMapAsJson(const std::map<std::string, int>& data);
};

} // namespace AISecurityVision
