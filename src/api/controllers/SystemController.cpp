#include "SystemController.h"
#include "../../core/TaskManager.h"
#include "../../database/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include <thread>

using namespace AISecurityVision;

void SystemController::handleGetStatus(const std::string& request, std::string& response) {
    try {
        logInfo("GET /api/system/status called");

        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto activePipelines = m_taskManager->getActivePipelines();
        int totalPipelines = activePipelines.size();
        
        // Get system metrics
        std::string cpuUsage = "0.0";
        std::string memoryUsage = "0.0";
        std::string gpuUsage = "0.0";
        
        try {
            cpuUsage = std::to_string(m_taskManager->getCpuUsage());
            memoryUsage = "0.0"; // TODO: Implement getMemoryUsage in TaskManager
            gpuUsage = m_taskManager->getGpuMemoryUsage();
        } catch (const std::exception& e) {
            logWarn("Failed to get system metrics: " + std::string(e.what()));
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"running\","
             << "\"version\":\"1.0.0\","
             << "\"build_date\":\"" << __DATE__ << " " << __TIME__ << "\","
             << "\"uptime_seconds\":0,"
             << "\"active_pipelines\":" << totalPipelines << ","
             << "\"system_metrics\":{"
             << "\"cpu_usage\":" << cpuUsage << ","
             << "\"memory_usage\":" << memoryUsage << ","
             << "\"gpu_usage\":\"" << gpuUsage << "\","
             << "\"disk_usage\":0.0"
             << "},"
             << "\"pipelines\":[";

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";
            
            const std::string& pipelineId = activePipelines[i];
            auto pipeline = m_taskManager->getPipeline(pipelineId);
            
            json << "{"
                 << "\"id\":\"" << pipelineId << "\","
                 << "\"status\":\"" << (pipeline ? "active" : "inactive") << "\","
                 << "\"fps\":" << (pipeline ? 25.0 : 0.0) << ","  // TODO: Implement getCurrentFPS
                 << "\"frame_count\":" << (pipeline ? 0 : 0)      // TODO: Implement getProcessedFrameCount
                 << "}";
        }

        json << "],"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("System status response generated successfully");

    } catch (const std::exception& e) {
        logError("Exception in handleGetStatus: " + std::string(e.what()));
        response = createErrorResponse("Internal server error: " + std::string(e.what()), 500);
    }
}

void SystemController::handleGetSystemInfo(const std::string& request, std::string& response) {
    try {
        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"system_name\":\"AI Security Vision System\","
             << "\"version\":\"1.0.0\","
             << "\"build_date\":\"" << __DATE__ << " " << __TIME__ << "\","
             << "\"platform\":\"RK3588 Ubuntu\","
             << "\"cpu_cores\":" << std::thread::hardware_concurrency() << ","
             << "\"memory_total\":\"8GB\","
             << "\"gpu_info\":\"" << m_taskManager->getGpuMemoryUsage() << "\","
             << "\"uptime_seconds\":0,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned system info");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system info: " + std::string(e.what()), 500);
    }
}

void SystemController::handleGetSystemMetrics(const std::string& request, std::string& response) {
    try {
        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"cpu_usage\":" << m_taskManager->getCpuUsage() << ","
             << "\"memory_usage\":0.0," // TODO: Implement getMemoryUsage in TaskManager
             << "\"gpu_memory\":\"" << m_taskManager->getGpuMemoryUsage() << "\","
             << "\"active_pipelines\":" << m_taskManager->getActivePipelineCount() << ","
             << "\"total_processed_frames\":0," // TODO: Implement getTotalProcessedFrames
             << "\"average_fps\":0.0," // TODO: Implement getAverageFPS
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system metrics: " + std::string(e.what()), 500);
    }
}

void SystemController::handleGetPipelineStats(const std::string& request, std::string& response) {
    try {
        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        auto activePipelines = m_taskManager->getActivePipelines();

        std::ostringstream json;
        json << "{"
             << "\"total_pipelines\":" << activePipelines.size() << ","
             << "\"pipelines\":[";

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";

            const std::string& pipelineId = activePipelines[i];
            auto pipeline = m_taskManager->getPipeline(pipelineId);

            json << "{"
                 << "\"id\":\"" << pipelineId << "\","
                 << "\"status\":\"" << (pipeline ? "active" : "inactive") << "\","
                 << "\"current_fps\":" << (pipeline ? 25.0 : 0.0) << ","  // TODO: Implement getCurrentFPS
                 << "\"processed_frames\":" << (pipeline ? 0 : 0) << ","   // TODO: Implement getProcessedFrameCount
                 << "\"dropped_frames\":" << (pipeline ? 0 : 0) << ","     // TODO: Implement getDroppedFrameCount
                 << "\"detection_count\":" << (pipeline ? 0 : 0) << ","    // TODO: Implement getTotalDetectionCount
                 << "\"last_frame_time\":\"" << getCurrentTimestamp() << "\""
                 << "}";
        }

        json << "],"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get pipeline stats: " + std::string(e.what()), 500);
    }
}

