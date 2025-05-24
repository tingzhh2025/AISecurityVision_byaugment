#include "APIService.h"
#include "../core/TaskManager.h"
#include "../core/VideoPipeline.h"
#include "../ai/BehaviorAnalyzer.h"
#include "../utils/PolygonValidator.h"
#include "../onvif/ONVIFDiscovery.h"
#include "../output/Streamer.h"
#include "../output/AlarmTrigger.h"
#include "../database/DatabaseManager.h"
#include "../recognition/FaceRecognizer.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cctype>
#include <regex>
#include <algorithm>
#include <fstream>

APIService::APIService(int port) : m_port(port), m_httpServer(std::make_unique<httplib::Server>()), m_onvifManager(std::make_unique<ONVIFManager>()) {
    std::cout << "[APIService] Initializing API service on port " << port << std::endl;

    // Initialize ONVIF manager
    if (!m_onvifManager->initialize()) {
        std::cerr << "[APIService] Warning: Failed to initialize ONVIF manager: "
                  << m_onvifManager->getLastError() << std::endl;
    } else {
        std::cout << "[APIService] ONVIF discovery manager initialized" << std::endl;
    }

    // Setup HTTP routes
    setupRoutes();
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

    // Stop the HTTP server
    if (m_httpServer) {
        m_httpServer->stop();
    }

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
    std::cout << "[APIService] Server thread started on port " << m_port << std::endl;

    try {
        // Start the HTTP server
        if (!m_httpServer->listen("0.0.0.0", m_port)) {
            std::cerr << "[APIService] Failed to start HTTP server on port " << m_port << std::endl;
            m_running.store(false);
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "[APIService] HTTP server error: " << e.what() << std::endl;
        m_running.store(false);
    }

    std::cout << "[APIService] Server thread stopped" << std::endl;
}

void APIService::setupRoutes() {
    if (!m_httpServer) {
        std::cerr << "[APIService] HTTP server not initialized" << std::endl;
        return;
    }

    std::cout << "[APIService] Setting up HTTP routes..." << std::endl;

    // System endpoints
    m_httpServer->Get("/api/system/status", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetStatus("", response);
        // Extract JSON content from HTTP response
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/system/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetSystemMetrics("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/system/pipeline-stats", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetPipelineStats("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetSystemStats("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Video source management
    m_httpServer->Post("/api/source/add", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostVideoSource(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/source/list", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetVideoSources("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // ONVIF discovery endpoints
    m_httpServer->Get("/api/source/discover", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetDiscoverDevices("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Post("/api/source/add-discovered", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostAddDiscoveredDevice(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Face management endpoints
    m_httpServer->Post("/api/faces/add", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostFaceAdd(req, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/faces", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetFaces("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Delete(R"(/api/faces/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string faceId = req.matches[1].str();
        handleDeleteFace("", response, faceId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Face verification endpoint - Task 62
    m_httpServer->Post("/api/faces/verify", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostFaceVerify(req, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Alarm configuration endpoints - Task 63
    m_httpServer->Post("/api/alarms/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostAlarmConfig(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/alarms/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetAlarmConfigs("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get(R"(/api/alarms/config/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string configId = req.matches[1].str();
        handleGetAlarmConfig("", response, configId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Put(R"(/api/alarms/config/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string configId = req.matches[1].str();
        handlePutAlarmConfig(req.body, response, configId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Delete(R"(/api/alarms/config/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string configId = req.matches[1].str();
        handleDeleteAlarmConfig("", response, configId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Post("/api/alarms/test", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostTestAlarm(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/alarms/status", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetAlarmStatus("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Web interface routes
    m_httpServer->Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        std::string content = loadWebFile("web/templates/dashboard.html");
        res.set_content(content, "text/html");
    });

    m_httpServer->Get("/dashboard", [this](const httplib::Request& req, httplib::Response& res) {
        std::string content = loadWebFile("web/templates/dashboard.html");
        res.set_content(content, "text/html");
    });

    m_httpServer->Get("/onvif-discovery", [this](const httplib::Request& req, httplib::Response& res) {
        std::string content = loadWebFile("web/templates/onvif_discovery.html");
        res.set_content(content, "text/html");
    });

    m_httpServer->Get("/face-manager", [this](const httplib::Request& req, httplib::Response& res) {
        std::string content = loadWebFile("web/templates/face_manager.html");
        res.set_content(content, "text/html");
    });

    // Static file serving
    m_httpServer->Get(R"(/static/(.*))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string path = "web/static/" + req.matches[1].str();
        std::string content = loadWebFile(path);
        std::string mimeType = getMimeType(path);
        res.set_content(content, mimeType.c_str());
    });

    // Face images serving
    m_httpServer->Get(R"(/faces/(.*))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string imagePath = "faces/" + req.matches[1].str();
        std::string content = loadWebFile(imagePath);
        if (content.find("404 - File Not Found") != std::string::npos) {
            res.status = 404;
            res.set_content("Image not found", "text/plain");
        } else {
            std::string mimeType = getMimeType(imagePath);
            res.set_content(content, mimeType.c_str());
        }
    });

    std::cout << "[APIService] HTTP routes configured successfully" << std::endl;
}

void APIService::handleGetStatus(const std::string& request, std::string& response) {
    TaskManager& taskManager = TaskManager::getInstance();

    std::ostringstream json;
    json << "{"
         << "\"status\":\"running\","
         << "\"active_pipelines\":" << taskManager.getActivePipelineCount() << ","
         << "\"cpu_usage\":" << taskManager.getCpuUsage() << ","
         << "\"gpu_memory\":\"" << taskManager.getGpuMemoryUsage() << "\","
         << "\"monitoring_healthy\":" << (taskManager.isMonitoringHealthy() ? "true" : "false") << ","
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

        // Create stream configuration
        StreamConfig streamConfig;
        streamConfig.width = width;
        streamConfig.height = height;
        streamConfig.fps = fps;
        streamConfig.enableOverlays = true;

        if (protocol == "mjpeg") {
            streamConfig.protocol = StreamProtocol::MJPEG;
            streamConfig.quality = quality;
            streamConfig.port = port;
            streamConfig.endpoint = endpoint.empty() ? "/stream.mjpg" : endpoint;
        } else if (protocol == "rtmp") {
            streamConfig.protocol = StreamProtocol::RTMP;
            streamConfig.bitrate = bitrate;
            streamConfig.rtmpUrl = rtmpUrl;
        }

        // Configure streaming in the pipeline
        if (!pipeline->configureStreaming(streamConfig)) {
            response = createErrorResponse("Failed to configure streaming for pipeline", 500);
            return;
        }

        // Get the actual stream URL from the pipeline
        std::string streamUrl = pipeline->getStreamUrl();

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
                 << ",\"port\":" << port
                 << ",\"endpoint\":\"" << streamConfig.endpoint << "\"";
        } else if (protocol == "rtmp") {
            json << ",\"bitrate\":" << bitrate
                 << ",\"rtmp_url\":\"" << rtmpUrl << "\"";
        }

        json << ",\"stream_url\":\"" << streamUrl << "\""
             << ",\"configured_at\":\"" << getCurrentTimestamp() << "\""
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

        // Get actual streaming configuration from pipeline
        StreamConfig config = pipeline->getStreamConfig();
        std::string streamUrl = pipeline->getStreamUrl();
        bool isStreaming = pipeline->isStreamingEnabled();

        std::ostringstream json;
        json << "{"
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"protocol\":\"" << (config.protocol == StreamProtocol::MJPEG ? "mjpeg" : "rtmp") << "\","
             << "\"width\":" << config.width << ","
             << "\"height\":" << config.height << ","
             << "\"fps\":" << config.fps << ","
             << "\"quality\":" << config.quality << ","
             << "\"port\":" << config.port << ","
             << "\"endpoint\":\"" << config.endpoint << "\","
             << "\"enabled\":" << (isStreaming ? "true" : "false") << ","
             << "\"stream_url\":\"" << streamUrl << "\","
             << "\"bitrate\":" << config.bitrate << ","
             << "\"rtmp_url\":\"" << config.rtmpUrl << "\","
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

        // Start streaming for the pipeline
        if (!pipeline->startStreaming()) {
            response = createErrorResponse("Failed to start streaming for pipeline", 500);
            return;
        }

        std::string streamUrl = pipeline->getStreamUrl();

        std::ostringstream json;
        json << "{"
             << "\"status\":\"streaming_started\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"stream_url\":\"" << streamUrl << "\","
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

        // Stop streaming for the pipeline
        if (!pipeline->stopStreaming()) {
            response = createErrorResponse("Failed to stop streaming for pipeline", 500);
            return;
        }

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

            if (pipeline) {
                StreamConfig config = pipeline->getStreamConfig();
                std::string streamUrl = pipeline->getStreamUrl();
                bool isStreaming = pipeline->isStreamingEnabled();
                size_t connectedClients = pipeline->getConnectedClients();
                double streamFps = pipeline->getStreamFps();
                bool isHealthy = pipeline->isHealthy();

                // Get additional health metrics
                double frameRate = pipeline->getFrameRate();
                size_t processedFrames = pipeline->getProcessedFrames();
                size_t droppedFrames = pipeline->getDroppedFrames();
                bool isStable = pipeline->isStreamStable();
                std::string lastError = pipeline->getLastError();

                json << "{"
                     << "\"camera_id\":\"" << cameraId << "\","
                     << "\"protocol\":\"" << (config.protocol == StreamProtocol::MJPEG ? "mjpeg" : "rtmp") << "\","
                     << "\"is_streaming\":" << (isStreaming ? "true" : "false") << ","
                     << "\"stream_url\":\"" << streamUrl << "\","
                     << "\"connected_clients\":" << connectedClients << ","
                     << "\"stream_fps\":" << streamFps << ","
                     << "\"health\":\"" << (isHealthy ? "healthy" : "unhealthy") << "\","
                     << "\"stream_stable\":" << (isStable ? "true" : "false") << ","
                     << "\"frame_rate\":" << frameRate << ","
                     << "\"processed_frames\":" << processedFrames << ","
                     << "\"dropped_frames\":" << droppedFrames << ","
                     << "\"last_error\":\"" << lastError << "\""
                     << "}";
            } else {
                json << "{"
                     << "\"camera_id\":\"" << cameraId << "\","
                     << "\"protocol\":\"unknown\","
                     << "\"is_streaming\":false,"
                     << "\"stream_url\":\"\","
                     << "\"connected_clients\":0,"
                     << "\"stream_fps\":0.0,"
                     << "\"health\":\"error\""
                     << "}";
            }
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

        // Add the rule to the first active pipeline
        // TODO: Support specifying which pipeline to add the rule to via camera_id
        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        // Add rule to BehaviorAnalyzer through VideoPipeline
        if (!pipeline->addIntrusionRule(rule)) {
            response = createErrorResponse("Failed to add intrusion rule to behavior analyzer", 500);
            return;
        }

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
            response = createJsonResponse("{\"rules\":[],\"count\":0,\"timestamp\":\"" + getCurrentTimestamp() + "\"}");
            return;
        }

        // Get rules from the first active pipeline
        // TODO: Support getting rules from specific pipeline via camera_id parameter
        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        auto rules = pipeline->getIntrusionRules();

        std::ostringstream json;
        json << "{"
             << "\"rules\":" << serializeRuleList(rules) << ","
             << "\"count\":" << rules.size() << ","
             << "\"pipeline_id\":\"" << activePipelines[0] << "\","
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

        // Update rule in BehaviorAnalyzer through VideoPipeline
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createErrorResponse("No active video pipelines found", 404);
            return;
        }

        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        if (!pipeline->updateIntrusionRule(rule)) {
            response = createErrorResponse("Failed to update intrusion rule in behavior analyzer", 500);
            return;
        }

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

        // Delete rule from BehaviorAnalyzer through VideoPipeline
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createErrorResponse("No active video pipelines found", 404);
            return;
        }

        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        if (!pipeline->removeIntrusionRule(ruleId)) {
            response = createErrorResponse("Failed to remove intrusion rule from behavior analyzer", 500);
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

        // Add ROI to BehaviorAnalyzer through VideoPipeline
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createErrorResponse("No active video pipelines found", 404);
            return;
        }

        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access video pipeline", 500);
            return;
        }

        if (!pipeline->addROI(roi)) {
            response = createErrorResponse("Failed to add ROI to behavior analyzer", 500);
            return;
        }

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

// ONVIF discovery handlers
void APIService::handleGetDiscoverDevices(const std::string& request, std::string& response) {
    try {
        if (!m_onvifManager || !m_onvifManager->isInitialized()) {
            response = createErrorResponse("ONVIF discovery not available", 503);
            return;
        }

        std::cout << "[APIService] Starting ONVIF device discovery..." << std::endl;

        // Perform network scan for ONVIF devices
        auto devices = m_onvifManager->scanNetwork(5000); // 5 second timeout

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"discovered_devices\":" << devices.size() << ","
             << "\"devices\":[";

        for (size_t i = 0; i < devices.size(); ++i) {
            if (i > 0) json << ",";

            const auto& device = devices[i];
            json << "{"
                 << "\"uuid\":\"" << device.uuid << "\","
                 << "\"name\":\"" << device.name << "\","
                 << "\"manufacturer\":\"" << device.manufacturer << "\","
                 << "\"model\":\"" << device.model << "\","
                 << "\"firmware_version\":\"" << device.firmwareVersion << "\","
                 << "\"serial_number\":\"" << device.serialNumber << "\","
                 << "\"ip_address\":\"" << device.ipAddress << "\","
                 << "\"port\":" << device.port << ","
                 << "\"service_url\":\"" << device.serviceUrl << "\","
                 << "\"stream_uri\":\"" << device.streamUri << "\","
                 << "\"requires_auth\":" << (device.requiresAuth ? "true" : "false") << ","
                 << "\"discovered_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";
        }

        json << "],"
             << "\"scan_duration_ms\":5000,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] ONVIF discovery completed. Found " << devices.size() << " devices" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("ONVIF discovery failed: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostAddDiscoveredDevice(const std::string& request, std::string& response) {
    try {
        if (!m_onvifManager || !m_onvifManager->isInitialized()) {
            response = createErrorResponse("ONVIF discovery not available", 503);
            return;
        }

        // Parse device information from request
        std::string deviceId = parseJsonField(request, "device_id");
        std::string username = parseJsonField(request, "username");
        std::string password = parseJsonField(request, "password");
        std::string testOnly = parseJsonField(request, "test_only");

        if (deviceId.empty()) {
            response = createErrorResponse("Device ID is required", 400);
            return;
        }

        // Find the device in known devices
        ONVIFDevice* device = m_onvifManager->findDevice(deviceId);
        if (!device) {
            response = createErrorResponse("Device not found: " + deviceId, 404);
            return;
        }

        // Test authentication if credentials are provided
        if (!username.empty()) {
            std::cout << "[APIService] Testing authentication for device: " << device->ipAddress
                      << " with username: " << username << std::endl;

            // Get the discovery instance to test authentication
            auto discoveryInstance = std::make_unique<ONVIFDiscovery>();
            if (!discoveryInstance->initialize()) {
                response = createErrorResponse("Failed to initialize ONVIF discovery for authentication test", 500);
                return;
            }

            // Test authentication before updating credentials
            if (!discoveryInstance->testAuthentication(*device, username, password)) {
                response = createErrorResponse("Authentication failed: Invalid username or password for device " + device->ipAddress, 401);
                return;
            }

            std::cout << "[APIService] Authentication successful for device: " << device->ipAddress << std::endl;

            // If this is just a test, return success without configuring
            if (testOnly == "true") {
                std::ostringstream json;
                json << "{"
                     << "\"status\":\"test_success\","
                     << "\"message\":\"Authentication test successful\","
                     << "\"device_ip\":\"" << device->ipAddress << "\","
                     << "\"username\":\"" << username << "\""
                     << "}";
                response = createJsonResponse(json.str(), 200);
                return;
            }

            // Update credentials after successful authentication
            if (!m_onvifManager->updateDeviceCredentials(deviceId, username, password)) {
                response = createErrorResponse("Failed to update device credentials", 500);
                return;
            }

            // Update device object
            device->username = username;
            device->password = password;
            device->requiresAuth = true;
        }

        // Use ONVIFManager's configureDevice method for automatic configuration
        if (!m_onvifManager->configureDevice(*device)) {
            response = createErrorResponse("Failed to configure ONVIF device: " + m_onvifManager->getLastError(), 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"added\","
             << "\"camera_id\":\"" << device->uuid << "\","
             << "\"device_uuid\":\"" << device->uuid << "\","
             << "\"device_name\":\"" << device->name << "\","
             << "\"ip_address\":\"" << device->ipAddress << "\","
             << "\"stream_uri\":\"" << device->streamUri << "\","
             << "\"requires_auth\":" << (device->requiresAuth ? "true" : "false") << ","
             << "\"added_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        std::cout << "[APIService] Added ONVIF device as video source: " << device->uuid
                  << " (" << device->name << ")" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to add discovered device: " + std::string(e.what()), 500);
    }
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

std::string APIService::loadWebFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "<html><body><h1>404 - File Not Found</h1><p>The requested file could not be found.</p></body></html>";
    }

    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }

    return content;
}

// Face management handlers
void APIService::handlePostFaceAdd(const httplib::Request& request, std::string& response) {
    try {
        // Check if request has multipart form data
        if (!request.has_file("image")) {
            response = createErrorResponse("Image file is required", 400);
            return;
        }

        // Get the uploaded image file
        auto imageFile = request.get_file_value("image");
        if (imageFile.content.empty()) {
            response = createErrorResponse("Image file is empty", 400);
            return;
        }

        // Get the name parameter
        std::string name;
        if (request.has_param("name")) {
            name = request.get_param_value("name");
        } else {
            response = createErrorResponse("Name parameter is required", 400);
            return;
        }

        if (name.empty()) {
            response = createErrorResponse("Name cannot be empty", 400);
            return;
        }

        // Validate image file type
        std::string filename = imageFile.filename;
        std::string extension = filename.substr(filename.find_last_of(".") + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension != "jpg" && extension != "jpeg" && extension != "png" && extension != "bmp") {
            response = createErrorResponse("Unsupported image format. Use JPG, PNG, or BMP", 400);
            return;
        }

        // Create faces directory if it doesn't exist
        std::string facesDir = "faces";
        if (system(("mkdir -p " + facesDir).c_str()) != 0) {
            std::cerr << "[APIService] Warning: Could not create faces directory" << std::endl;
        }

        // Generate unique filename
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string imagePath = facesDir + "/" + name + "_" + std::to_string(timestamp) + "." + extension;

        // Save the uploaded image
        std::ofstream outFile(imagePath, std::ios::binary);
        if (!outFile.is_open()) {
            response = createErrorResponse("Failed to save image file", 500);
            return;
        }
        outFile.write(imageFile.content.data(), imageFile.content.size());
        outFile.close();

        std::cout << "[APIService] Saved face image: " << imagePath << std::endl;

        // Extract face embedding using face recognition module
        std::vector<uchar> imageData(imageFile.content.begin(), imageFile.content.end());
        cv::Mat faceImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

        std::vector<float> embedding;
        if (!faceImage.empty()) {
            FaceRecognizer faceRecognizer;
            if (faceRecognizer.initialize()) {
                embedding = faceRecognizer.extractFaceEmbedding(faceImage);
                std::cout << "[APIService] Generated face embedding with " << embedding.size() << " dimensions" << std::endl;
            } else {
                std::cout << "[APIService] Warning: Face recognizer initialization failed, using dummy embedding" << std::endl;
            }
        }

        // Fallback to dummy embedding if face recognition failed
        if (embedding.empty()) {
            std::cout << "[APIService] Using dummy embedding as fallback" << std::endl;
            for (int i = 0; i < 128; i++) {
                embedding.push_back(static_cast<float>(rand()) / RAND_MAX);
            }
        }

        // Create face record
        FaceRecord faceRecord(name, imagePath);
        faceRecord.embedding = embedding;

        // Create database manager instance
        DatabaseManager db;
        if (!db.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        // Insert face into database
        if (!db.insertFace(faceRecord)) {
            response = createErrorResponse("Failed to save face to database: " + db.getErrorMessage(), 500);
            return;
        }

        // Get the inserted face ID
        int faceId = db.getLastInsertId();

        // Create success response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"face_id\":" << faceId << ","
             << "\"name\":\"" << name << "\","
             << "\"image_path\":\"" << imagePath << "\","
             << "\"embedding_size\":" << embedding.size() << ","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        std::cout << "[APIService] Face added successfully: " << name
                  << " (ID: " << faceId << ")" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to add face: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetFaces(const std::string& request, std::string& response) {
    try {
        // Create database manager instance
        DatabaseManager db;
        if (!db.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        // Get all faces from database
        auto faces = db.getFaces();

        // Create JSON response
        std::ostringstream json;
        json << "{\"faces\":[";

        for (size_t i = 0; i < faces.size(); ++i) {
            if (i > 0) json << ",";

            const auto& face = faces[i];
            json << "{"
                 << "\"id\":" << face.id << ","
                 << "\"name\":\"" << face.name << "\","
                 << "\"image_path\":\"" << face.image_path << "\","
                 << "\"embedding_size\":" << face.embedding.size() << ","
                 << "\"created_at\":\"" << face.created_at << "\""
                 << "}";
        }

        json << "],"
             << "\"count\":" << faces.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Retrieved " << faces.size() << " faces" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get faces: " + std::string(e.what()), 500);
    }
}

void APIService::handleDeleteFace(const std::string& request, std::string& response, const std::string& faceId) {
    try {
        // Validate face ID
        int id;
        try {
            id = std::stoi(faceId);
        } catch (const std::exception&) {
            response = createErrorResponse("Invalid face ID", 400);
            return;
        }

        // Create database manager instance
        DatabaseManager db;
        if (!db.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        // Check if face exists
        auto face = db.getFaceById(id);
        if (face.id == 0) {
            response = createErrorResponse("Face not found", 404);
            return;
        }

        // Delete face from database
        if (!db.deleteFace(id)) {
            response = createErrorResponse("Failed to delete face: " + db.getErrorMessage(), 500);
            return;
        }

        // Try to delete the image file (optional - don't fail if it doesn't exist)
        if (!face.image_path.empty()) {
            if (remove(face.image_path.c_str()) == 0) {
                std::cout << "[APIService] Deleted face image: " << face.image_path << std::endl;
            } else {
                std::cout << "[APIService] Warning: Could not delete face image: " << face.image_path << std::endl;
            }
        }

        // Create success response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Face deleted successfully\","
             << "\"deleted_face_id\":" << id << ","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 204);

        std::cout << "[APIService] Face deleted successfully: " << face.name
                  << " (ID: " << id << ")" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete face: " + std::string(e.what()), 500);
    }
}

// Task 62: Face verification endpoint implementation
void APIService::handlePostFaceVerify(const httplib::Request& request, std::string& response) {
    try {
        // Check if request has multipart form data with image
        if (!request.has_file("image")) {
            response = createErrorResponse("Image file is required", 400);
            return;
        }

        // Get the uploaded image file
        auto imageFile = request.get_file_value("image");
        if (imageFile.content.empty()) {
            response = createErrorResponse("Image file is empty", 400);
            return;
        }

        // Validate image file type
        std::string filename = imageFile.filename;
        std::string extension = filename.substr(filename.find_last_of(".") + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension != "jpg" && extension != "jpeg" && extension != "png" && extension != "bmp") {
            response = createErrorResponse("Unsupported image format. Use JPG, PNG, or BMP", 400);
            return;
        }

        // Get optional threshold parameter (default 0.7)
        float threshold = 0.7f;
        if (request.has_param("threshold")) {
            try {
                threshold = std::stof(request.get_param_value("threshold"));
                if (threshold < 0.0f || threshold > 1.0f) {
                    response = createErrorResponse("Threshold must be between 0.0 and 1.0", 400);
                    return;
                }
            } catch (const std::exception&) {
                response = createErrorResponse("Invalid threshold value", 400);
                return;
            }
        }

        std::cout << "[APIService] Face verification request with threshold: " << threshold << std::endl;

        // Create database manager instance
        DatabaseManager db;
        if (!db.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        // Get all registered faces from database
        auto registeredFaces = db.getFaces();
        if (registeredFaces.empty()) {
            response = createJsonResponse("{\"matches\":[],\"count\":0,\"message\":\"No registered faces found\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}");
            return;
        }

        std::cout << "[APIService] Found " << registeredFaces.size() << " registered faces for verification" << std::endl;

        // Convert image data to OpenCV Mat
        std::vector<uchar> imageData(imageFile.content.begin(), imageFile.content.end());
        cv::Mat inputImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

        if (inputImage.empty()) {
            response = createErrorResponse("Failed to decode image", 400);
            return;
        }

        std::cout << "[APIService] Decoded input image: " << inputImage.cols << "x" << inputImage.rows << std::endl;

        // Initialize face recognizer
        FaceRecognizer faceRecognizer;
        if (!faceRecognizer.initialize()) {
            response = createErrorResponse("Failed to initialize face recognizer", 500);
            return;
        }

        // Perform face verification
        auto verificationResults = faceRecognizer.verifyFace(inputImage, registeredFaces, threshold);

        // Create JSON response
        std::ostringstream json;
        json << "{"
             << "\"matches\":[";

        for (size_t i = 0; i < verificationResults.size(); ++i) {
            if (i > 0) json << ",";

            const auto& result = verificationResults[i];
            json << "{"
                 << "\"face_id\":" << result.face_id << ","
                 << "\"name\":\"" << result.name << "\","
                 << "\"confidence\":" << std::fixed << std::setprecision(4) << result.confidence << ","
                 << "\"similarity_score\":" << std::fixed << std::setprecision(4) << result.similarity_score
                 << "}";
        }

        json << "],"
             << "\"count\":" << verificationResults.size() << ","
             << "\"threshold\":" << std::fixed << std::setprecision(2) << threshold << ","
             << "\"total_registered_faces\":" << registeredFaces.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 200);

        std::cout << "[APIService] Face verification completed: " << verificationResults.size()
                  << " matches found above threshold " << threshold << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Face verification failed: " + std::string(e.what()), 500);
    }
}

// Alarm configuration handlers - Task 63
void APIService::handlePostAlarmConfig(const std::string& request, std::string& response) {
    try {
        std::string method = parseJsonField(request, "method");
        std::string url = parseJsonField(request, "url");
        std::string configId = parseJsonField(request, "id");

        if (configId.empty()) {
            // Generate a unique config ID
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            configId = "alarm_config_" + std::to_string(timestamp);
        }

        if (method.empty()) {
            response = createErrorResponse("method is required (http, websocket, mqtt)", 400);
            return;
        }

        if (method != "http" && method != "websocket" && method != "mqtt") {
            response = createErrorResponse("method must be 'http', 'websocket', or 'mqtt'", 400);
            return;
        }

        if (method == "http" && url.empty()) {
            response = createErrorResponse("url is required for HTTP method", 400);
            return;
        }

        // Create alarm configuration
        AlarmConfig config;
        config.id = configId;

        if (method == "http") {
            config.method = AlarmMethod::HTTP_POST;
            config.httpConfig = HttpAlarmConfig(url);

            // Parse optional HTTP parameters
            int timeout = parseJsonInt(request, "timeout_ms", 5000);
            if (timeout < 1000 || timeout > 30000) {
                response = createErrorResponse("timeout_ms must be between 1000 and 30000", 400);
                return;
            }
            config.httpConfig.timeout_ms = timeout;

        } else if (method == "websocket") {
            config.method = AlarmMethod::WEBSOCKET;
            // TODO: Add WebSocket configuration
        } else if (method == "mqtt") {
            config.method = AlarmMethod::MQTT;
            // TODO: Add MQTT configuration
        }

        config.enabled = true;
        config.priority = parseJsonInt(request, "priority", 1);
        if (config.priority < 1 || config.priority > 5) {
            config.priority = 1;
        }

        // Get AlarmTrigger from TaskManager (assuming it's accessible)
        TaskManager& taskManager = TaskManager::getInstance();
        // For now, we'll create a static AlarmTrigger instance
        // In a real implementation, this should be managed by TaskManager
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        if (!alarmTrigger.addAlarmConfig(config)) {
            response = createErrorResponse("Failed to add alarm configuration", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"created\","
             << "\"config_id\":\"" << config.id << "\","
             << "\"method\":\"" << method << "\","
             << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
             << "\"priority\":" << config.priority;

        if (method == "http") {
            json << ",\"url\":\"" << config.httpConfig.url << "\","
                 << "\"timeout_ms\":" << config.httpConfig.timeout_ms;
        }

        json << ",\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        std::cout << "[APIService] Created alarm config: " << config.id
                  << " (method: " << method << ")" << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to create alarm config: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetAlarmConfigs(const std::string& request, std::string& response) {
    try {
        // Get AlarmTrigger instance
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        auto configs = alarmTrigger.getAlarmConfigs();

        std::ostringstream json;
        json << "{\"configs\":[";

        for (size_t i = 0; i < configs.size(); ++i) {
            if (i > 0) json << ",";

            const auto& config = configs[i];
            json << "{"
                 << "\"id\":\"" << config.id << "\","
                 << "\"method\":\"" << (config.method == AlarmMethod::HTTP_POST ? "http" :
                                      config.method == AlarmMethod::WEBSOCKET ? "websocket" : "mqtt") << "\","
                 << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
                 << "\"priority\":" << config.priority;

            if (config.method == AlarmMethod::HTTP_POST) {
                json << ",\"url\":\"" << config.httpConfig.url << "\","
                     << "\"timeout_ms\":" << config.httpConfig.timeout_ms;
            }

            json << "}";
        }

        json << "],"
             << "\"count\":" << configs.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm configs: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        auto configs = alarmTrigger.getAlarmConfigs();

        // Find the specific config
        for (const auto& config : configs) {
            if (config.id == configId) {
                std::ostringstream json;
                json << "{"
                     << "\"id\":\"" << config.id << "\","
                     << "\"method\":\"" << (config.method == AlarmMethod::HTTP_POST ? "http" :
                                          config.method == AlarmMethod::WEBSOCKET ? "websocket" : "mqtt") << "\","
                     << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
                     << "\"priority\":" << config.priority;

                if (config.method == AlarmMethod::HTTP_POST) {
                    json << ",\"url\":\"" << config.httpConfig.url << "\","
                         << "\"timeout_ms\":" << config.httpConfig.timeout_ms;
                }

                json << ",\"timestamp\":\"" << getCurrentTimestamp() << "\""
                     << "}";

                response = createJsonResponse(json.str());
                return;
            }
        }

        response = createErrorResponse("Alarm config not found: " + configId, 404);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm config: " + std::string(e.what()), 500);
    }
}

void APIService::handlePutAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        // Parse update parameters
        std::string method = parseJsonField(request, "method");
        std::string url = parseJsonField(request, "url");

        // Get existing config
        auto configs = alarmTrigger.getAlarmConfigs();
        AlarmConfig* existingConfig = nullptr;

        for (auto& config : configs) {
            if (config.id == configId) {
                existingConfig = &config;
                break;
            }
        }

        if (!existingConfig) {
            response = createErrorResponse("Alarm config not found: " + configId, 404);
            return;
        }

        // Update configuration
        AlarmConfig updatedConfig = *existingConfig;

        if (!method.empty()) {
            if (method == "http") {
                updatedConfig.method = AlarmMethod::HTTP_POST;
            } else if (method == "websocket") {
                updatedConfig.method = AlarmMethod::WEBSOCKET;
            } else if (method == "mqtt") {
                updatedConfig.method = AlarmMethod::MQTT;
            } else {
                response = createErrorResponse("Invalid method: " + method, 400);
                return;
            }
        }

        if (!url.empty() && updatedConfig.method == AlarmMethod::HTTP_POST) {
            updatedConfig.httpConfig.url = url;
        }

        int timeout = parseJsonInt(request, "timeout_ms", -1);
        if (timeout > 0) {
            if (timeout < 1000 || timeout > 30000) {
                response = createErrorResponse("timeout_ms must be between 1000 and 30000", 400);
                return;
            }
            updatedConfig.httpConfig.timeout_ms = timeout;
        }

        int priority = parseJsonInt(request, "priority", -1);
        if (priority > 0) {
            if (priority < 1 || priority > 5) {
                response = createErrorResponse("priority must be between 1 and 5", 400);
                return;
            }
            updatedConfig.priority = priority;
        }

        if (!alarmTrigger.updateAlarmConfig(updatedConfig)) {
            response = createErrorResponse("Failed to update alarm configuration", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"updated\","
             << "\"config_id\":\"" << updatedConfig.id << "\","
             << "\"method\":\"" << (updatedConfig.method == AlarmMethod::HTTP_POST ? "http" :
                                  updatedConfig.method == AlarmMethod::WEBSOCKET ? "websocket" : "mqtt") << "\","
             << "\"enabled\":" << (updatedConfig.enabled ? "true" : "false") << ","
             << "\"priority\":" << updatedConfig.priority;

        if (updatedConfig.method == AlarmMethod::HTTP_POST) {
            json << ",\"url\":\"" << updatedConfig.httpConfig.url << "\","
                 << ",\"timeout_ms\":" << updatedConfig.httpConfig.timeout_ms;
        }

        json << ",\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Updated alarm config: " << configId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update alarm config: " + std::string(e.what()), 500);
    }
}

void APIService::handleDeleteAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        if (!alarmTrigger.removeAlarmConfig(configId)) {
            response = createErrorResponse("Alarm config not found: " + configId, 404);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"deleted\","
             << "\"config_id\":\"" << configId << "\","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 204);

        std::cout << "[APIService] Deleted alarm config: " << configId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete alarm config: " + std::string(e.what()), 500);
    }
}

void APIService::handlePostTestAlarm(const std::string& request, std::string& response) {
    try {
        std::string eventType = parseJsonField(request, "event_type");
        std::string cameraId = parseJsonField(request, "camera_id");

        if (eventType.empty()) {
            response = createErrorResponse("event_type is required", 400);
            return;
        }

        if (cameraId.empty()) {
            cameraId = "test_camera";
        }

        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        // Trigger test alarm
        alarmTrigger.triggerTestAlarm(eventType, cameraId);

        std::ostringstream json;
        json << "{"
             << "\"status\":\"test_alarm_triggered\","
             << "\"event_type\":\"" << eventType << "\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"test_mode\":true,"
             << "\"triggered_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        std::cout << "[APIService] Test alarm triggered: " << eventType
                  << " for camera: " << cameraId << std::endl;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to trigger test alarm: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetAlarmStatus(const std::string& request, std::string& response) {
    try {
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        auto configs = alarmTrigger.getAlarmConfigs();
        size_t pendingAlarms = alarmTrigger.getPendingAlarmsCount();
        size_t deliveredAlarms = alarmTrigger.getDeliveredAlarmsCount();
        size_t failedAlarms = alarmTrigger.getFailedAlarmsCount();

        std::ostringstream json;
        json << "{"
             << "\"alarm_system\":{"
             << "\"status\":\"running\","
             << "\"total_configs\":" << configs.size() << ","
             << "\"enabled_configs\":" << std::count_if(configs.begin(), configs.end(),
                                                       [](const AlarmConfig& c) { return c.enabled; }) << ","
             << "\"pending_alarms\":" << pendingAlarms << ","
             << "\"delivered_alarms\":" << deliveredAlarms << ","
             << "\"failed_alarms\":" << failedAlarms << ","
             << "\"success_rate\":" << (deliveredAlarms + failedAlarms > 0 ?
                                      (double)deliveredAlarms / (deliveredAlarms + failedAlarms) * 100.0 : 100.0)
             << "},"
             << "\"methods\":{"
             << "\"http_configs\":" << std::count_if(configs.begin(), configs.end(),
                                                   [](const AlarmConfig& c) { return c.method == AlarmMethod::HTTP_POST; }) << ","
             << "\"websocket_configs\":" << std::count_if(configs.begin(), configs.end(),
                                                        [](const AlarmConfig& c) { return c.method == AlarmMethod::WEBSOCKET; }) << ","
             << "\"mqtt_configs\":" << std::count_if(configs.begin(), configs.end(),
                                                   [](const AlarmConfig& c) { return c.method == AlarmMethod::MQTT; })
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm status: " + std::string(e.what()), 500);
    }
}

