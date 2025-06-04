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
        std::ostringstream json;
        json << "{"
             << "\"system_name\":\"AI Security Vision System\","
             << "\"version\":\"1.0.0\","
             << "\"debug_mode\":false,"
             << "\"log_level\":\"INFO\","
             << "\"max_pipelines\":10,"
             << "\"monitoring_interval\":1000,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned system configuration");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system config: " + std::string(e.what()), 500);
    }
}

void SystemController::handlePostSystemConfig(const std::string& request, std::string& response) {
    try {
        // TODO: Parse and apply system configuration changes
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"System configuration updated\","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("System configuration updated");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update system config: " + std::string(e.what()), 500);
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