void SystemController::handleGetSystemStats(const std::string& request, std::string& response) {
    try {
        if (!m_taskManager) {
            response = createErrorResponse("TaskManager not initialized", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"system\":{"
             << "\"uptime_seconds\":0,"
             << "\"cpu_usage\":" << m_taskManager->getCpuUsage() << ","
             << "\"memory_usage\":0.0," // TODO: Implement getMemoryUsage in TaskManager
             << "\"disk_usage\":0.0,"
             << "\"network_rx_bytes\":0,"
             << "\"network_tx_bytes\":0"
             << "},"
             << "\"ai_processing\":{"
             << "\"active_pipelines\":" << m_taskManager->getActivePipelineCount() << ","
             << "\"total_processed_frames\":0," // TODO: Implement getTotalProcessedFrames
             << "\"average_fps\":0.0," // TODO: Implement getAverageFPS
             << "\"gpu_memory\":\"" << m_taskManager->getGpuMemoryUsage() << "\""
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system stats: " + std::string(e.what()), 500);
    }
}

std::string SystemController::readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "404 - File Not Found";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string SystemController::getMimeType(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string extension = filePath.substr(dotPos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "svg") return "image/svg+xml";
    if (extension == "ico") return "image/x-icon";
    if (extension == "woff") return "font/woff";
    if (extension == "woff2") return "font/woff2";
    if (extension == "ttf") return "font/ttf";

    return "application/octet-stream";
}

bool SystemController::fileExists(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}

std::string SystemController::loadWebFile(const std::string& filePath) {
    return readFile("web/" + filePath);
}

std::string SystemController::createFileResponse(const std::string& content, const std::string& mimeType, int statusCode) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " ";

    switch (statusCode) {
        case 200: response << "OK"; break;
        case 404: response << "Not Found"; break;
        case 500: response << "Internal Server Error"; break;
        default: response << "Unknown"; break;
    }

    response << "\r\n";
    response << "Content-Type: " << mimeType << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "\r\n";
    response << content;

    return response.str();
}

void SystemController::handleGetSystemConfig(const std::string& request, std::string& response) {
    try {
        // Initialize database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Failed to initialize database", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"system_name\":\"AI Security Vision System\","
             << "\"version\":\"1.0.0\","
             << "\"debug_mode\":false,"
             << "\"log_level\":\"INFO\","
             << "\"max_pipelines\":10,"
             << "\"monitoring_interval\":1000,";

        // Add AI configuration
        json << "\"ai\":{";

        // Load AI config from database with defaults
        std::string confidenceThreshold = dbManager.getConfig("ai", "confidence_threshold", "0.25");
        std::string nmsThreshold = dbManager.getConfig("ai", "nms_threshold", "0.45");
        std::string maxDetections = dbManager.getConfig("ai", "max_detections", "100");
        std::string detectionInterval = dbManager.getConfig("ai", "detection_interval", "1.0");
        std::string enabled = dbManager.getConfig("ai", "enabled", "true");

        json << "\"confidenceThreshold\":" << confidenceThreshold << ","
             << "\"nmsThreshold\":" << nmsThreshold << ","
             << "\"maxDetections\":" << maxDetections << ","
             << "\"detectionInterval\":" << detectionInterval << ","
             << "\"enabled\":" << (enabled == "true" ? "true" : "false");

        json << "},";

        // Add person statistics configuration
        json << "\"personStats\":{";

        std::string personEnabled = dbManager.getConfig("person_stats", "enabled", "false");
        std::string genderThreshold = dbManager.getConfig("person_stats", "gender_threshold", "0.7");
        std::string ageThreshold = dbManager.getConfig("person_stats", "age_threshold", "0.7");
        std::string batchSize = dbManager.getConfig("person_stats", "batch_size", "10");
        std::string enableCaching = dbManager.getConfig("person_stats", "enable_caching", "true");

        json << "\"enabled\":" << (personEnabled == "true" ? "true" : "false") << ","
             << "\"genderThreshold\":" << genderThreshold << ","
             << "\"ageThreshold\":" << ageThreshold << ","
             << "\"batchSize\":" << batchSize << ","
             << "\"enableCaching\":" << (enableCaching == "true" ? "true" : "false");

        json << "},";

        json << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned system configuration with AI and person stats config");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system config: " + std::string(e.what()), 500);
        logError("Exception in handleGetSystemConfig: " + std::string(e.what()));
    }
}

