#include "APIService.h"
#include "../core/TaskManager.h"
#include "../ai/BehaviorAnalyzer.h"
#include "../utils/PolygonValidator.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cctype>
#include <regex>
#include <algorithm>
#include <fstream>

APIService::APIService(int port) : m_port(port), m_httpServer(nullptr) {
    std::cout << "[APIService] Initializing API service on port " << port << std::endl;
}

APIService::~APIService() {
    stop();
}

bool APIService::start() {
    if (m_running.load()) {
        std::cout << "[APIService] Service already running" << std::endl;
        return true;
    }

    try {
        m_running.store(true);
        m_serverThread = std::thread(&APIService::serverThread, this);

        // Give the server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "[APIService] API service started on port " << m_port << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[APIService] Failed to start: " << e.what() << std::endl;
        m_running.store(false);
        return false;
    }
}

void APIService::stop() {
    if (!m_running.load()) {
        return;
    }

    std::cout << "[APIService] Stopping API service..." << std::endl;

    m_running.store(false);

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    std::cout << "[APIService] API service stopped" << std::endl;
}

bool APIService::isRunning() const {
    return m_running.load();
}

void APIService::setPort(int port) {
    m_port = port;
}

int APIService::getPort() const {
    return m_port;
}

void APIService::serverThread() {
    std::cout << "[APIService] Server thread started" << std::endl;

    // TODO: Implement actual HTTP server using httplib
    // For now, this is a placeholder that simulates a running server

    while (m_running.load()) {
        // Simulate server processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // In a real implementation, this would:
        // 1. Listen for HTTP requests
        // 2. Parse request paths and methods
        // 3. Route to appropriate handlers
        // 4. Send responses back to clients
    }

    std::cout << "[APIService] Server thread stopped" << std::endl;
}

void APIService::setupRoutes() {
    // TODO: Setup HTTP routes

    // System endpoints
    // GET /api/system/status - Basic system status
    // GET /api/system/metrics - Detailed system metrics

    // Video source management
    // POST /api/source/add - Add new video source
    // DELETE /api/source/{id} - Remove video source
    // GET /api/source/list - List all video sources
    // POST /api/source/discover - Discover ONVIF devices

    // Recording management
    // POST /api/record/start - Start manual recording
    // POST /api/record/stop - Stop manual recording
    // POST /api/record/config - Update recording configuration
    // GET /api/record/status - Get recording status

    // Face management
    // GET /api/faces - List all faces
    // POST /api/faces/add - Add new face
    // DELETE /api/faces/{id} - Delete face

    // Behavior rules management
    // POST /api/rules - Create new rule
    // GET /api/rules - List all rules
    // PUT /api/rules/{id} - Update rule
    // DELETE /api/rules/{id} - Delete rule
}

void APIService::handleGetStatus(const std::string& request, std::string& response) {
    TaskManager& taskManager = TaskManager::getInstance();

    std::ostringstream json;
    json << "{"
         << "\"status\":\"running\","
         << "\"active_pipelines\":" << taskManager.getActivePipelineCount() << ","
         << "\"cpu_usage\":" << taskManager.getCpuUsage() << ","
         << "\"gpu_memory\":\"" << taskManager.getGpuMemoryUsage() << "\","
         << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
         << "}";

    response = createJsonResponse(json.str());
}

void APIService::handlePostVideoSource(const std::string& request, std::string& response) {
    // TODO: Parse JSON request to extract video source parameters
    // For now, return a placeholder response

    response = createJsonResponse("{\"message\":\"Video source endpoint not implemented yet\"}");
}

void APIService::handleDeleteVideoSource(const std::string& request, std::string& response) {
    // TODO: Extract source ID from URL path and remove from TaskManager

    response = createJsonResponse("{\"message\":\"Delete video source endpoint not implemented yet\"}");
}

void APIService::handleGetVideoSources(const std::string& request, std::string& response) {
    TaskManager& taskManager = TaskManager::getInstance();
    auto activePipelines = taskManager.getActivePipelines();

    std::ostringstream json;
    json << "{\"sources\":[";

    for (size_t i = 0; i < activePipelines.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":\"" << activePipelines[i] << "\",\"status\":\"active\"}";
    }

    json << "]}";

    response = createJsonResponse(json.str());
}

