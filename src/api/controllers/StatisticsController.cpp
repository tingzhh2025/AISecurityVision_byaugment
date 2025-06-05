#include "StatisticsController.h"
#include "../../core/TaskManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>

using namespace AISecurityVision;

void StatisticsController::handleGetStatistics(const std::string& request, std::string& response) {
    try {
        // Collect comprehensive statistics
        SystemStats systemStats = collectSystemStats();
        DetectionStats detectionStats = collectDetectionStats();
        auto cameraStats = getCameraStats();

        // Build comprehensive response
        std::ostringstream json;
        json << "{"
             << "\"system\":" << serializeSystemStats(systemStats) << ","
             << "\"detection\":" << serializeDetectionStats(detectionStats) << ","
             << "\"cameras\":" << serializeCameraStats(cameraStats) << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved comprehensive statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get statistics: " + std::string(e.what()), 500);
    }
}

void StatisticsController::handleGetSystemStats(const std::string& request, std::string& response) {
    try {
        SystemStats stats = collectSystemStats();
        response = createJsonResponse(serializeSystemStats(stats));
        logInfo("Retrieved system statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system stats: " + std::string(e.what()), 500);
    }
}

void StatisticsController::handleGetDetectionStats(const std::string& request, std::string& response) {
    try {
        DetectionStats stats = collectDetectionStats();
        response = createJsonResponse(serializeDetectionStats(stats));
        logInfo("Retrieved detection statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get detection stats: " + std::string(e.what()), 500);
    }
}

void StatisticsController::handleGetCameraStats(const std::string& request, std::string& response) {
    try {
        auto stats = getCameraStats();
        response = createJsonResponse(serializeCameraStats(stats));
        logInfo("Retrieved camera statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get camera stats: " + std::string(e.what()), 500);
    }
}

StatisticsController::SystemStats StatisticsController::collectSystemStats() {
    SystemStats stats;
    
    // For demonstration, return sample statistics
    // In a real implementation, this would collect actual system metrics
    stats.cpu_usage = 25.5;
    stats.memory_usage = 45.2;
    stats.disk_usage = 67.8;
    stats.gpu_usage = 15.3;
    
    stats.active_cameras = 2;
    stats.total_detections_today = 1247;
    stats.total_recordings_today = 45;
    stats.avg_detection_time = 35.7;
    
    stats.network_throughput = 125.6; // MB/s
    stats.active_connections = 8;
    
    auto now = std::chrono::system_clock::now();
    auto start_time = std::chrono::system_clock::from_time_t(0); // Placeholder
    stats.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    stats.last_restart = "2025-06-04T18:18:37Z";
    
    return stats;
}

StatisticsController::DetectionStats StatisticsController::collectDetectionStats() {
    DetectionStats stats;
    
    // Sample detection statistics
    stats.detections_by_class["person"] = 856;
    stats.detections_by_class["car"] = 234;
    stats.detections_by_class["truck"] = 89;
    stats.detections_by_class["bicycle"] = 68;
    
    stats.detections_by_camera["camera_01"] = 623;
    stats.detections_by_camera["camera_02"] = 624;
    
    stats.detections_by_hour["00"] = 45;
    stats.detections_by_hour["01"] = 23;
    stats.detections_by_hour["02"] = 12;
    // ... continue for all hours
    
    stats.total_detections = 1247;
    stats.avg_confidence = 0.847;
    stats.avg_processing_time = 35.7;
    
    return stats;
}

std::map<std::string, int> StatisticsController::getCameraStats() {
    std::map<std::string, int> stats;
    
    // Sample camera statistics
    stats["total_cameras"] = 2;
    stats["active_cameras"] = 2;
    stats["offline_cameras"] = 0;
    stats["error_cameras"] = 0;
    stats["total_streams"] = 2;
    stats["recording_cameras"] = 2;
    
    return stats;
}

std::string StatisticsController::serializeSystemStats(const SystemStats& stats) {
    std::ostringstream json;
    json << "{"
         << "\"performance\":{"
         << "\"cpu_usage\":" << stats.cpu_usage << ","
         << "\"memory_usage\":" << stats.memory_usage << ","
         << "\"disk_usage\":" << stats.disk_usage << ","
         << "\"gpu_usage\":" << stats.gpu_usage
         << "},"
         << "\"system\":{"
         << "\"active_cameras\":" << stats.active_cameras << ","
         << "\"total_detections_today\":" << stats.total_detections_today << ","
         << "\"total_recordings_today\":" << stats.total_recordings_today << ","
         << "\"avg_detection_time\":" << stats.avg_detection_time
         << "},"
         << "\"network\":{"
         << "\"throughput_mbps\":" << stats.network_throughput << ","
         << "\"active_connections\":" << stats.active_connections
         << "},"
         << "\"uptime\":{"
         << "\"uptime_seconds\":" << stats.uptime_seconds << ","
         << "\"last_restart\":\"" << stats.last_restart << "\""
         << "}"
         << "}";
    
    return json.str();
}

std::string StatisticsController::serializeDetectionStats(const DetectionStats& stats) {
    std::ostringstream json;
    json << "{"
         << "\"total_detections\":" << stats.total_detections << ","
         << "\"avg_confidence\":" << stats.avg_confidence << ","
         << "\"avg_processing_time\":" << stats.avg_processing_time << ","
         << "\"by_class\":" << serializeMapAsJson(stats.detections_by_class) << ","
         << "\"by_camera\":" << serializeMapAsJson(stats.detections_by_camera) << ","
         << "\"by_hour\":" << serializeMapAsJson(stats.detections_by_hour)
         << "}";
    
    return json.str();
}

std::string StatisticsController::serializeCameraStats(const std::map<std::string, int>& stats) {
    return serializeMapAsJson(stats);
}

std::string StatisticsController::serializeMapAsJson(const std::map<std::string, int>& data) {
    std::ostringstream json;
    json << "{";
    
    bool first = true;
    for (const auto& [key, value] : data) {
        if (!first) json << ",";
        json << "\"" << key << "\":" << value;
        first = false;
    }
    
    json << "}";
    return json.str();
}