void SystemController::handlePostSystemConfig(const std::string& request, std::string& response) {
    try {
        logInfo("Received system config update request: " + request);

        // Parse JSON request
        nlohmann::json configJson;
        try {
            configJson = nlohmann::json::parse(request);
        } catch (const std::exception& e) {
            response = createErrorResponse("Invalid JSON format: " + std::string(e.what()), 400);
            return;
        }

        // Initialize database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Failed to initialize database", 500);
            return;
        }

        bool hasUpdates = false;
        std::vector<std::string> updatedSections;

        // Handle AI configuration
        if (configJson.contains("ai")) {
            const auto& aiConfig = configJson["ai"];

            if (aiConfig.contains("confidenceThreshold")) {
                double threshold = aiConfig["confidenceThreshold"];
                dbManager.saveConfig("ai", "confidence_threshold", std::to_string(threshold));
                hasUpdates = true;
            }

            if (aiConfig.contains("nmsThreshold")) {
                double threshold = aiConfig["nmsThreshold"];
                dbManager.saveConfig("ai", "nms_threshold", std::to_string(threshold));
                hasUpdates = true;
            }

            if (aiConfig.contains("maxDetections")) {
                int maxDet = aiConfig["maxDetections"];
                dbManager.saveConfig("ai", "max_detections", std::to_string(maxDet));
                hasUpdates = true;
            }

            if (aiConfig.contains("detectionInterval")) {
                double interval = aiConfig["detectionInterval"];
                dbManager.saveConfig("ai", "detection_interval", std::to_string(interval));
                hasUpdates = true;
            }

            if (aiConfig.contains("enabled")) {
                bool enabled = aiConfig["enabled"];
                dbManager.saveConfig("ai", "enabled", enabled ? "true" : "false");
                hasUpdates = true;
            }

            if (hasUpdates) {
                updatedSections.push_back("AI");
                logInfo("Updated AI configuration");
            }
        }

        // Handle person statistics configuration
        if (configJson.contains("personStats")) {
            const auto& personConfig = configJson["personStats"];

            if (personConfig.contains("enabled")) {
                bool enabled = personConfig["enabled"];
                dbManager.saveConfig("person_stats", "enabled", enabled ? "true" : "false");
                hasUpdates = true;
            }

            if (personConfig.contains("genderThreshold")) {
                double threshold = personConfig["genderThreshold"];
                dbManager.saveConfig("person_stats", "gender_threshold", std::to_string(threshold));
                hasUpdates = true;
            }

            if (personConfig.contains("ageThreshold")) {
                double threshold = personConfig["ageThreshold"];
                dbManager.saveConfig("person_stats", "age_threshold", std::to_string(threshold));
                hasUpdates = true;
            }

            if (personConfig.contains("batchSize")) {
                int batchSize = personConfig["batchSize"];
                dbManager.saveConfig("person_stats", "batch_size", std::to_string(batchSize));
                hasUpdates = true;
            }

            if (personConfig.contains("enableCaching")) {
                bool caching = personConfig["enableCaching"];
                dbManager.saveConfig("person_stats", "enable_caching", caching ? "true" : "false");
                hasUpdates = true;
            }

            if (hasUpdates) {
                updatedSections.push_back("Person Statistics");
                logInfo("Updated person statistics configuration");
            }
        }

        // Handle system configuration
        if (configJson.contains("system")) {
            const auto& sysConfig = configJson["system"];

            if (sysConfig.contains("systemName")) {
                std::string name = sysConfig["systemName"];
                dbManager.saveConfig("system", "system_name", name);
                hasUpdates = true;
            }

            if (sysConfig.contains("debugMode")) {
                bool debug = sysConfig["debugMode"];
                dbManager.saveConfig("system", "debug_mode", debug ? "true" : "false");
                hasUpdates = true;
            }

            if (sysConfig.contains("logLevel")) {
                std::string level = sysConfig["logLevel"];
                dbManager.saveConfig("system", "log_level", level);
                hasUpdates = true;
            }

            if (hasUpdates) {
                updatedSections.push_back("System");
                logInfo("Updated system configuration");
            }
        }

        if (!hasUpdates) {
            response = createErrorResponse("No valid configuration updates found", 400);
            return;
        }

        // Create success response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Configuration updated successfully\","
             << "\"updated_sections\":[";

        for (size_t i = 0; i < updatedSections.size(); ++i) {
            if (i > 0) json << ",";
            json << "\"" << updatedSections[i] << "\"";
        }

        json << "],"
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("System configuration updated successfully");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update system config: " + std::string(e.what()), 500);
        logError("Exception in handlePostSystemConfig: " + std::string(e.what()));
    }
}

void SystemController::handleGetConfigCategory(const std::string& category, std::string& response) {
    try {
        std::ostringstream json;
        json << "{"
             << "\"category\":\"" << category << "\","
             << "\"config\":{},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned config for category: " + category);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get config category: " + std::string(e.what()), 500);
    }
}

void SystemController::handleGetDashboard(const std::string& request, std::string& response) {
    try {
        std::string dashboardHtml = loadWebFile("index.html");
        if (dashboardHtml.empty() || dashboardHtml == "404 - File Not Found") {
            response = createErrorResponse("Dashboard not found", 404);
            return;
        }

        response = createFileResponse(dashboardHtml, "text/html");
        logInfo("Served dashboard");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to load dashboard: " + std::string(e.what()), 500);
    }
}

void SystemController::handleStaticFile(const std::string& request, std::string& response, const std::string& filePath) {
    try {
        std::string content = loadWebFile(filePath);
        if (content.empty() || content == "404 - File Not Found") {
            response = createErrorResponse("File not found", 404);
            return;
        }

        std::string mimeType = getMimeType(filePath);
        response = createFileResponse(content, mimeType);
        logInfo("Served static file: " + filePath);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to load file: " + std::string(e.what()), 500);
    }
}
