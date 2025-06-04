#include "PersonStatsController.h"
#include "../../core/TaskManager.h"
#include "../../core/VideoPipeline.h"
#include <nlohmann/json.hpp>
#include <sstream>

using namespace AISecurityVision;

void PersonStatsController::handleGetPersonStats(const std::string& request, std::string& response, const std::string& cameraId) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto pipeline = m_taskManager->getPipeline(cameraId);
        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Get person statistics from pipeline
        auto pipelineStats = pipeline->getCurrentPersonStats();

        // Convert to local PersonStats structure
        PersonStats stats;
        stats.total_persons = pipelineStats.total_persons;
        stats.male_count = pipelineStats.male_count;
        stats.female_count = pipelineStats.female_count;
        stats.child_count = pipelineStats.child_count;
        stats.young_count = pipelineStats.young_count;
        stats.middle_count = pipelineStats.middle_count;
        stats.senior_count = pipelineStats.senior_count;
        stats.person_boxes = pipelineStats.person_boxes;
        stats.person_genders = pipelineStats.person_genders;
        stats.person_ages = pipelineStats.person_ages;

        response = createJsonResponse(serializePersonStats(stats));
        logInfo("Retrieved person statistics for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get person stats: " + std::string(e.what()), 500);
    }
}

void PersonStatsController::handlePostPersonStatsEnable(const std::string& request, std::string& response, const std::string& cameraId) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto pipeline = m_taskManager->getPipeline(cameraId);
        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Enable person statistics in pipeline
        pipeline->setPersonStatsEnabled(true);

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Person statistics enabled\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"enabled_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Enabled person statistics for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to enable person stats: " + std::string(e.what()), 500);
    }
}

void PersonStatsController::handlePostPersonStatsDisable(const std::string& request, std::string& response, const std::string& cameraId) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto pipeline = m_taskManager->getPipeline(cameraId);
        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Disable person statistics in pipeline
        pipeline->setPersonStatsEnabled(false);

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Person statistics disabled\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"disabled_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Disabled person statistics for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to disable person stats: " + std::string(e.what()), 500);
    }
}

void PersonStatsController::handleGetPersonStatsConfig(const std::string& request, std::string& response, const std::string& cameraId) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto pipeline = m_taskManager->getPipeline(cameraId);
        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Create person statistics configuration from pipeline settings
        PersonStatsConfig config;
        config.enabled = pipeline->isPersonStatsEnabled();
        // Note: Individual config parameters are not exposed by VideoPipeline
        // Using default values for now
        config.gender_threshold = 0.7f;
        config.age_threshold = 0.6f;
        config.batch_size = 4;
        config.enable_caching = true;

        response = createJsonResponse(serializePersonStatsConfig(config));
        logInfo("Retrieved person statistics config for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get person stats config: " + std::string(e.what()), 500);
    }
}

void PersonStatsController::handlePostPersonStatsConfig(const std::string& request, std::string& response, const std::string& cameraId) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto pipeline = m_taskManager->getPipeline(cameraId);
        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Parse configuration from request
        PersonStatsConfig config;
        if (!deserializePersonStatsConfig(request, config)) {
            response = createErrorResponse("Invalid person stats configuration", 400);
            return;
        }

        // Apply configuration to pipeline
        pipeline->setPersonStatsEnabled(config.enabled);
        pipeline->setPersonStatsConfig(config.gender_threshold, config.age_threshold,
                                      config.batch_size, config.enable_caching);

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Person statistics configuration updated\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Updated person statistics config for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update person stats config: " + std::string(e.what()), 500);
    }
}

std::string PersonStatsController::serializePersonStats(const PersonStats& stats) {
    std::ostringstream json;
    json << "{"
         << "\"total_persons\":" << stats.total_persons << ","
         << "\"gender_stats\":{"
         << "\"male_count\":" << stats.male_count << ","
         << "\"female_count\":" << stats.female_count
         << "},"
         << "\"age_stats\":{"
         << "\"child_count\":" << stats.child_count << ","
         << "\"young_count\":" << stats.young_count << ","
         << "\"middle_count\":" << stats.middle_count << ","
         << "\"senior_count\":" << stats.senior_count
         << "},"
         << "\"race_stats\":{"
         << "\"black_count\":" << stats.black_count << ","
         << "\"asian_count\":" << stats.asian_count << ","
         << "\"latino_count\":" << stats.latino_count << ","
         << "\"middle_eastern_count\":" << stats.middle_eastern_count << ","
         << "\"white_count\":" << stats.white_count
         << "},"
         << "\"mask_stats\":{"
         << "\"mask_count\":" << stats.mask_count << ","
         << "\"no_mask_count\":" << stats.no_mask_count
         << "},"
         << "\"quality_stats\":{"
         << "\"average_quality\":" << stats.average_quality
         << "},"
         << "\"detection_boxes\":" << stats.person_boxes.size() << ","
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
         << "}";
    return json.str();
}

std::string PersonStatsController::serializePersonStatsConfig(const PersonStatsConfig& config) {
    std::ostringstream json;
    json << "{"
         << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
         << "\"gender_threshold\":" << config.gender_threshold << ","
         << "\"age_threshold\":" << config.age_threshold << ","
         << "\"batch_size\":" << config.batch_size << ","
         << "\"enable_caching\":" << (config.enable_caching ? "true" : "false") << ","
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
         << "}";
    return json.str();
}

bool PersonStatsController::deserializePersonStatsConfig(const std::string& json, PersonStatsConfig& config) {
    try {
        auto j = nlohmann::json::parse(json);

        config.enabled = j.value("enabled", false);
        config.gender_threshold = j.value("gender_threshold", 0.7f);
        config.age_threshold = j.value("age_threshold", 0.6f);
        config.batch_size = j.value("batch_size", 4);
        config.enable_caching = j.value("enable_caching", true);

        // Validate thresholds
        if (config.gender_threshold < 0.0f || config.gender_threshold > 1.0f) {
            config.gender_threshold = 0.7f;
        }
        if (config.age_threshold < 0.0f || config.age_threshold > 1.0f) {
            config.age_threshold = 0.6f;
        }

        // Validate batch size
        if (config.batch_size < 1 || config.batch_size > 16) {
            config.batch_size = 4;
        }

        return true;
    } catch (const std::exception& e) {
        logError("Failed to deserialize person stats config: " + std::string(e.what()));
        return false;
    }
}
