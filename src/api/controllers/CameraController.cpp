#include "CameraController.h"
#include "../../database/DatabaseManager.h"
#include "../../core/TaskManager.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstddef>
#include <exception>

namespace AISecurityVision {

// ========== Detection Configuration Methods ==========

void CameraController::handleGetDetectionCategories(const std::string& request, std::string& response) {
    try {
        // Get enabled categories from database
        ::DatabaseManager dbManager;
        std::vector<std::string> enabledCategories;
        
        if (dbManager.initialize()) {
            std::string categoriesJson = dbManager.getConfig("detection", "enabled_categories", "");
            if (!categoriesJson.empty()) {
                try {
                    auto j = nlohmann::json::parse(categoriesJson);
                    if (j.is_array()) {
                        for (const auto& cat : j) {
                            if (cat.is_string()) {
                                enabledCategories.push_back(cat.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    logWarn("Failed to parse enabled categories: " + std::string(e.what()));
                }
            }
        }

        // Default categories if none configured
        if (enabledCategories.empty()) {
            enabledCategories = {"person", "car", "truck", "bicycle"};
        }

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"enabled_categories\":[";
        
        for (size_t i = 0; i < enabledCategories.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << enabledCategories[i] << "\"";
        }
        
        json << "],"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved detection categories");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get detection categories: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostDetectionCategories(const std::string& request, std::string& response) {
    try {
        // Parse enabled categories from request
        auto j = nlohmann::json::parse(request);
        if (!j.contains("enabled_categories") || !j["enabled_categories"].is_array()) {
            response = createErrorResponse("enabled_categories array is required", 400);
            return;
        }

        std::vector<std::string> enabledCategories;
        for (const auto& cat : j["enabled_categories"]) {
            if (cat.is_string()) {
                enabledCategories.push_back(cat.get<std::string>());
            }
        }

        // Save to database
        ::DatabaseManager dbManager;
        if (dbManager.initialize()) {
            nlohmann::json categoriesJson = enabledCategories;
            if (!dbManager.saveConfig("detection", "enabled_categories", categoriesJson.dump())) {
                logWarn("Failed to save enabled categories to database");
            }
        }

        // Apply to all active pipelines
        if (m_taskManager) {
            auto pipelines = m_taskManager->getActivePipelines();
            for (const auto& pipelineId : pipelines) {
                auto pipeline = m_taskManager->getPipeline(pipelineId);
                if (pipeline) {
                    pipeline->setEnabledCategories(enabledCategories);
                }
            }
        }

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Detection categories updated\","
             << "\"enabled_categories\":" << enabledCategories.size() << ","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Updated detection categories");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update detection categories: " + std::string(e.what()), 400);
    }
}

void CameraController::handleGetAvailableCategories(const std::string& request, std::string& response) {
    try {
        // YOLOv8 COCO 80 classes
        std::ostringstream json;
        json << "{"
             << "\"categories\":{"
             << "\"person_vehicle\":["
             << "\"person\",\"bicycle\",\"car\",\"motorcycle\",\"airplane\","
             << "\"bus\",\"train\",\"truck\",\"boat\""
             << "],"
             << "\"traffic\":["
             << "\"traffic light\",\"fire hydrant\",\"stop sign\",\"parking meter\""
             << "],"
             << "\"animals\":["
             << "\"bird\",\"cat\",\"dog\",\"horse\",\"sheep\",\"cow\","
             << "\"elephant\",\"bear\",\"zebra\",\"giraffe\""
             << "],"
             << "\"sports\":["
             << "\"frisbee\",\"skis\",\"snowboard\",\"sports ball\","
             << "\"kite\",\"baseball bat\",\"baseball glove\",\"skateboard\","
             << "\"surfboard\",\"tennis racket\""
             << "],"
             << "\"household\":["
             << "\"bottle\",\"wine glass\",\"cup\",\"fork\",\"knife\","
             << "\"spoon\",\"bowl\",\"banana\",\"apple\",\"sandwich\","
             << "\"orange\",\"broccoli\",\"carrot\",\"hot dog\",\"pizza\","
             << "\"donut\",\"cake\""
             << "],"
             << "\"furniture\":["
             << "\"chair\",\"couch\",\"potted plant\",\"bed\",\"dining table\","
             << "\"toilet\",\"tv\",\"laptop\",\"mouse\",\"remote\",\"keyboard\","
             << "\"cell phone\""
             << "],"
             << "\"other\":["
             << "\"microwave\",\"oven\",\"toaster\",\"sink\",\"refrigerator\","
             << "\"book\",\"clock\",\"vase\",\"scissors\",\"teddy bear\","
             << "\"hair drier\",\"toothbrush\",\"bench\",\"backpack\","
             << "\"umbrella\",\"handbag\",\"tie\",\"suitcase\""
             << "]"
             << "},"
             << "\"total_classes\":80,"
             << "\"model\":\"YOLOv8\","
             << "\"dataset\":\"COCO\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved available detection categories");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get available categories: " + std::string(e.what()), 500);
    }
}

void CameraController::handleGetDetectionConfig(const std::string& request, std::string& response) {
    try {
        // Get detection configuration from database
        ::DatabaseManager dbManager;
        float confidenceThreshold = 0.5f;
        float nmsThreshold = 0.4f;
        int maxDetections = 100;
        int detectionInterval = 1;

        if (dbManager.initialize()) {
            confidenceThreshold = std::stof(dbManager.getConfig("detection", "confidence_threshold", "0.5"));
            nmsThreshold = std::stof(dbManager.getConfig("detection", "nms_threshold", "0.4"));
            maxDetections = std::stoi(dbManager.getConfig("detection", "max_detections", "100"));
            detectionInterval = std::stoi(dbManager.getConfig("detection", "detection_interval", "1"));
        }

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"confidence_threshold\":" << confidenceThreshold << ","
             << "\"nms_threshold\":" << nmsThreshold << ","
             << "\"max_detections\":" << maxDetections << ","
             << "\"detection_interval\":" << detectionInterval << ","
             << "\"backend\":\"RKNN\","
             << "\"model\":\"YOLOv8n\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved detection configuration");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get detection config: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostDetectionConfig(const std::string& request, std::string& response) {
    try {
        // Parse detection config from request
        auto j = nlohmann::json::parse(request);
        
        float confidenceThreshold = j.value("confidence_threshold", 0.5f);
        float nmsThreshold = j.value("nms_threshold", 0.4f);
        int maxDetections = j.value("max_detections", 100);
        int detectionInterval = j.value("detection_interval", 1);

        // Validate parameters
        if (confidenceThreshold < 0.0f || confidenceThreshold > 1.0f) {
            response = createErrorResponse("confidence_threshold must be between 0.0 and 1.0", 400);
            return;
        }

        if (nmsThreshold < 0.0f || nmsThreshold > 1.0f) {
            response = createErrorResponse("nms_threshold must be between 0.0 and 1.0", 400);
            return;
        }

        if (maxDetections < 1 || maxDetections > 1000) {
            response = createErrorResponse("max_detections must be between 1 and 1000", 400);
            return;
        }

        if (detectionInterval < 1 || detectionInterval > 30) {
            response = createErrorResponse("detection_interval must be between 1 and 30", 400);
            return;
        }

        // Save to database
        ::DatabaseManager dbManager;
        if (dbManager.initialize()) {
            dbManager.saveConfig("detection", "confidence_threshold", std::to_string(confidenceThreshold));
            dbManager.saveConfig("detection", "nms_threshold", std::to_string(nmsThreshold));
            dbManager.saveConfig("detection", "max_detections", std::to_string(maxDetections));
            dbManager.saveConfig("detection", "detection_interval", std::to_string(detectionInterval));
        }

        // Apply to all active pipelines
        if (m_taskManager) {
            auto pipelines = m_taskManager->getActivePipelines();
            for (const auto& pipelineId : pipelines) {
                auto pipeline = m_taskManager->getPipeline(pipelineId);
                if (pipeline) {
                    pipeline->setDetectionThresholds(confidenceThreshold, nmsThreshold);
                }
            }
        }

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Detection configuration updated\","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Updated detection configuration");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update detection config: " + std::string(e.what()), 400);
    }
}

void CameraController::handleGetDetectionStats(const std::string& request, std::string& response) {
    try {
        // Collect detection statistics from all active pipelines
        int totalDetections = 0;
        std::map<std::string, int> detectionsByClass;
        float avgProcessingTime = 0.0f;
        int pipelineCount = 0;

        if (m_taskManager) {
            auto pipelines = m_taskManager->getActivePipelines();
            pipelineCount = pipelines.size();

            for (const auto& pipelineId : pipelines) {
                auto pipeline = m_taskManager->getPipeline(pipelineId);
                if (pipeline) {
                    auto stats = pipeline->getDetectionStats();
                    totalDetections += stats.total_detections;
                    avgProcessingTime += stats.avg_processing_time;
                    
                    for (const auto& [className, count] : stats.detections_by_class) {
                        detectionsByClass[className] += count;
                    }
                }
            }

            if (pipelineCount > 0) {
                avgProcessingTime /= pipelineCount;
            }
        }

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"total_detections\":" << totalDetections << ","
             << "\"active_pipelines\":" << pipelineCount << ","
             << "\"avg_processing_time\":" << avgProcessingTime << ","
             << "\"detections_by_class\":{";

        bool first = true;
        for (const auto& [className, count] : detectionsByClass) {
            if (!first) json << ",";
            json << "\"" << className << "\":" << count;
            first = false;
        }

        json << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved detection statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get detection stats: " + std::string(e.what()), 500);
    }
}

} // namespace AISecurityVision
