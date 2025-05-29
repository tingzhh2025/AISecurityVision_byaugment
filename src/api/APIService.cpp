#include "APIService.h"
#include "../core/TaskManager.h"
#include "../core/VideoPipeline.h"
#include "../video/FFmpegDecoder.h"
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

#include "../core/Logger.h"
using namespace AISecurityVision;
APIService::APIService(int port) : m_port(port), m_httpServer(std::make_unique<httplib::Server>()), m_onvifManager(std::make_unique<ONVIFManager>()) {
    LOG_INFO() << "[APIService] Initializing API service on port " << port;

    // Initialize ONVIF manager
    if (!m_onvifManager->initialize()) {
        LOG_ERROR() << "[APIService] Warning: Failed to initialize ONVIF manager: "
                  << m_onvifManager->getLastError();
    } else {
        LOG_INFO() << "[APIService] ONVIF discovery manager initialized";
    }

    // Setup HTTP routes
    setupRoutes();
}

APIService::~APIService() {
    stop();
}

bool APIService::start() {
    if (m_running.load()) {
        LOG_INFO() << "[APIService] Service already running";
        return true;
    }

    try {
        m_running.store(true);
        m_serverThread = std::thread(&APIService::serverThread, this);

        // Give the server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        LOG_INFO() << "[APIService] API service started on port " << m_port;
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[APIService] Failed to start: " << e.what();
        m_running.store(false);
        return false;
    }
}