std::string APIService::createJsonResponse(const std::string& data, int statusCode) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << data.length() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "\r\n"
             << data;
    return response.str();
}

std::string APIService::createErrorResponse(const std::string& error, int statusCode) {
    std::ostringstream json;
    json << "{\"error\":\"" << error << "\"}";
    return createJsonResponse(json.str(), statusCode);
}

// Recording API handlers
void APIService::handlePostRecordStart(const std::string& request, std::string& response) {
    try {
        // Parse duration from request body
        int duration = parseJsonInt(request, "duration", 60); // Default 60 seconds
        std::string cameraId = parseJsonField(request, "camera_id");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        // Validate duration
        if (duration < 10 || duration > 300) {
            response = createErrorResponse("Duration must be between 10 and 300 seconds", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // TODO: Access recorder from pipeline and start manual recording
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"recording_started\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"duration\":" << duration << ","
             << "\"start_time\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Manual recording started for camera: " << cameraId
                  << ", duration: " << duration << "s" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to start recording: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostRecordStop(const std::string& request, std::string& response) {
    try {
        std::string cameraId = parseJsonField(request, "camera_id");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // TODO: Access recorder from pipeline and stop manual recording
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"recording_stopped\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"stop_time\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Manual recording stopped for camera: " << cameraId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to stop recording: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostRecordConfig(const std::string& request, std::string& response) {
    try {
        int preEventDuration = parseJsonInt(request, "pre_event_duration", 30);
        int postEventDuration = parseJsonInt(request, "post_event_duration", 30);
        std::string outputDir = parseJsonField(request, "output_dir");

        // Validate durations
        if (preEventDuration < 10 || preEventDuration > 300) {
            response = createErrorResponse("pre_event_duration must be between 10 and 300 seconds", 400);
            return;
        }

        if (postEventDuration < 10 || postEventDuration > 300) {
            response = createErrorResponse("post_event_duration must be between 10 and 300 seconds", 400);
            return;
        }

        // TODO: Update recording configuration for all pipelines
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"config_updated\","
             << "\"pre_event_duration\":" << preEventDuration << ","
             << "\"post_event_duration\":" << postEventDuration;

        if (!outputDir.empty()) {
            json << ",\"output_dir\":\"" << outputDir << "\"";
        }

        json << ",\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Recording configuration updated: pre=" << preEventDuration
                  << "s, post=" << postEventDuration << "s" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update config: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetRecordStatus(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        std::ostringstream json;
        json << "{\"cameras\":[";

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";

            const std::string& cameraId = activePipelines[i];
            auto pipeline = taskManager.getPipeline(cameraId);

            json << "{"
                 << "\"camera_id\":\"" << cameraId << "\","
                 << "\"is_recording\":false," // TODO: Get actual recording status
                 << "\"recording_path\":\"\","  // TODO: Get current recording path
                 << "\"buffer_size\":0"        // TODO: Get buffer size
                 << "}";
        }

        json << "],"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get recording status: " + std::string(e.what()), 500);
    }
}

// System monitoring handler
void APIService::handleGetSystemMetrics(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();

        std::ostringstream json;
        json << "{"
             << "\"system_status\":\"running\","
             << "\"cpu_usage\":" << taskManager.getCpuUsage() << ","
             << "\"gpu_memory\":\"" << taskManager.getGpuMemoryUsage() << "\","
             << "\"gpu_utilization\":" << taskManager.getGpuUtilization() << ","
             << "\"gpu_temperature\":" << taskManager.getGpuTemperature() << ","
             << "\"active_pipelines\":" << taskManager.getActivePipelineCount() << ","
             << "\"uptime_seconds\":" << std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count() << ","
             << "\"memory_usage\":{"
             << "\"total_mb\":0,"      // TODO: Get actual memory usage
             << "\"used_mb\":0,"       // TODO: Get actual memory usage
             << "\"available_mb\":0"   // TODO: Get actual memory usage
             << "},"
             << "\"disk_usage\":{"
             << "\"total_gb\":0,"      // TODO: Get actual disk usage
             << "\"used_gb\":0,"       // TODO: Get actual disk usage
             << "\"available_gb\":0"   // TODO: Get actual disk usage
             << "},"
             << "\"network\":{"
             << "\"bytes_received\":0," // TODO: Get network stats
             << "\"bytes_sent\":0"      // TODO: Get network stats
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system metrics: " + std::string(e.what()), 500);
    }
}

// Enhanced pipeline statistics handler
void APIService::handleGetPipelineStats(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto pipelineStats = taskManager.getAllPipelineStats();

        std::ostringstream json;
        json << "{\"pipelines\":[";

        for (size_t i = 0; i < pipelineStats.size(); ++i) {
            if (i > 0) json << ",";

            const auto& stats = pipelineStats[i];
            json << "{"
                 << "\"source_id\":\"" << stats.sourceId << "\","
                 << "\"protocol\":\"" << stats.protocol << "\","
                 << "\"url\":\"" << stats.url << "\","
                 << "\"is_running\":" << (stats.isRunning ? "true" : "false") << ","
                 << "\"is_healthy\":" << (stats.isHealthy ? "true" : "false") << ","
                 << "\"frame_rate\":" << stats.frameRate << ","
                 << "\"processed_frames\":" << stats.processedFrames << ","
                 << "\"dropped_frames\":" << stats.droppedFrames << ","
                 << "\"last_error\":\"" << stats.lastError << "\","
                 << "\"uptime_seconds\":" << stats.uptime
                 << "}";
        }

        json << "],"
             << "\"total_pipelines\":" << pipelineStats.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get pipeline stats: " + std::string(e.what()), 500);
    }
}

// Enhanced system statistics handler
void APIService::handleGetSystemStats(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto systemStats = taskManager.getSystemStats();

        std::ostringstream json;
        json << "{"
             << "\"system\":{"
             << "\"total_pipelines\":" << systemStats.totalPipelines << ","
             << "\"running_pipelines\":" << systemStats.runningPipelines << ","
             << "\"healthy_pipelines\":" << systemStats.healthyPipelines << ","
             << "\"total_frame_rate\":" << systemStats.totalFrameRate << ","
             << "\"total_processed_frames\":" << systemStats.totalProcessedFrames << ","
             << "\"total_dropped_frames\":" << systemStats.totalDroppedFrames << ","
             << "\"uptime_seconds\":" << systemStats.systemUptime
             << "},"
             << "\"resources\":{"
             << "\"cpu_usage\":" << systemStats.cpuUsage << ","
             << "\"gpu_memory\":\"" << systemStats.gpuMemUsage << "\","
             << "\"gpu_utilization\":" << systemStats.gpuUtilization << ","
             << "\"gpu_temperature\":" << systemStats.gpuTemperature
             << "},"
             << "\"performance\":{"
             << "\"avg_frame_rate\":" << (systemStats.runningPipelines > 0 ?
                 systemStats.totalFrameRate / systemStats.runningPipelines : 0.0) << ","
             << "\"drop_rate\":" << (systemStats.totalProcessedFrames > 0 ?
                 (double)systemStats.totalDroppedFrames / systemStats.totalProcessedFrames * 100.0 : 0.0) << ","
             << "\"health_ratio\":" << (systemStats.totalPipelines > 0 ?
                 (double)systemStats.healthyPipelines / systemStats.totalPipelines * 100.0 : 100.0)
             << "},"
             << "\"monitoring\":{"
             << "\"cycles\":" << taskManager.getMonitoringCycles() << ","
             << "\"avg_cycle_time\":" << taskManager.getAverageMonitoringTime() << ","
             << "\"max_cycle_time\":" << taskManager.getMaxMonitoringTime() << ","
             << "\"healthy\":" << (taskManager.isMonitoringHealthy() ? "true" : "false") << ","
             << "\"target_interval\":" << TaskManager::MONITORING_INTERVAL_MS
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system stats: " + std::string(e.what()), 500);
    }
}

// Utility functions
std::string APIService::parseJsonField(const std::string& json, const std::string& field) {
    // Simple JSON field extraction (not a full parser)
    std::string searchStr = "\"" + field + "\":\"";
    size_t start = json.find(searchStr);

    if (start == std::string::npos) {
        return "";
    }

    start += searchStr.length();
    size_t end = json.find("\"", start);

    if (end == std::string::npos) {
        return "";
    }

    return json.substr(start, end - start);
}

int APIService::parseJsonInt(const std::string& json, const std::string& field, int defaultValue) {
    // Simple JSON integer field extraction
    std::string searchStr = "\"" + field + "\":";
    size_t start = json.find(searchStr);

    if (start == std::string::npos) {
        return defaultValue;
    }

    start += searchStr.length();

    // Skip whitespace
    while (start < json.length() && std::isspace(json[start])) {
        start++;
    }

    // Find end of number
    size_t end = start;
    while (end < json.length() && (std::isdigit(json[end]) || json[end] == '-')) {
        end++;
    }

    if (end == start) {
        return defaultValue;
    }

    try {
        return std::stoi(json.substr(start, end - start));
    } catch (const std::exception&) {
        return defaultValue;
    }
}

std::string APIService::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Streaming configuration handlers
void APIService::handlePostStreamConfig(const std::string& request, std::string& response) {
    try {
        std::string cameraId = parseJsonField(request, "camera_id");
        std::string protocol = parseJsonField(request, "protocol");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        if (protocol.empty()) {
            response = createErrorResponse("protocol is required (mjpeg or rtmp)", 400);
            return;
        }

        if (protocol != "mjpeg" && protocol != "rtmp") {
            response = createErrorResponse("protocol must be 'mjpeg' or 'rtmp'", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // Parse configuration parameters
        int width = parseJsonInt(request, "width", 640);
        int height = parseJsonInt(request, "height", 480);
        int fps = parseJsonInt(request, "fps", 15);
        int quality = parseJsonInt(request, "quality", 80);
        int bitrate = parseJsonInt(request, "bitrate", 2000000);
        int port = parseJsonInt(request, "port", 8000);
        std::string rtmpUrl = parseJsonField(request, "rtmp_url");
        std::string endpoint = parseJsonField(request, "endpoint");

        // Validate parameters
        if (width < 320 || width > 1920) {
            response = createErrorResponse("width must be between 320 and 1920", 400);
            return;
        }

        if (height < 240 || height > 1080) {
            response = createErrorResponse("height must be between 240 and 1080", 400);
            return;
        }

        if (fps < 1 || fps > 60) {
            response = createErrorResponse("fps must be between 1 and 60", 400);
            return;
        }

        if (protocol == "rtmp" && rtmpUrl.empty()) {
            response = createErrorResponse("rtmp_url is required for RTMP protocol", 400);
            return;
        }

        // TODO: Configure streaming for the pipeline
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"configured\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"protocol\":\"" << protocol << "\","
             << "\"width\":" << width << ","
             << "\"height\":" << height << ","
             << "\"fps\":" << fps;

        if (protocol == "mjpeg") {
            json << ",\"quality\":" << quality
                 << ",\"port\":" << port;
            if (!endpoint.empty()) {
                json << ",\"endpoint\":\"" << endpoint << "\"";
            }
        } else if (protocol == "rtmp") {
            json << ",\"bitrate\":" << bitrate
                 << ",\"rtmp_url\":\"" << rtmpUrl << "\"";
        }

        json << ",\"configured_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Configured " << protocol << " streaming for camera: " << cameraId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to configure streaming: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetStreamConfig(const std::string& request, std::string& response) {
    try {
        std::string cameraId = parseJsonField(request, "camera_id");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // TODO: Get actual streaming configuration from pipeline
        // For now, return default configuration

        std::ostringstream json;
        json << "{"
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"protocol\":\"mjpeg\","
             << "\"width\":640,"
             << "\"height\":480,"
             << "\"fps\":15,"
             << "\"quality\":80,"
             << "\"port\":8000,"
             << "\"endpoint\":\"/stream.mjpg\","
             << "\"enabled\":true,"
             << "\"stream_url\":\"http://localhost:8000/stream.mjpg\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get stream config: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostStreamStart(const std::string& request, std::string& response) {
    try {
        std::string cameraId = parseJsonField(request, "camera_id");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // TODO: Start streaming for the pipeline
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"streaming_started\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"stream_url\":\"http://localhost:8000/stream.mjpg\","
             << "\"started_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Started streaming for camera: " << cameraId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to start streaming: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostStreamStop(const std::string& request, std::string& response) {
    try {
        std::string cameraId = parseJsonField(request, "camera_id");

        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            response = createErrorResponse("Camera not found: " + cameraId, 404);
            return;
        }

        // TODO: Stop streaming for the pipeline
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"streaming_stopped\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"stopped_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Stopped streaming for camera: " << cameraId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to stop streaming: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetStreamStatus(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        std::ostringstream json;
        json << "{\"streams\":[";

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";

            const std::string& cameraId = activePipelines[i];
            auto pipeline = taskManager.getPipeline(cameraId);

            json << "{"
                 << "\"camera_id\":\"" << cameraId << "\","
                 << "\"protocol\":\"mjpeg\","
                 << "\"is_streaming\":true,"
                 << "\"stream_url\":\"http://localhost:8000/stream.mjpg\","
                 << "\"connected_clients\":0,"
                 << "\"stream_fps\":15.0,"
                 << "\"health\":\"healthy\""
                 << "}";
        }

        json << "],"
             << "\"total_streams\":" << activePipelines.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get stream status: " + std::string(e.what()), 500);
    }
}

// Behavior rule management handlers
void APIService::handlePostRules(const std::string& request, std::string& response) {
    try {
        // Parse the JSON request to create an IntrusionRule
        IntrusionRule rule;
        if (!deserializeIntrusionRule(request, rule)) {
            response = createErrorResponse("Invalid rule format", 400);
            return;
        }

        // Validate the rule
        if (rule.id.empty()) {
            response = createErrorResponse("Rule ID is required", 400);
            return;
        }

        if (rule.roi.id.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        // Enhanced polygon validation with detailed error reporting
        auto validationResult = validateROIPolygonDetailed(rule.roi.polygon);
        if (!validationResult.isValid) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"" << validationResult.errorMessage << "\","
                     << "\"error_code\":\"" << validationResult.errorCode << "\","
                     << "\"polygon_points\":" << rule.roi.polygon.size() << ","
                     << "\"validation_details\":{"
                     << "\"area\":" << validationResult.area << ","
                     << "\"is_closed\":" << (validationResult.isClosed ? "true" : "false") << ","
                     << "\"is_convex\":" << (validationResult.isConvex ? "true" : "false") << ","
                     << "\"has_self_intersection\":" << (validationResult.hasSelfIntersection ? "true" : "false")
                     << "}"
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // Get the BehaviorAnalyzer from TaskManager
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createErrorResponse("No active video pipelines found", 404);
            return;
        }

        // For now, add the rule to the first active pipeline
        // TODO: Support specifying which pipeline to add the rule to
        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        // TODO: Access BehaviorAnalyzer from pipeline and add rule
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"created\","
             << "\"rule_id\":\"" << rule.id << "\","
             << "\"roi_id\":\"" << rule.roi.id << "\","
             << "\"min_duration\":" << rule.minDuration << ","
             << "\"confidence\":" << rule.confidence << ","
             << "\"enabled\":" << (rule.enabled ? "true" : "false") << ","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        std::cout << "[APIService] Created intrusion rule: " << rule.id
                  << " with ROI: " << rule.roi.id << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to create rule: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetRules(const std::string& request, std::string& response) {
    try {
        // Get all rules from BehaviorAnalyzer
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createJsonResponse("{\"rules\":[]}");
            return;
        }

        // TODO: Access BehaviorAnalyzer from pipeline and get rules
        // For now, return a sample rule list

        std::ostringstream json;
        json << "{"
             << "\"rules\":["
             << "{"
             << "\"id\":\"default_intrusion\","
             << "\"roi\":{"
             << "\"id\":\"default_roi\","
             << "\"name\":\"Default Intrusion Zone\","
             << "\"polygon\":[{\"x\":100,\"y\":100},{\"x\":500,\"y\":100},{\"x\":500,\"y\":400},{\"x\":100,\"y\":400}],"
             << "\"enabled\":true,"
             << "\"priority\":1"
             << "},"
             << "\"min_duration\":5.0,"
             << "\"confidence\":0.7,"
             << "\"enabled\":true"
             << "}"
             << "],"
             << "\"count\":1,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get rules: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetRule(const std::string& request, std::string& response, const std::string& ruleId) {
    try {
        if (ruleId.empty()) {
            response = createErrorResponse("Rule ID is required", 400);
            return;
        }

        // TODO: Get specific rule from BehaviorAnalyzer
        // For now, return a sample rule if ID matches

        if (ruleId == "default_intrusion") {
            std::ostringstream json;
            json << "{"
                 << "\"id\":\"default_intrusion\","
                 << "\"roi\":{"
                 << "\"id\":\"default_roi\","
                 << "\"name\":\"Default Intrusion Zone\","
                 << "\"polygon\":[{\"x\":100,\"y\":100},{\"x\":500,\"y\":100},{\"x\":500,\"y\":400},{\"x\":100,\"y\":400}],"
                 << "\"enabled\":true,"
                 << "\"priority\":1"
                 << "},"
                 << "\"min_duration\":5.0,"
                 << "\"confidence\":0.7,"
                 << "\"enabled\":true,"
                 << "\"created_at\":\"2024-01-01 00:00:00.000\","
                 << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";

            response = createJsonResponse(json.str());
        } else {
            response = createErrorResponse("Rule not found: " + ruleId, 404);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get rule: " + std::string(e.what()), 500);
    }
}

void APIService::handlePutRule(const std::string& request, std::string& response, const std::string& ruleId) {
    try {
        if (ruleId.empty()) {
            response = createErrorResponse("Rule ID is required", 400);
            return;
        }

        // Parse the JSON request to update the IntrusionRule
        IntrusionRule rule;
        if (!deserializeIntrusionRule(request, rule)) {
            response = createErrorResponse("Invalid rule format", 400);
            return;
        }

        // Ensure the rule ID matches the URL parameter
        rule.id = ruleId;

        // Validate the rule
        if (rule.roi.id.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        // Enhanced polygon validation with detailed error reporting
        auto validationResult = validateROIPolygonDetailed(rule.roi.polygon);
        if (!validationResult.isValid) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"" << validationResult.errorMessage << "\","
                     << "\"error_code\":\"" << validationResult.errorCode << "\","
                     << "\"polygon_points\":" << rule.roi.polygon.size() << ","
                     << "\"validation_details\":{"
                     << "\"area\":" << validationResult.area << ","
                     << "\"is_closed\":" << (validationResult.isClosed ? "true" : "false") << ","
                     << "\"is_convex\":" << (validationResult.isConvex ? "true" : "false") << ","
                     << "\"has_self_intersection\":" << (validationResult.hasSelfIntersection ? "true" : "false")
                     << "}"
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // TODO: Update rule in BehaviorAnalyzer
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"updated\","
             << "\"rule_id\":\"" << rule.id << "\","
             << "\"roi_id\":\"" << rule.roi.id << "\","
             << "\"min_duration\":" << rule.minDuration << ","
             << "\"confidence\":" << rule.confidence << ","
             << "\"enabled\":" << (rule.enabled ? "true" : "false") << ","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Updated intrusion rule: " << rule.id << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update rule: " + std::string(e.what()), 500);
    }
}

void APIService::handleDeleteRule(const std::string& request, std::string& response, const std::string& ruleId) {
    try {
        if (ruleId.empty()) {
            response = createErrorResponse("Rule ID is required", 400);
            return;
        }

        // TODO: Delete rule from BehaviorAnalyzer
        // For now, simulate success for known rule

        if (ruleId == "default_intrusion") {
            response = createErrorResponse("Cannot delete default rule", 403);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"deleted\","
             << "\"rule_id\":\"" << ruleId << "\","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Deleted intrusion rule: " << ruleId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete rule: " + std::string(e.what()), 500);
    }
}

// ROI management handlers
void APIService::handlePostROIs(const std::string& request, std::string& response) {
    try {
        // Parse the JSON request to create an ROI
        ROI roi;
        if (!deserializeROI(request, roi)) {
            response = createErrorResponse("Invalid ROI format", 400);
            return;
        }

        // Validate the ROI
        if (roi.id.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        if (roi.name.empty()) {
            response = createErrorResponse("ROI name is required", 400);
            return;
        }

        // Enhanced polygon validation with detailed error reporting
        auto validationResult = validateROIPolygonDetailed(roi.polygon);
        if (!validationResult.isValid) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"" << validationResult.errorMessage << "\","
                     << "\"error_code\":\"" << validationResult.errorCode << "\","
                     << "\"polygon_points\":" << roi.polygon.size() << ","
                     << "\"validation_details\":{"
                     << "\"area\":" << validationResult.area << ","
                     << "\"is_closed\":" << (validationResult.isClosed ? "true" : "false") << ","
                     << "\"is_convex\":" << (validationResult.isConvex ? "true" : "false") << ","
                     << "\"has_self_intersection\":" << (validationResult.hasSelfIntersection ? "true" : "false")
                     << "}"
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // TODO: Add ROI to BehaviorAnalyzer
        // For now, simulate success

        std::ostringstream json;
        json << "{"
             << "\"status\":\"created\","
             << "\"roi_id\":\"" << roi.id << "\","
             << "\"name\":\"" << roi.name << "\","
             << "\"polygon_points\":" << roi.polygon.size() << ","
             << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
             << "\"priority\":" << roi.priority << ","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        std::cout << "[APIService] Created ROI: " << roi.id << " (" << roi.name << ")" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to create ROI: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetROIs(const std::string& request, std::string& response) {
    try {
        // TODO: Get all ROIs from BehaviorAnalyzer
        // For now, return a sample ROI list

        std::ostringstream json;
        json << "{"
             << "\"rois\":["
             << "{"
             << "\"id\":\"default_roi\","
             << "\"name\":\"Default Intrusion Zone\","
             << "\"polygon\":[{\"x\":100,\"y\":100},{\"x\":500,\"y\":100},{\"x\":500,\"y\":400},{\"x\":100,\"y\":400}],"
             << "\"enabled\":true,"
             << "\"priority\":1"
             << "}"
             << "],"
             << "\"count\":1,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get ROIs: " + std::string(e.what()), 500);
    }
}

// JSON serialization methods
std::string APIService::serializeROI(const ROI& roi) {
    std::ostringstream json;
    json << "{"
         << "\"id\":\"" << roi.id << "\","
         << "\"name\":\"" << roi.name << "\","
         << "\"polygon\":[";

    for (size_t i = 0; i < roi.polygon.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"x\":" << roi.polygon[i].x << ",\"y\":" << roi.polygon[i].y << "}";
    }

    json << "],"
         << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
         << "\"priority\":" << roi.priority
         << "}";

    return json.str();
}

std::string APIService::serializeIntrusionRule(const IntrusionRule& rule) {
    std::ostringstream json;
    json << "{"
         << "\"id\":\"" << rule.id << "\","
         << "\"roi\":" << serializeROI(rule.roi) << ","
         << "\"min_duration\":" << rule.minDuration << ","
         << "\"confidence\":" << rule.confidence << ","
         << "\"enabled\":" << (rule.enabled ? "true" : "false")
         << "}";

    return json.str();
}

std::string APIService::serializeROIList(const std::vector<ROI>& rois) {
    std::ostringstream json;
    json << "[";

    for (size_t i = 0; i < rois.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeROI(rois[i]);
    }

    json << "]";
    return json.str();
}

std::string APIService::serializeRuleList(const std::vector<IntrusionRule>& rules) {
    std::ostringstream json;
    json << "[";

    for (size_t i = 0; i < rules.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeIntrusionRule(rules[i]);
    }

    json << "]";
    return json.str();
}

// JSON deserialization methods
bool APIService::deserializeROI(const std::string& json, ROI& roi) {
    try {
        // Simple JSON parsing for ROI
        roi.id = parseJsonField(json, "id");
        roi.name = parseJsonField(json, "name");
        roi.enabled = parseJsonField(json, "enabled") != "false";
        roi.priority = parseJsonInt(json, "priority", 1);

        // Parse polygon points
        std::regex polygonRegex(R"("polygon"\s*:\s*\[(.*?)\])");
        std::smatch polygonMatch;

        if (std::regex_search(json, polygonMatch, polygonRegex)) {
            std::string polygonStr = polygonMatch[1].str();

            // Parse individual points
            std::regex pointRegex(R"(\{\s*"x"\s*:\s*(\d+)\s*,\s*"y"\s*:\s*(\d+)\s*\})");
            std::sregex_iterator pointIter(polygonStr.begin(), polygonStr.end(), pointRegex);
            std::sregex_iterator pointEnd;

            roi.polygon.clear();
            for (; pointIter != pointEnd; ++pointIter) {
                int x = std::stoi((*pointIter)[1].str());
                int y = std::stoi((*pointIter)[2].str());
                roi.polygon.emplace_back(x, y);
            }
        }

        return !roi.id.empty() && !roi.name.empty();

    } catch (const std::exception& e) {
        std::cerr << "[APIService] Failed to deserialize ROI: " << e.what() << std::endl;
        return false;
    }
}

bool APIService::deserializeIntrusionRule(const std::string& json, IntrusionRule& rule) {
    try {
        // Parse basic rule fields
        rule.id = parseJsonField(json, "id");
        rule.minDuration = std::stod(parseJsonField(json, "min_duration"));
        rule.confidence = std::stod(parseJsonField(json, "confidence"));
        rule.enabled = parseJsonField(json, "enabled") != "false";

        // Parse embedded ROI
        std::regex roiRegex(R"("roi"\s*:\s*(\{.*?\}))");
        std::smatch roiMatch;

        if (std::regex_search(json, roiMatch, roiRegex)) {
            std::string roiJson = roiMatch[1].str();
            if (!deserializeROI(roiJson, rule.roi)) {
                return false;
            }
        } else {
            return false;
        }

        return !rule.id.empty() && !rule.roi.id.empty();

    } catch (const std::exception& e) {
        std::cerr << "[APIService] Failed to deserialize IntrusionRule: " << e.what() << std::endl;
        return false;
    }
}

bool APIService::validateROIPolygon(const std::vector<cv::Point>& polygon) {
    // Use the enhanced validation for backward compatibility
    return validateROIPolygonDetailed(polygon).isValid;
}

APIService::PolygonValidationResult APIService::validateROIPolygonDetailed(const std::vector<cv::Point>& polygon) {
    // Create polygon validator with appropriate configuration for ROI validation
    PolygonValidator::ValidationConfig config;
    config.minPoints = 3;
    config.maxPoints = 50;  // Reasonable limit for ROI polygons
    config.minX = 0;
    config.maxX = 10000;
    config.minY = 0;
    config.maxY = 10000;
    config.minArea = 100.0;  // Minimum 100 square pixels
    config.maxArea = 1000000.0;  // Maximum 1M square pixels
    config.requireClosed = false;  // Allow open polygons for flexibility
    config.allowSelfIntersection = false;  // Disallow self-intersecting polygons
    config.requireConvex = false;  // Allow non-convex polygons

    PolygonValidator validator(config);
    auto result = validator.validate(polygon);

    // Convert to APIService result format
    PolygonValidationResult apiResult;
    apiResult.isValid = result.isValid;
    apiResult.errorMessage = result.errorMessage;
    apiResult.errorCode = result.errorCode;
    apiResult.area = result.area;
    apiResult.isClosed = result.isClosed;
    apiResult.isConvex = result.isConvex;
    apiResult.hasSelfIntersection = result.hasSelfIntersection;

    return apiResult;
}

// Web dashboard handlers
void APIService::handleGetDashboard(const std::string& request, std::string& response) {
    try {
        std::string dashboardPath = "web/templates/dashboard.html";

        if (!fileExists(dashboardPath)) {
            response = createErrorResponse("Dashboard not found", 404);
            return;
        }

        std::string content = readFile(dashboardPath);
        if (content.empty()) {
            response = createErrorResponse("Failed to load dashboard", 500);
            return;
        }

        response = createFileResponse(content, "text/html");

    } catch (const std::exception& e) {
        response = createErrorResponse("Dashboard error: " + std::string(e.what()), 500);
    }
}

void APIService::handleStaticFile(const std::string& request, std::string& response, const std::string& filePath) {
    try {
        std::string fullPath = "web" + filePath;

        if (!fileExists(fullPath)) {
            response = createErrorResponse("File not found", 404);
            return;
        }

        std::string content = readFile(fullPath);
        if (content.empty()) {
            response = createErrorResponse("Failed to read file", 500);
            return;
        }

        std::string mimeType = getMimeType(fullPath);
        response = createFileResponse(content, mimeType);

    } catch (const std::exception& e) {
        response = createErrorResponse("File serving error: " + std::string(e.what()), 500);
    }
}

// File serving utilities
std::string APIService::readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string APIService::getMimeType(const std::string& filePath) {
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
    if (extension == "txt") return "text/plain";

    return "application/octet-stream";
}

std::string APIService::createFileResponse(const std::string& content, const std::string& mimeType, int statusCode) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " OK\r\n"
             << "Content-Type: " << mimeType << "\r\n"
             << "Content-Length: " << content.length() << "\r\n"
             << "Cache-Control: public, max-age=3600\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "\r\n"
             << content;
    return response.str();
}

bool APIService::fileExists(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}