void APIService::stop() {
    if (!m_running.load()) {
        return;
    }

    LOG_INFO() << "[APIService] Stopping API service...";

    m_running.store(false);

    // Stop the HTTP server
    if (m_httpServer) {
        m_httpServer->stop();
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    LOG_INFO() << "[APIService] API service stopped";
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
    LOG_INFO() << "[APIService] Server thread started on port " << m_port;

    try {
        // Start the HTTP server
        if (!m_httpServer->listen("0.0.0.0", m_port)) {
            LOG_ERROR() << "[APIService] Failed to start HTTP server on port " << m_port;
            m_running.store(false);
            return;
        }
    } catch (const std::exception& e) {
        LOG_ERROR() << "[APIService] HTTP server error: " << e.what();
        m_running.store(false);
    }

    LOG_INFO() << "[APIService] Server thread stopped";
}

void APIService::setupRoutes() {
    if (!m_httpServer) {
        LOG_ERROR() << "[APIService] HTTP server not initialized";
        return;
    }

    LOG_INFO() << "[APIService] Setting up HTTP routes...";

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

    // Frontend compatibility endpoints
    m_httpServer->Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetAlerts("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/system/info", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetSystemInfo("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/cameras", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetCameras("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Post("/api/cameras", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostVideoSource(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Post("/api/cameras/test-connection", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleTestCameraConnection(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/recordings", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetRecordings("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/detection/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetDetectionConfig("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // ROI management endpoints - Task 68
    m_httpServer->Post("/api/rois", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostROIs(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/rois", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string cameraId = req.get_param_value("camera_id");
        handleGetROIs(cameraId, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get(R"(/api/rois/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string roiId = req.matches[1].str();
        handleGetROI("", response, roiId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Put(R"(/api/rois/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string roiId = req.matches[1].str();
        handlePutROI(req.body, response, roiId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Delete(R"(/api/rois/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        std::string roiId = req.matches[1].str();
        handleDeleteROI("", response, roiId);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // Bulk ROI operations endpoint - Task 72
    m_httpServer->Post("/api/rois/bulk", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostBulkROIs(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    // ReID configuration endpoints - Task 76
    m_httpServer->Post("/api/reid/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePostReIDConfig(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/reid/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetReIDConfig("", response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Put("/api/reid/threshold", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handlePutReIDThreshold(req.body, response);
        size_t contentStart = response.find("\r\n\r\n");
        if (contentStart != std::string::npos) {
            response = response.substr(contentStart + 4);
        }
        res.set_content(response, "application/json");
    });

    m_httpServer->Get("/api/reid/status", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        handleGetReIDStatus("", response);
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

    // Video stream proxy endpoint
    m_httpServer->Get(R"(/stream/camera/(.*))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string cameraId = req.matches[1].str();
        handleStreamProxy(cameraId, req, res);
    });

    LOG_INFO() << "[APIService] HTTP routes configured successfully";
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
    try {
        // Parse JSON request to extract video source parameters
        std::string id = parseJsonField(request, "id");
        std::string name = parseJsonField(request, "name");
        std::string url = parseJsonField(request, "url");
        std::string protocol = parseJsonField(request, "protocol");
        int width = parseJsonInt(request, "width", 1920);
        int height = parseJsonInt(request, "height", 1080);
        int fps = parseJsonInt(request, "fps", 25);
        bool enabled = parseJsonBool(request, "enabled", true);

        // Validate required fields
        if (id.empty()) {
            response = createErrorResponse("id is required", 400);
            return;
        }
        if (url.empty()) {
            response = createErrorResponse("url is required", 400);
            return;
        }
        if (protocol.empty()) {
            response = createErrorResponse("protocol is required", 400);
            return;
        }

        // Validate protocol
        if (protocol != "rtsp" && protocol != "rtmp" && protocol != "http" && protocol != "file") {
            response = createErrorResponse("Invalid protocol. Supported: rtsp, rtmp, http, file", 400);
            return;
        }

        // Create VideoSource object
        VideoSource source;
        source.id = id;
        source.name = name.empty() ? id : name;
        source.url = url;
        source.protocol = protocol;
        source.width = width;
        source.height = height;
        source.fps = fps;
        source.enabled = enabled;

        // Add to TaskManager
        TaskManager& taskManager = TaskManager::getInstance();
        if (taskManager.addVideoSource(source)) {
            std::ostringstream json;
            json << "{"
                 << "\"status\":\"added\","
                 << "\"id\":\"" << id << "\","
                 << "\"name\":\"" << source.name << "\","
                 << "\"url\":\"" << url << "\","
                 << "\"protocol\":\"" << protocol << "\","
                 << "\"width\":" << width << ","
                 << "\"height\":" << height << ","
                 << "\"fps\":" << fps << ","
                 << "\"enabled\":" << (enabled ? "true" : "false") << ","
                 << "\"added_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";

            response = createJsonResponse(json.str(), 201);
            LOG_INFO() << "[APIService] Added video source: " << id << " (" << protocol << ")";
        } else {
            response = createErrorResponse("Failed to add video source. Check if ID already exists or maximum limit reached.", 409);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to parse video source request: " + std::string(e.what()), 400);
    }
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

        LOG_INFO() << "[APIService] Manual recording started for camera: " << cameraId
                  << ", duration: " << duration << "s";

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

        LOG_INFO() << "[APIService] Manual recording stopped for camera: " << cameraId;

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

        LOG_INFO() << "[APIService] Recording configuration updated: pre=" << preEventDuration
                  << "s, post=" << postEventDuration << "s";

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

float APIService::parseJsonFloat(const std::string& json, const std::string& field, float defaultValue) {
    // Simple JSON float field extraction
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

    // Find end of number (including decimal point)
    size_t end = start;
    while (end < json.length() && (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-')) {
        end++;
    }

    if (end == start) {
        return defaultValue;
    }

    try {
        return std::stof(json.substr(start, end - start));
    } catch (const std::exception&) {
        return defaultValue;
    }
}

bool APIService::parseJsonBool(const std::string& json, const std::string& field, bool defaultValue) {
    // Simple JSON boolean field extraction
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

    // Check for true/false
    if (json.substr(start, 4) == "true") {
        return true;
    } else if (json.substr(start, 5) == "false") {
        return false;
    }

    return defaultValue;
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

        LOG_INFO() << "[APIService] Configured " << protocol << " streaming for camera: " << cameraId;

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

        LOG_INFO() << "[APIService] Started streaming for camera: " << cameraId;

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

        LOG_INFO() << "[APIService] Stopped streaming for camera: " << cameraId;

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

        LOG_INFO() << "[APIService] Created intrusion rule: " << rule.id
                  << " with ROI: " << rule.roi.id;

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

        LOG_INFO() << "[APIService] Updated intrusion rule: " << rule.id;

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

        LOG_INFO() << "[APIService] Deleted intrusion rule: " << ruleId;

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

        // Parse camera_id from request
        std::string cameraId = parseJsonField(request, "camera_id");
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
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

        // Validate priority level (1-5 scale) - Task 69
        if (roi.priority < 1 || roi.priority > 5) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Priority must be between 1 and 5 (inclusive)\","
                     << "\"error_code\":\"INVALID_PRIORITY\","
                     << "\"provided_priority\":" << roi.priority << ","
                     << "\"valid_range\":\"1-5\","
                     << "\"description\":\"Priority levels: 1=Low, 2=Low-Medium, 3=Medium, 4=High, 5=Critical\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // Validate time format if provided - Task 70
        if (!BehaviorAnalyzer::isValidTimeFormat(roi.start_time)) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Invalid start_time format\","
                     << "\"error_code\":\"INVALID_TIME_FORMAT\","
                     << "\"provided_time\":\"" << roi.start_time << "\","
                     << "\"valid_formats\":[\"HH:MM\", \"HH:MM:SS\"],"
                     << "\"examples\":[\"09:00\", \"09:00:00\", \"17:30\", \"17:30:45\"],"
                     << "\"description\":\"Time must be in ISO 8601 format (HH:MM or HH:MM:SS)\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        if (!BehaviorAnalyzer::isValidTimeFormat(roi.end_time)) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Invalid end_time format\","
                     << "\"error_code\":\"INVALID_TIME_FORMAT\","
                     << "\"provided_time\":\"" << roi.end_time << "\","
                     << "\"valid_formats\":[\"HH:MM\", \"HH:MM:SS\"],"
                     << "\"examples\":[\"09:00\", \"09:00:00\", \"17:30\", \"17:30:45\"],"
                     << "\"description\":\"Time must be in ISO 8601 format (HH:MM or HH:MM:SS)\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
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

        // Convert polygon to JSON string for database storage
        std::ostringstream polygonJson;
        polygonJson << "[";
        for (size_t i = 0; i < roi.polygon.size(); ++i) {
            if (i > 0) polygonJson << ",";
            polygonJson << "{\"x\":" << roi.polygon[i].x << ",\"y\":" << roi.polygon[i].y << "}";
        }
        polygonJson << "]";

        // Create ROI record for database
        ROIRecord roiRecord(roi.id, cameraId, roi.name, polygonJson.str());
        roiRecord.enabled = roi.enabled;
        roiRecord.priority = roi.priority;
        roiRecord.start_time = roi.start_time;
        roiRecord.end_time = roi.end_time;

        // Store in database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        if (!dbManager.insertROI(roiRecord)) {
            response = createErrorResponse("Failed to store ROI in database: " + dbManager.getErrorMessage(), 500);
            return;
        }

        // Also add ROI to BehaviorAnalyzer through VideoPipeline if camera is active
        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);
        if (pipeline) {
            pipeline->addROI(roi);
            LOG_INFO() << "[APIService] Added ROI to active pipeline: " << cameraId;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"created\","
             << "\"roi_id\":\"" << roi.id << "\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"name\":\"" << roi.name << "\","
             << "\"polygon_points\":" << roi.polygon.size() << ","
             << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
             << "\"priority\":" << roi.priority;

        // Add time fields if they exist
        if (!roi.start_time.empty()) {
            json << ",\"start_time\":\"" << roi.start_time << "\"";
        }
        if (!roi.end_time.empty()) {
            json << ",\"end_time\":\"" << roi.end_time << "\"";
        }

        json << ",\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        LOG_INFO() << "[APIService] Created ROI: " << roi.id << " (" << roi.name << ") for camera: " << cameraId;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to create ROI: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetROIs(const std::string& cameraId, std::string& response) {
    try {
        // Get ROIs from database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        auto rois = dbManager.getROIs(cameraId);

        std::ostringstream json;
        json << "{"
             << "\"rois\":[";

        for (size_t i = 0; i < rois.size(); ++i) {
            if (i > 0) json << ",";

            const auto& roi = rois[i];
            json << "{"
                 << "\"id\":\"" << roi.roi_id << "\","
                 << "\"camera_id\":\"" << roi.camera_id << "\","
                 << "\"name\":\"" << roi.name << "\","
                 << "\"polygon\":" << roi.polygon_data << ","
                 << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
                 << "\"priority\":" << roi.priority;

            // Add time fields if they exist
            if (!roi.start_time.empty()) {
                json << ",\"start_time\":\"" << roi.start_time << "\"";
            }
            if (!roi.end_time.empty()) {
                json << ",\"end_time\":\"" << roi.end_time << "\"";
            }

            json << ",\"created_at\":\"" << roi.created_at << "\","
                 << "\"updated_at\":\"" << roi.updated_at << "\""
                 << "}";
        }

        json << "],"
             << "\"count\":" << rois.size() << ","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] Retrieved " << rois.size() << " ROIs"
                  << (cameraId.empty() ? "" : " for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get ROIs: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetROI(const std::string& request, std::string& response, const std::string& roiId) {
    try {
        if (roiId.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        // Get ROI from database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        auto roi = dbManager.getROIById(roiId);
        if (roi.roi_id.empty()) {
            response = createErrorResponse("ROI not found: " + roiId, 404);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"id\":\"" << roi.roi_id << "\","
             << "\"camera_id\":\"" << roi.camera_id << "\","
             << "\"name\":\"" << roi.name << "\","
             << "\"polygon\":" << roi.polygon_data << ","
             << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
             << "\"priority\":" << roi.priority;

        // Add time fields if they exist
        if (!roi.start_time.empty()) {
            json << ",\"start_time\":\"" << roi.start_time << "\"";
        }
        if (!roi.end_time.empty()) {
            json << ",\"end_time\":\"" << roi.end_time << "\"";
        }

        json << ",\"created_at\":\"" << roi.created_at << "\","
             << "\"updated_at\":\"" << roi.updated_at << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] Retrieved ROI: " << roiId;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get ROI: " + std::string(e.what()), 500);
    }
}

void APIService::handlePutROI(const std::string& request, std::string& response, const std::string& roiId) {
    try {
        if (roiId.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        // Parse the JSON request to update the ROI
        ROI roi;
        if (!deserializeROI(request, roi)) {
            response = createErrorResponse("Invalid ROI format", 400);
            return;
        }

        // Parse camera_id from request
        std::string cameraId = parseJsonField(request, "camera_id");
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        // Ensure the ROI ID matches the URL parameter
        roi.id = roiId;

        // Validate the ROI
        if (roi.name.empty()) {
            response = createErrorResponse("ROI name is required", 400);
            return;
        }

        // Validate priority level (1-5 scale) - Task 69
        if (roi.priority < 1 || roi.priority > 5) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Priority must be between 1 and 5 (inclusive)\","
                     << "\"error_code\":\"INVALID_PRIORITY\","
                     << "\"provided_priority\":" << roi.priority << ","
                     << "\"valid_range\":\"1-5\","
                     << "\"description\":\"Priority levels: 1=Low, 2=Low-Medium, 3=Medium, 4=High, 5=Critical\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // Validate time format if provided - Task 70
        if (!BehaviorAnalyzer::isValidTimeFormat(roi.start_time)) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Invalid start_time format\","
                     << "\"error_code\":\"INVALID_TIME_FORMAT\","
                     << "\"provided_time\":\"" << roi.start_time << "\","
                     << "\"valid_formats\":[\"HH:MM\", \"HH:MM:SS\"],"
                     << "\"examples\":[\"09:00\", \"09:00:00\", \"17:30\", \"17:30:45\"],"
                     << "\"description\":\"Time must be in ISO 8601 format (HH:MM or HH:MM:SS)\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        if (!BehaviorAnalyzer::isValidTimeFormat(roi.end_time)) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Invalid end_time format\","
                     << "\"error_code\":\"INVALID_TIME_FORMAT\","
                     << "\"provided_time\":\"" << roi.end_time << "\","
                     << "\"valid_formats\":[\"HH:MM\", \"HH:MM:SS\"],"
                     << "\"examples\":[\"09:00\", \"09:00:00\", \"17:30\", \"17:30:45\"],"
                     << "\"description\":\"Time must be in ISO 8601 format (HH:MM or HH:MM:SS)\""
                     << "}";
            response = createJsonResponse(errorJson.str(), 400);
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

        // Get existing ROI from database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        auto existingROI = dbManager.getROIById(roiId);
        if (existingROI.roi_id.empty()) {
            response = createErrorResponse("ROI not found: " + roiId, 404);
            return;
        }

        // Convert polygon to JSON string for database storage
        std::ostringstream polygonJson;
        polygonJson << "[";
        for (size_t i = 0; i < roi.polygon.size(); ++i) {
            if (i > 0) polygonJson << ",";
            polygonJson << "{\"x\":" << roi.polygon[i].x << ",\"y\":" << roi.polygon[i].y << "}";
        }
        polygonJson << "]";

        // Update ROI record
        ROIRecord roiRecord = existingROI;
        roiRecord.camera_id = cameraId;
        roiRecord.name = roi.name;
        roiRecord.polygon_data = polygonJson.str();
        roiRecord.enabled = roi.enabled;
        roiRecord.priority = roi.priority;
        roiRecord.start_time = roi.start_time;
        roiRecord.end_time = roi.end_time;
        roiRecord.updated_at = getCurrentTimestamp();

        if (!dbManager.updateROI(roiRecord)) {
            response = createErrorResponse("Failed to update ROI in database: " + dbManager.getErrorMessage(), 500);
            return;
        }

        // Update ROI in BehaviorAnalyzer through VideoPipeline if camera is active
        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);
        if (pipeline) {
            // Remove old ROI and add updated one
            pipeline->removeROI(roiId);
            pipeline->addROI(roi);
            LOG_INFO() << "[APIService] Updated ROI in active pipeline: " << cameraId;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"updated\","
             << "\"roi_id\":\"" << roi.id << "\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"name\":\"" << roi.name << "\","
             << "\"polygon_points\":" << roi.polygon.size() << ","
             << "\"enabled\":" << (roi.enabled ? "true" : "false") << ","
             << "\"priority\":" << roi.priority;

        // Add time fields if they exist
        if (!roi.start_time.empty()) {
            json << ",\"start_time\":\"" << roi.start_time << "\"";
        }
        if (!roi.end_time.empty()) {
            json << ",\"end_time\":\"" << roi.end_time << "\"";
        }

        json << ",\"updated_at\":\"" << roiRecord.updated_at << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] Updated ROI: " << roiId << " (" << roi.name << ") for camera: " << cameraId;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update ROI: " + std::string(e.what()), 500);
    }
}

void APIService::handleDeleteROI(const std::string& request, std::string& response, const std::string& roiId) {
    try {
        if (roiId.empty()) {
            response = createErrorResponse("ROI ID is required", 400);
            return;
        }

        // Get existing ROI from database
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        auto existingROI = dbManager.getROIById(roiId);
        if (existingROI.roi_id.empty()) {
            response = createErrorResponse("ROI not found: " + roiId, 404);
            return;
        }

        // Delete from database
        if (!dbManager.deleteROI(roiId)) {
            response = createErrorResponse("Failed to delete ROI from database: " + dbManager.getErrorMessage(), 500);
            return;
        }

        // Remove ROI from BehaviorAnalyzer through VideoPipeline if camera is active
        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(existingROI.camera_id);
        if (pipeline) {
            pipeline->removeROI(roiId);
            LOG_INFO() << "[APIService] Removed ROI from active pipeline: " << existingROI.camera_id;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"deleted\","
             << "\"roi_id\":\"" << roiId << "\","
             << "\"camera_id\":\"" << existingROI.camera_id << "\","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] Deleted ROI: " << roiId << " from camera: " << existingROI.camera_id;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete ROI: " + std::string(e.what()), 500);
    }
}

// Bulk ROI operations handler - Task 72
void APIService::handlePostBulkROIs(const std::string& request, std::string& response) {
    try {
        // Parse the JSON request to extract the operations array
        std::regex operationsRegex(R"("operations"\s*:\s*\[(.*)\])");
        std::smatch operationsMatch;

        if (!std::regex_search(request, operationsMatch, operationsRegex)) {
            response = createErrorResponse("operations array is required", 400);
            return;
        }

        std::string operationsStr = operationsMatch[1].str();

        // Parse individual operations
        std::vector<std::string> operations;
        std::regex operationRegex(R"(\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\})");
        std::sregex_iterator operationIter(operationsStr.begin(), operationsStr.end(), operationRegex);
        std::sregex_iterator operationEnd;

        for (; operationIter != operationEnd; ++operationIter) {
            operations.push_back(operationIter->str());
        }

        if (operations.empty()) {
            response = createErrorResponse("At least one operation is required", 400);
            return;
        }

        // Validate all operations before executing any
        std::vector<ROI> roisToInsert;
        std::vector<ROI> roisToUpdate;
        std::vector<std::string> roisToDelete;
        std::vector<std::string> validationErrors;

        for (size_t i = 0; i < operations.size(); ++i) {
            const std::string& operation = operations[i];
            std::string operationType = parseJsonField(operation, "operation");

            if (operationType == "create") {
                ROI roi;
                if (!deserializeROI(operation, roi)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid ROI format");
                    continue;
                }

                std::string cameraId = parseJsonField(operation, "camera_id");
                if (cameraId.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": camera_id is required");
                    continue;
                }

                // Validate ROI fields
                if (roi.id.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": ROI ID is required");
                    continue;
                }

                if (roi.name.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": ROI name is required");
                    continue;
                }

                // Validate priority level (1-5 scale)
                if (roi.priority < 1 || roi.priority > 5) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Priority must be between 1 and 5");
                    continue;
                }

                // Validate time format if provided
                if (!BehaviorAnalyzer::isValidTimeFormat(roi.start_time)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid start_time format");
                    continue;
                }

                if (!BehaviorAnalyzer::isValidTimeFormat(roi.end_time)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid end_time format");
                    continue;
                }

                // Enhanced polygon validation
                auto validationResult = validateROIPolygonDetailed(roi.polygon);
                if (!validationResult.isValid) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": " + validationResult.errorMessage);
                    continue;
                }

                roisToInsert.push_back(roi);

            } else if (operationType == "update") {
                ROI roi;
                if (!deserializeROI(operation, roi)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid ROI format");
                    continue;
                }

                std::string cameraId = parseJsonField(operation, "camera_id");
                if (cameraId.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": camera_id is required");
                    continue;
                }

                // Similar validation as create
                if (roi.id.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": ROI ID is required");
                    continue;
                }

                if (roi.name.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": ROI name is required");
                    continue;
                }

                if (roi.priority < 1 || roi.priority > 5) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Priority must be between 1 and 5");
                    continue;
                }

                if (!BehaviorAnalyzer::isValidTimeFormat(roi.start_time)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid start_time format");
                    continue;
                }

                if (!BehaviorAnalyzer::isValidTimeFormat(roi.end_time)) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid end_time format");
                    continue;
                }

                auto validationResult = validateROIPolygonDetailed(roi.polygon);
                if (!validationResult.isValid) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": " + validationResult.errorMessage);
                    continue;
                }

                roisToUpdate.push_back(roi);

            } else if (operationType == "delete") {
                std::string roiId = parseJsonField(operation, "roi_id");
                if (roiId.empty()) {
                    validationErrors.push_back("Operation " + std::to_string(i) + ": roi_id is required for delete operation");
                    continue;
                }

                roisToDelete.push_back(roiId);

            } else {
                validationErrors.push_back("Operation " + std::to_string(i) + ": Invalid operation type. Must be 'create', 'update', or 'delete'");
            }
        }

        // If there are validation errors, return them without executing any operations
        if (!validationErrors.empty()) {
            std::ostringstream errorJson;
            errorJson << "{"
                     << "\"error\":\"Validation failed\","
                     << "\"error_code\":\"BULK_VALIDATION_FAILED\","
                     << "\"validation_errors\":[";

            for (size_t i = 0; i < validationErrors.size(); ++i) {
                if (i > 0) errorJson << ",";
                errorJson << "\"" << validationErrors[i] << "\"";
            }

            errorJson << "],"
                     << "\"total_errors\":" << validationErrors.size() << ","
                     << "\"message\":\"All operations must be valid before any can be executed\""
                     << "}";

            response = createJsonResponse(errorJson.str(), 400);
            return;
        }

        // All validations passed, now execute operations atomically
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Database not available", 503);
            return;
        }

        // Begin transaction for atomic operations
        if (!dbManager.beginTransaction()) {
            response = createErrorResponse("Failed to begin transaction: " + dbManager.getErrorMessage(), 500);
            return;
        }

        bool transactionSuccess = true;
        std::string transactionError;

        // Execute operations in order
        try {
            // Process insertions
            for (const auto& roi : roisToInsert) {
                // Convert polygon to JSON string for database storage
                std::ostringstream polygonJson;
                polygonJson << "[";
                for (size_t i = 0; i < roi.polygon.size(); ++i) {
                    if (i > 0) polygonJson << ",";
                    polygonJson << "{\"x\":" << roi.polygon[i].x << ",\"y\":" << roi.polygon[i].y << "}";
                }
                polygonJson << "]";

                // Create ROI record for database
                ROIRecord roiRecord(roi.id, parseJsonField(operations[0], "camera_id"), roi.name, polygonJson.str());
                roiRecord.enabled = roi.enabled;
                roiRecord.priority = roi.priority;
                roiRecord.start_time = roi.start_time;
                roiRecord.end_time = roi.end_time;

                if (!dbManager.insertROI(roiRecord)) {
                    transactionSuccess = false;
                    transactionError = "Failed to insert ROI " + roi.id + ": " + dbManager.getErrorMessage();
                    break;
                }
            }

            // Process updates
            if (transactionSuccess) {
                for (const auto& roi : roisToUpdate) {
                    // Get existing ROI to preserve database ID
                    auto existingROI = dbManager.getROIById(roi.id);
                    if (existingROI.roi_id.empty()) {
                        transactionSuccess = false;
                        transactionError = "ROI not found for update: " + roi.id;
                        break;
                    }

                    // Convert polygon to JSON string
                    std::ostringstream polygonJson;
                    polygonJson << "[";
                    for (size_t i = 0; i < roi.polygon.size(); ++i) {
                        if (i > 0) polygonJson << ",";
                        polygonJson << "{\"x\":" << roi.polygon[i].x << ",\"y\":" << roi.polygon[i].y << "}";
                    }
                    polygonJson << "]";

                    // Update ROI record
                    ROIRecord roiRecord = existingROI;
                    roiRecord.name = roi.name;
                    roiRecord.polygon_data = polygonJson.str();
                    roiRecord.enabled = roi.enabled;
                    roiRecord.priority = roi.priority;
                    roiRecord.start_time = roi.start_time;
                    roiRecord.end_time = roi.end_time;
                    roiRecord.updated_at = getCurrentTimestamp();

                    if (!dbManager.updateROI(roiRecord)) {
                        transactionSuccess = false;
                        transactionError = "Failed to update ROI " + roi.id + ": " + dbManager.getErrorMessage();
                        break;
                    }
                }
            }

            // Process deletions
            if (transactionSuccess) {
                for (const auto& roiId : roisToDelete) {
                    // Check if ROI exists
                    auto existingROI = dbManager.getROIById(roiId);
                    if (existingROI.roi_id.empty()) {
                        transactionSuccess = false;
                        transactionError = "ROI not found for deletion: " + roiId;
                        break;
                    }

                    if (!dbManager.deleteROI(roiId)) {
                        transactionSuccess = false;
                        transactionError = "Failed to delete ROI " + roiId + ": " + dbManager.getErrorMessage();
                        break;
                    }
                }
            }

        } catch (const std::exception& e) {
            transactionSuccess = false;
            transactionError = "Exception during bulk operation: " + std::string(e.what());
        }

        // Commit or rollback transaction
        if (transactionSuccess) {
            if (!dbManager.commitTransaction()) {
                response = createErrorResponse("Failed to commit transaction: " + dbManager.getErrorMessage(), 500);
                return;
            }

            // Update active pipelines if operations succeeded
            TaskManager& taskManager = TaskManager::getInstance();

            // Add new ROIs to active pipelines
            for (const auto& roi : roisToInsert) {
                std::string cameraId = parseJsonField(operations[0], "camera_id"); // Get from first operation
                auto pipeline = taskManager.getPipeline(cameraId);
                if (pipeline) {
                    pipeline->addROI(roi);
                }
            }

            // Update ROIs in active pipelines
            for (const auto& roi : roisToUpdate) {
                std::string cameraId = parseJsonField(operations[0], "camera_id"); // Get from first operation
                auto pipeline = taskManager.getPipeline(cameraId);
                if (pipeline) {
                    pipeline->removeROI(roi.id);
                    pipeline->addROI(roi);
                }
            }

            // Remove ROIs from active pipelines
            for (const auto& roiId : roisToDelete) {
                // Find which camera this ROI belonged to by checking all pipelines
                auto activePipelines = taskManager.getActivePipelines();
                for (const auto& cameraId : activePipelines) {
                    auto pipeline = taskManager.getPipeline(cameraId);
                    if (pipeline) {
                        pipeline->removeROI(roiId);
                    }
                }
            }

            // Create success response
            std::ostringstream json;
            json << "{"
                 << "\"status\":\"success\","
                 << "\"message\":\"Bulk ROI operations completed successfully\","
                 << "\"operations_executed\":" << operations.size() << ","
                 << "\"created\":" << roisToInsert.size() << ","
                 << "\"updated\":" << roisToUpdate.size() << ","
                 << "\"deleted\":" << roisToDelete.size() << ","
                 << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
                 << "}";

            response = createJsonResponse(json.str(), 200);

            LOG_INFO() << "[APIService] Bulk ROI operations completed successfully: "
                      << roisToInsert.size() << " created, "
                      << roisToUpdate.size() << " updated, "
                      << roisToDelete.size() << " deleted";

        } else {
            // Rollback transaction
            dbManager.rollbackTransaction();

            response = createErrorResponse("Bulk operation failed: " + transactionError + ". All changes have been rolled back.", 500);

            LOG_ERROR() << "[APIService] Bulk ROI operation failed and rolled back: " << transactionError;
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to process bulk ROI operations: " + std::string(e.what()), 500);
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
         << "\"priority\":" << roi.priority;

    // Add time fields if they exist
    if (!roi.start_time.empty()) {
        json << ",\"start_time\":\"" << roi.start_time << "\"";
    }
    if (!roi.end_time.empty()) {
        json << ",\"end_time\":\"" << roi.end_time << "\"";
    }

    json << "}";

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
        roi.start_time = parseJsonField(json, "start_time");
        roi.end_time = parseJsonField(json, "end_time");

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
        LOG_ERROR() << "[APIService] Failed to deserialize ROI: " << e.what();
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
        LOG_ERROR() << "[APIService] Failed to deserialize IntrusionRule: " << e.what();
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

        LOG_INFO() << "[APIService] Starting ONVIF device discovery...";

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

        LOG_INFO() << "[APIService] ONVIF discovery completed. Found " << devices.size() << " devices";

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
            LOG_INFO() << "[APIService] Testing authentication for device: " << device->ipAddress
                      << " with username: " << username;

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

            LOG_INFO() << "[APIService] Authentication successful for device: " << device->ipAddress;

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

        LOG_INFO() << "[APIService] Added ONVIF device as video source: " << device->uuid
                  << " (" << device->name << ")";

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
            LOG_WARN() << "[APIService] Warning: Could not create faces directory";
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

        LOG_INFO() << "[APIService] Saved face image: " << imagePath;

        // Extract face embedding using face recognition module
        std::vector<uchar> imageData(imageFile.content.begin(), imageFile.content.end());
        cv::Mat faceImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

        std::vector<float> embedding;
        if (!faceImage.empty()) {
            FaceRecognizer faceRecognizer;
            if (faceRecognizer.initialize()) {
                embedding = faceRecognizer.extractFaceEmbedding(faceImage);
                LOG_INFO() << "[APIService] Generated face embedding with " << embedding.size() << " dimensions";
            } else {
                LOG_ERROR() << "[APIService] Warning: Face recognizer initialization failed, using dummy embedding";
            }
        }

        // Fallback to dummy embedding if face recognition failed
        if (embedding.empty()) {
            LOG_INFO() << "[APIService] Using dummy embedding as fallback";
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

        LOG_INFO() << "[APIService] Face added successfully: " << name
                  << " (ID: " << faceId << ")";

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

        LOG_INFO() << "[APIService] Retrieved " << faces.size() << " faces";

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
                LOG_INFO() << "[APIService] Deleted face image: " << face.image_path;
            } else {
                LOG_WARN() << "[APIService] Warning: Could not delete face image: " << face.image_path;
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

        LOG_INFO() << "[APIService] Face deleted successfully: " << face.name
                  << " (ID: " << id << ")";

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

        LOG_INFO() << "[APIService] Face verification request with threshold: " << threshold;

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

        LOG_INFO() << "[APIService] Found " << registeredFaces.size() << " registered faces for verification";

        // Convert image data to OpenCV Mat
        std::vector<uchar> imageData(imageFile.content.begin(), imageFile.content.end());
        cv::Mat inputImage = cv::imdecode(imageData, cv::IMREAD_COLOR);

        if (inputImage.empty()) {
            response = createErrorResponse("Failed to decode image", 400);
            return;
        }

        LOG_INFO() << "[APIService] Decoded input image: " << inputImage.cols << "x" << inputImage.rows;

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

        LOG_INFO() << "[APIService] Face verification completed: " << verificationResults.size()
                  << " matches found above threshold " << threshold;

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

            // Parse WebSocket configuration
            int port = parseJsonInt(request, "port", 8081);
            if (port < 1024 || port > 65535) {
                response = createErrorResponse("port must be between 1024 and 65535", 400);
                return;
            }
            config.webSocketConfig.port = port;

            int maxConnections = parseJsonInt(request, "max_connections", 100);
            if (maxConnections < 1 || maxConnections > 1000) {
                response = createErrorResponse("max_connections must be between 1 and 1000", 400);
                return;
            }
            config.webSocketConfig.max_connections = maxConnections;

            int pingInterval = parseJsonInt(request, "ping_interval_ms", 30000);
            if (pingInterval < 5000 || pingInterval > 300000) {
                response = createErrorResponse("ping_interval_ms must be between 5000 and 300000", 400);
                return;
            }
            config.webSocketConfig.ping_interval_ms = pingInterval;

        } else if (method == "mqtt") {
            config.method = AlarmMethod::MQTT;

            // Parse MQTT configuration
            std::string broker = parseJsonField(request, "broker");
            if (broker.empty()) {
                response = createErrorResponse("broker is required for MQTT method", 400);
                return;
            }
            config.mqttConfig.broker = broker;

            int port = parseJsonInt(request, "port", 1883);
            if (port < 1 || port > 65535) {
                response = createErrorResponse("port must be between 1 and 65535", 400);
                return;
            }
            config.mqttConfig.port = port;

            std::string topic = parseJsonField(request, "topic");
            if (!topic.empty()) {
                config.mqttConfig.topic = topic;
            }

            std::string clientId = parseJsonField(request, "client_id");
            if (!clientId.empty()) {
                config.mqttConfig.client_id = clientId;
            }

            std::string username = parseJsonField(request, "username");
            if (!username.empty()) {
                config.mqttConfig.username = username;
            }

            std::string password = parseJsonField(request, "password");
            if (!password.empty()) {
                config.mqttConfig.password = password;
            }

            int qos = parseJsonInt(request, "qos", 1);
            if (qos < 0 || qos > 2) {
                response = createErrorResponse("qos must be 0, 1, or 2", 400);
                return;
            }
            config.mqttConfig.qos = qos;

            bool retain = parseJsonField(request, "retain") == "true";
            config.mqttConfig.retain = retain;

            int keepAlive = parseJsonInt(request, "keep_alive_seconds", 60);
            if (keepAlive < 10 || keepAlive > 300) {
                response = createErrorResponse("keep_alive_seconds must be between 10 and 300", 400);
                return;
            }
            config.mqttConfig.keep_alive_seconds = keepAlive;

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

        // Start WebSocket server if this is a WebSocket configuration
        if (config.method == AlarmMethod::WEBSOCKET) {
            alarmTrigger.startWebSocketServer(config.webSocketConfig.port);
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
        } else if (method == "websocket") {
            json << ",\"port\":" << config.webSocketConfig.port << ","
                 << "\"max_connections\":" << config.webSocketConfig.max_connections << ","
                 << "\"ping_interval_ms\":" << config.webSocketConfig.ping_interval_ms;
        } else if (method == "mqtt") {
            json << ",\"broker\":\"" << config.mqttConfig.broker << "\","
                 << "\"port\":" << config.mqttConfig.port << ","
                 << "\"topic\":\"" << config.mqttConfig.topic << "\","
                 << "\"qos\":" << config.mqttConfig.qos << ","
                 << "\"retain\":" << (config.mqttConfig.retain ? "true" : "false") << ","
                 << "\"keep_alive_seconds\":" << config.mqttConfig.keep_alive_seconds;
        }

        json << ",\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);

        LOG_INFO() << "[APIService] Created alarm config: " << config.id
                  << " (method: " << method << ")";

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
            } else if (config.method == AlarmMethod::WEBSOCKET) {
                json << ",\"port\":" << config.webSocketConfig.port << ","
                     << "\"max_connections\":" << config.webSocketConfig.max_connections << ","
                     << "\"ping_interval_ms\":" << config.webSocketConfig.ping_interval_ms;
            } else if (config.method == AlarmMethod::MQTT) {
                json << ",\"broker\":\"" << config.mqttConfig.broker << "\","
                     << "\"port\":" << config.mqttConfig.port << ","
                     << "\"topic\":\"" << config.mqttConfig.topic << "\","
                     << "\"qos\":" << config.mqttConfig.qos << ","
                     << "\"retain\":" << (config.mqttConfig.retain ? "true" : "false") << ","
                     << "\"keep_alive_seconds\":" << config.mqttConfig.keep_alive_seconds;
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
                } else if (config.method == AlarmMethod::WEBSOCKET) {
                    json << ",\"port\":" << config.webSocketConfig.port << ","
                         << "\"max_connections\":" << config.webSocketConfig.max_connections << ","
                         << "\"ping_interval_ms\":" << config.webSocketConfig.ping_interval_ms;
                } else if (config.method == AlarmMethod::MQTT) {
                    json << ",\"broker\":\"" << config.mqttConfig.broker << "\","
                         << "\"port\":" << config.mqttConfig.port << ","
                         << "\"topic\":\"" << config.mqttConfig.topic << "\","
                         << "\"qos\":" << config.mqttConfig.qos << ","
                         << "\"retain\":" << (config.mqttConfig.retain ? "true" : "false") << ","
                         << "\"keep_alive_seconds\":" << config.mqttConfig.keep_alive_seconds;
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
                 << "\"timeout_ms\":" << updatedConfig.httpConfig.timeout_ms;
        } else if (updatedConfig.method == AlarmMethod::WEBSOCKET) {
            json << ",\"port\":" << updatedConfig.webSocketConfig.port << ","
                 << "\"max_connections\":" << updatedConfig.webSocketConfig.max_connections << ","
                 << "\"ping_interval_ms\":" << updatedConfig.webSocketConfig.ping_interval_ms;
        } else if (updatedConfig.method == AlarmMethod::MQTT) {
            json << ",\"broker\":\"" << updatedConfig.mqttConfig.broker << "\","
                 << "\"port\":" << updatedConfig.mqttConfig.port << ","
                 << "\"topic\":\"" << updatedConfig.mqttConfig.topic << "\","
                 << "\"qos\":" << updatedConfig.mqttConfig.qos << ","
                 << "\"retain\":" << (updatedConfig.mqttConfig.retain ? "true" : "false") << ","
                 << "\"keep_alive_seconds\":" << updatedConfig.mqttConfig.keep_alive_seconds;
        }

        json << ",\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] Updated alarm config: " << configId;

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

        LOG_INFO() << "[APIService] Deleted alarm config: " << configId;

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

        LOG_INFO() << "[APIService] Test alarm triggered: " << eventType
                  << " for camera: " << cameraId;

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

// Task 76: ReID configuration handlers implementation

void APIService::handlePostReIDConfig(const std::string& request, std::string& response) {
    try {
        // Parse ReID configuration from request
        bool enabled = parseJsonBool(request, "enabled", true);
        float threshold = parseJsonFloat(request, "similarity_threshold", 0.7f);
        int maxMatches = parseJsonInt(request, "max_matches", 5);
        float matchTimeout = parseJsonFloat(request, "match_timeout", 30.0f);
        bool crossCameraEnabled = parseJsonBool(request, "cross_camera_enabled", true);

        // Validate threshold
        if (threshold < 0.5f || threshold > 0.95f) {
            response = createErrorResponse("similarity_threshold must be between 0.5 and 0.95", 400);
            return;
        }

        // Validate max matches
        if (maxMatches < 1 || maxMatches > 20) {
            response = createErrorResponse("max_matches must be between 1 and 20", 400);
            return;
        }

        // Validate match timeout
        if (matchTimeout < 5.0 || matchTimeout > 300.0) {
            response = createErrorResponse("match_timeout must be between 5.0 and 300.0 seconds", 400);
            return;
        }

        // Apply configuration to all active pipelines
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        int updatedPipelines = 0;
        for (const auto& pipelineId : activePipelines) {
            auto pipeline = taskManager.getPipeline(pipelineId);
            if (pipeline) {
                auto behaviorAnalyzer = pipeline->getBehaviorAnalyzer();
                if (behaviorAnalyzer) {
                    ReIDConfig config;
                    config.enabled = enabled;
                    config.similarityThreshold = threshold;
                    config.maxMatches = maxMatches;
                    config.matchTimeout = matchTimeout;
                    config.crossCameraEnabled = crossCameraEnabled;

                    behaviorAnalyzer->setReIDConfig(config);
                    updatedPipelines++;
                }
            }
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"config_updated\","
             << "\"config\":{"
             << "\"enabled\":" << (enabled ? "true" : "false") << ","
             << "\"similarity_threshold\":" << threshold << ","
             << "\"max_matches\":" << maxMatches << ","
             << "\"match_timeout\":" << matchTimeout << ","
             << "\"cross_camera_enabled\":" << (crossCameraEnabled ? "true" : "false")
             << "},"
             << "\"updated_pipelines\":" << updatedPipelines << ","
             << "\"total_pipelines\":" << activePipelines.size() << ","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] ReID configuration updated: enabled=" << enabled
                  << ", threshold=" << threshold << ", pipelines=" << updatedPipelines;

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update ReID config: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetReIDConfig(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        if (activePipelines.empty()) {
            response = createErrorResponse("No active pipelines found", 404);
            return;
        }

        // Get configuration from first active pipeline
        auto pipeline = taskManager.getPipeline(activePipelines[0]);
        if (!pipeline) {
            response = createErrorResponse("Failed to access pipeline", 500);
            return;
        }

        auto behaviorAnalyzer = pipeline->getBehaviorAnalyzer();
        if (!behaviorAnalyzer) {
            response = createErrorResponse("BehaviorAnalyzer not available", 500);
            return;
        }

        ReIDConfig config = behaviorAnalyzer->getReIDConfig();

        std::ostringstream json;
        json << "{"
             << "\"config\":{"
             << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
             << "\"similarity_threshold\":" << config.similarityThreshold << ","
             << "\"max_matches\":" << config.maxMatches << ","
             << "\"match_timeout\":" << config.matchTimeout << ","
             << "\"cross_camera_enabled\":" << (config.crossCameraEnabled ? "true" : "false")
             << "},"
             << "\"constraints\":{"
             << "\"threshold_range\":{\"min\":0.5,\"max\":0.95},"
             << "\"max_matches_range\":{\"min\":1,\"max\":20},"
             << "\"timeout_range\":{\"min\":5.0,\"max\":300.0}"
             << "},"
             << "\"active_pipelines\":" << activePipelines.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get ReID config: " + std::string(e.what()), 500);
    }
}

void APIService::handlePutReIDThreshold(const std::string& request, std::string& response) {
    try {
        // Parse threshold from request
        float threshold = parseJsonFloat(request, "threshold", -1.0f);
        if (threshold < 0.0f) {
            response = createErrorResponse("threshold field is required", 400);
            return;
        }

        // Validate threshold
        if (threshold < 0.5f || threshold > 0.95f) {
            response = createErrorResponse("threshold must be between 0.5 and 0.95", 400);
            return;
        }

        // Apply threshold to all active pipelines
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        int updatedPipelines = 0;
        for (const auto& pipelineId : activePipelines) {
            auto pipeline = taskManager.getPipeline(pipelineId);
            if (pipeline) {
                auto behaviorAnalyzer = pipeline->getBehaviorAnalyzer();
                if (behaviorAnalyzer) {
                    behaviorAnalyzer->setReIDSimilarityThreshold(threshold);
                    updatedPipelines++;
                }
            }
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"threshold_updated\","
             << "\"threshold\":" << threshold << ","
             << "\"updated_pipelines\":" << updatedPipelines << ","
             << "\"total_pipelines\":" << activePipelines.size() << ","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        LOG_INFO() << "[APIService] ReID similarity threshold updated to " << threshold
                  << " for " << updatedPipelines << " pipelines";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update ReID threshold: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetReIDStatus(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        std::ostringstream json;
        json << "{"
             << "\"reid_system\":{"
             << "\"total_pipelines\":" << activePipelines.size() << ","
             << "\"enabled_pipelines\":0,"
             << "\"avg_similarity_threshold\":0.0,"
             << "\"total_matches\":0,"
             << "\"cross_camera_tracks\":" << taskManager.getActiveCrossCameraTrackCount()
             << "},"
             << "\"pipelines\":[";

        int enabledPipelines = 0;
        float totalThreshold = 0.0f;
        int totalMatches = 0;

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";

            const auto& pipelineId = activePipelines[i];
            auto pipeline = taskManager.getPipeline(pipelineId);

            bool reidEnabled = false;
            float threshold = 0.0f;
            int matches = 0;

            if (pipeline) {
                auto behaviorAnalyzer = pipeline->getBehaviorAnalyzer();
                if (behaviorAnalyzer) {
                    reidEnabled = behaviorAnalyzer->isReIDEnabled();
                    threshold = behaviorAnalyzer->getReIDSimilarityThreshold();

                    if (reidEnabled) {
                        enabledPipelines++;
                        totalThreshold += threshold;
                        // TODO: Get actual match count from behavior analyzer
                        matches = 0; // Placeholder
                        totalMatches += matches;
                    }
                }
            }

            json << "{"
                 << "\"pipeline_id\":\"" << pipelineId << "\","
                 << "\"reid_enabled\":" << (reidEnabled ? "true" : "false") << ","
                 << "\"similarity_threshold\":" << threshold << ","
                 << "\"active_matches\":" << matches
                 << "}";
        }

        json << "],"
             << "\"statistics\":{"
             << "\"enabled_ratio\":" << (activePipelines.size() > 0 ?
                 (double)enabledPipelines / activePipelines.size() * 100.0 : 0.0) << ","
             << "\"avg_threshold\":" << (enabledPipelines > 0 ?
                 totalThreshold / enabledPipelines : 0.0f) << ","
             << "\"total_active_matches\":" << totalMatches
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get ReID status: " + std::string(e.what()), 500);
    }
}

// Frontend compatibility handlers implementation
void APIService::handleGetAlerts(const std::string& request, std::string& response) {
    try {
        // Return empty alerts list for now - can be extended later
        std::ostringstream json;
        json << "{"
             << "\"alerts\":[],"
             << "\"total\":0,"
             << "\"page\":1,"
             << "\"per_page\":20,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        LOG_INFO() << "[APIService] Returned alerts list (empty)";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alerts: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetSystemInfo(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();

        std::ostringstream json;
        json << "{"
             << "\"system_name\":\"AI Security Vision System\","
             << "\"version\":\"1.0.0\","
             << "\"build_date\":\"" << __DATE__ << " " << __TIME__ << "\","
             << "\"platform\":\"RK3588 Ubuntu\","
             << "\"cpu_cores\":" << std::thread::hardware_concurrency() << ","
             << "\"memory_total\":\"8GB\","
             << "\"gpu_info\":\"" << taskManager.getGpuMemoryUsage() << "\","
             << "\"uptime_seconds\":0,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        LOG_INFO() << "[APIService] Returned system info";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get system info: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetCameras(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();
        auto activePipelines = taskManager.getActivePipelines();

        std::ostringstream json;
        json << "{"
             << "\"cameras\":[";

        for (size_t i = 0; i < activePipelines.size(); ++i) {
            if (i > 0) json << ",";

            const auto& pipelineId = activePipelines[i];
            auto pipeline = taskManager.getPipeline(pipelineId);

            std::string status = "online";  // Changed from "active" to "online" for frontend compatibility
            std::string name = pipelineId;
            std::string url = "unknown";

            if (pipeline) {
                auto source = pipeline->getSource();
                name = source.name;
                url = source.url;

                // Check if pipeline is running and processing frames to determine status
                if (!pipeline->isRunning() || pipeline->getFrameRate() < 0.1) {
                    status = "offline";
                }
            }

            json << "{"
                 << "\"id\":\"" << pipelineId << "\","
                 << "\"name\":\"" << name << "\","
                 << "\"url\":\"" << url << "\","
                 << "\"status\":\"" << status << "\","
                 << "\"enabled\":true,"
                 << "\"created_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";
        }

        json << "],"
             << "\"total\":" << activePipelines.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        LOG_INFO() << "[APIService] Returned cameras list (" << activePipelines.size() << " cameras)";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get cameras: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetRecordings(const std::string& request, std::string& response) {
    try {
        // Return empty recordings list for now - can be extended later
        std::ostringstream json;
        json << "{"
             << "\"recordings\":[],"
             << "\"total\":0,"
             << "\"page\":1,"
             << "\"per_page\":20,"
             << "\"total_size\":0,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        LOG_INFO() << "[APIService] Returned recordings list (empty)";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get recordings: " + std::string(e.what()), 500);
    }
}

void APIService::handleGetDetectionConfig(const std::string& request, std::string& response) {
    try {
        TaskManager& taskManager = TaskManager::getInstance();

        std::ostringstream json;
        json << "{"
             << "\"detection_enabled\":true,"
             << "\"confidence_threshold\":0.5,"
             << "\"nms_threshold\":0.4,"
             << "\"max_detections\":100,"
             << "\"classes_enabled\":[\"person\",\"car\",\"truck\",\"bicycle\",\"motorcycle\"],"
             << "\"model_type\":\"yolov8n\","
             << "\"model_format\":\"rknn\","
             << "\"inference_device\":\"npu\","
             << "\"batch_size\":1,"
             << "\"input_size\":[640,640],"
             << "\"active_pipelines\":" << taskManager.getActivePipelineCount() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        LOG_INFO() << "[APIService] Returned detection config";

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get detection config: " + std::string(e.what()), 500);
    }
}

void APIService::handleTestCameraConnection(const std::string& request, std::string& response) {
    try {
        // Parse connection test parameters
        std::string url = parseJsonField(request, "url");
        std::string username = parseJsonField(request, "username");
        std::string password = parseJsonField(request, "password");
        std::string protocol = parseJsonField(request, "protocol");

        // Validate required fields
        if (url.empty()) {
            response = createErrorResponse("url is required", 400);
            return;
        }

        if (protocol.empty()) {
            protocol = "rtsp"; // Default to RTSP
        }

        LOG_INFO() << "[APIService] Testing camera connection: " << url;

        // Create a temporary VideoSource for testing
        VideoSource testSource;
        testSource.id = "test_connection";
        testSource.name = "Connection Test";
        testSource.url = url;
        testSource.protocol = protocol;
        testSource.username = username;
        testSource.password = password;
        testSource.width = 640;
        testSource.height = 480;
        testSource.fps = 25;
        testSource.enabled = true;

        // Try to create a temporary decoder to test the connection
        bool connectionSuccess = false;
        std::string errorMessage = "";

        try {
            // Create a temporary FFmpeg decoder
            auto decoder = std::make_unique<FFmpegDecoder>();
            if (decoder->initialize(testSource)) {
                // Try to get one frame to verify the connection
                cv::Mat testFrame;
                int64_t timestamp;
                if (decoder->getNextFrame(testFrame, timestamp)) {
                    connectionSuccess = true;
                    LOG_INFO() << "[APIService] Camera connection test successful: " << url;
                } else {
                    errorMessage = "Failed to receive video frames";
                }
                decoder->cleanup();
            } else {
                errorMessage = "Failed to initialize video decoder";
            }
        } catch (const std::exception& e) {
            errorMessage = "Connection error: " + std::string(e.what());
        }

        // Return test result
        std::ostringstream json;
        json << "{"
             << "\"success\":" << (connectionSuccess ? "true" : "false") << ","
             << "\"url\":\"" << url << "\","
             << "\"protocol\":\"" << protocol << "\"";

        if (!connectionSuccess) {
            json << ",\"message\":\"" << errorMessage << "\"";
        }

        json << ",\"tested_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        if (connectionSuccess) {
            LOG_INFO() << "[APIService] Camera connection test passed: " << url;
        } else {
            LOG_WARN() << "[APIService] Camera connection test failed: " << url << " - " << errorMessage;
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to test camera connection: " + std::string(e.what()), 500);
    }
}

// Video stream proxy handler
void APIService::handleStreamProxy(const std::string& cameraId, const httplib::Request& req, httplib::Response& res) {
    try {
        if (cameraId.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Camera ID is required\"}", "application/json");
            return;
        }

        // Get the pipeline for this camera
        TaskManager& taskManager = TaskManager::getInstance();
        auto pipeline = taskManager.getPipeline(cameraId);

        if (!pipeline) {
            res.status = 404;
            res.set_content("{\"error\":\"Camera not found: " + cameraId + "\"}", "application/json");
            return;
        }

        // Get the stream URL from the pipeline
        std::string streamUrl = pipeline->getStreamUrl();

        if (streamUrl.empty()) {
            res.status = 503;
            res.set_content("{\"error\":\"Stream not available for camera: " + cameraId + "\"}", "application/json");
            return;
        }

        // Check if streaming is enabled
        if (!pipeline->isStreamingEnabled()) {
            // Try to start streaming
            if (!pipeline->startStreaming()) {
                res.status = 503;
                res.set_content("{\"error\":\"Failed to start streaming for camera: " + cameraId + "\"}", "application/json");
                return;
            }
            streamUrl = pipeline->getStreamUrl();
        }

        // Parse the stream URL to extract host and port
        std::regex urlRegex(R"(http://([^:]+):(\d+)(/.*)?)");
        std::smatch urlMatch;

        if (!std::regex_match(streamUrl, urlMatch, urlRegex)) {
            res.status = 500;
            res.set_content("{\"error\":\"Invalid stream URL format: " + streamUrl + "\"}", "application/json");
            return;
        }

        std::string host = urlMatch[1].str();
        int port = std::stoi(urlMatch[2].str());
        std::string path = urlMatch.size() > 3 ? urlMatch[3].str() : "/";

        // Create HTTP client to proxy the stream
        httplib::Client client(host, port);
        client.set_connection_timeout(5, 0); // 5 seconds timeout
        client.set_read_timeout(30, 0);      // 30 seconds read timeout

        // Forward the request to the actual stream server
        auto streamRes = client.Get(path.c_str());

        if (!streamRes) {
            res.status = 503;
            res.set_content("{\"error\":\"Failed to connect to stream server\"}", "application/json");
            return;
        }

        if (streamRes->status != 200) {
            res.status = streamRes->status;
            res.set_content("{\"error\":\"Stream server returned status: " + std::to_string(streamRes->status) + "\"}", "application/json");
            return;
        }

        // Forward the response
        res.status = streamRes->status;
        res.body = streamRes->body;

        // Copy headers
        for (const auto& header : streamRes->headers) {
            res.set_header(header.first.c_str(), header.second.c_str());
        }

        LOG_INFO() << "[APIService] Proxied stream request for camera: " << cameraId
                  << " to " << streamUrl;

    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content("{\"error\":\"Stream proxy error: " + std::string(e.what()) + "\"}", "application/json");
        LOG_ERROR() << "[APIService] Stream proxy error for camera " << cameraId << ": " << e.what();
    }
}

