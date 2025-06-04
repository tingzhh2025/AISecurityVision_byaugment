#include "CameraController.h"
#include "../../core/TaskManager.h"
#include "../../video/FFmpegDecoder.h"
#include "../../database/DatabaseManager.h"
#include "../../onvif/ONVIFDiscovery.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <regex>

using namespace AISecurityVision;

void CameraController::initialize(TaskManager* taskManager, 
                                 ONVIFManager* onvifManager, 
                                 AISecurityVision::NetworkManager* networkManager) {
    BaseController::initialize(taskManager, onvifManager, networkManager);
    logInfo("CameraController initialized");
}

void CameraController::handleGetCameras(const std::string& request, std::string& response) {
    try {
        // Get cameras from both in-memory config and database to ensure completeness
        std::vector<CameraConfig> allCameras = m_cameraConfigs;

        // Also load cameras from database that might not be in memory
        DatabaseManager dbManager;
        if (dbManager.initialize()) {
            auto cameraIds = dbManager.getAllCameraIds();

            for (const std::string& cameraId : cameraIds) {
                // Check if this camera is already in m_cameraConfigs
                bool found = false;
                for (const auto& existingCamera : m_cameraConfigs) {
                    if (existingCamera.id == cameraId) {
                        found = true;
                        break;
                    }
                }

                // If not found in memory, load from database
                if (!found) {
                    std::string configJson = dbManager.getCameraConfig(cameraId);
                    if (!configJson.empty()) {
                        try {
                            nlohmann::json config = nlohmann::json::parse(configJson);
                            CameraConfig camera;
                            camera.id = cameraId;
                            camera.name = config.value("name", cameraId);
                            camera.url = config.value("url", "");
                            camera.protocol = config.value("protocol", "rtsp");
                            camera.username = config.value("username", "");
                            camera.password = config.value("password", "");
                            camera.width = config.value("width", 1280);
                            camera.height = config.value("height", 720);
                            camera.fps = config.value("fps", 25);
                            camera.mjpeg_port = config.value("mjpeg_port", 8000);
                            camera.enabled = config.value("enabled", true);

                            allCameras.push_back(camera);
                        } catch (const std::exception& e) {
                            logWarn("Failed to parse camera config for " + cameraId + ": " + e.what());
                        }
                    }
                }
            }
        }

        std::ostringstream json;
        json << "{"
             << "\"cameras\":[";

        // Add all cameras (both in-memory and database-loaded)
        for (size_t i = 0; i < allCameras.size(); ++i) {
            if (i > 0) json << ",";

            const auto& camera = allCameras[i];
            json << "{"
                 << "\"id\":\"" << camera.id << "\","
                 << "\"name\":\"" << camera.name << "\","
                 << "\"url\":\"" << camera.url << "\","
                 << "\"protocol\":\"" << camera.protocol << "\","
                 << "\"username\":\"" << camera.username << "\","
                 << "\"password\":\"" << camera.password << "\","
                 << "\"width\":" << camera.width << ","
                 << "\"height\":" << camera.height << ","
                 << "\"fps\":" << camera.fps << ","
                 << "\"mjpeg_port\":" << camera.mjpeg_port << ","
                 << "\"enabled\":" << (camera.enabled ? "true" : "false") << ","
                 << "\"status\":\"configured\","
                 << "\"created_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";
        }

        json << "],"
             << "\"total\":" << allCameras.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned cameras list (" + std::to_string(allCameras.size()) + " cameras)");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get cameras: " + std::string(e.what()), 500);
    }
}

void CameraController::handleGetVideoSources(const std::string& request, std::string& response) {
    if (!m_taskManager) {
        response = createErrorResponse("TaskManager not initialized", 500);
        return;
    }

    auto activePipelines = m_taskManager->getActivePipelines();

    std::ostringstream json;
    json << "{\"sources\":[";

    for (size_t i = 0; i < activePipelines.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":\"" << activePipelines[i] << "\",\"status\":\"active\"}";
    }

    json << "]}";

    response = createJsonResponse(json.str());
}

void CameraController::clearInMemoryConfigurations() {
    logInfo("Clearing in-memory camera configurations");
    m_cameraConfigs.clear();
    logInfo("In-memory camera configurations cleared");
}

std::string CameraController::serializeCameraConfig(const CameraConfig& config) {
    std::ostringstream json;
    json << "{"
         << "\"id\":\"" << config.id << "\","
         << "\"name\":\"" << config.name << "\","
         << "\"url\":\"" << config.url << "\","
         << "\"protocol\":\"" << config.protocol << "\","
         << "\"username\":\"" << config.username << "\","
         << "\"password\":\"" << config.password << "\","
         << "\"width\":" << config.width << ","
         << "\"height\":" << config.height << ","
         << "\"fps\":" << config.fps << ","
         << "\"mjpeg_port\":" << config.mjpeg_port << ","
         << "\"enabled\":" << (config.enabled ? "true" : "false")
         << "}";
    return json.str();
}

std::string CameraController::serializeCameraConfigList(const std::vector<CameraConfig>& configs) {
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < configs.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeCameraConfig(configs[i]);
    }
    json << "]";
    return json.str();
}

bool CameraController::deserializeCameraConfig(const std::string& json, CameraConfig& config) {
    try {
        auto j = nlohmann::json::parse(json);

        config.id = j.value("id", "");
        config.name = j.value("name", "");
        config.url = j.value("url", "");
        config.protocol = j.value("protocol", "rtsp");
        config.username = j.value("username", "");
        config.password = j.value("password", "");
        config.width = j.value("width", 1280);
        config.height = j.value("height", 720);
        config.fps = j.value("fps", 25);
        config.mjpeg_port = j.value("mjpeg_port", 8000);
        config.enabled = j.value("enabled", true);

        return true;
    } catch (const std::exception& e) {
        logError("Failed to deserialize camera config: " + std::string(e.what()));
        return false;
    }
}

void CameraController::handlePostVideoSource(const std::string& request, std::string& response) {
    try {
        logInfo("POST /api/cameras request received");
        logDebug("Request body: " + request);

        // Parse JSON request to extract video source parameters
        std::string id = parseJsonField(request, "id");
        logDebug("Parsed id: '" + id + "'");

        std::string name = parseJsonField(request, "name");
        std::string url = parseJsonField(request, "url");
        std::string protocol = parseJsonField(request, "protocol");
        std::string username = parseJsonField(request, "username");
        std::string password = parseJsonField(request, "password");
        int width = parseJsonInt(request, "width", 1920);
        int height = parseJsonInt(request, "height", 1080);
        int fps = parseJsonInt(request, "fps", 25);
        int mjpeg_port = parseJsonInt(request, "mjpeg_port", 8161);
        bool enabled = parseJsonBool(request, "enabled", true);

        // Validate required fields
        if (id.empty()) {
            response = createErrorResponse("id is required", 400);
            return;
        }

        if (name.empty()) {
            response = createErrorResponse("name is required", 400);
            return;
        }

        if (url.empty()) {
            response = createErrorResponse("url is required", 400);
            return;
        }

        // Set default protocol if not specified
        if (protocol.empty()) {
            protocol = "rtsp";
        }

        // Validate protocol
        if (protocol != "rtsp" && protocol != "http" && protocol != "file") {
            response = createErrorResponse("protocol must be 'rtsp', 'http', or 'file'", 400);
            return;
        }

        // Validate dimensions
        if (width <= 0 || width > 7680) {
            response = createErrorResponse("width must be between 1 and 7680", 400);
            return;
        }

        if (height <= 0 || height > 4320) {
            response = createErrorResponse("height must be between 1 and 4320", 400);
            return;
        }

        // Validate FPS
        if (fps <= 0 || fps > 120) {
            response = createErrorResponse("fps must be between 1 and 120", 400);
            return;
        }

        // Check if camera with this ID already exists
        for (const auto& existingCamera : m_cameraConfigs) {
            if (existingCamera.id == id) {
                response = createErrorResponse("Camera with ID '" + id + "' already exists", 409);
                return;
            }
        }

        // Create camera configuration
        CameraConfig camera;
        camera.id = id;
        camera.name = name;
        camera.url = url;
        camera.protocol = protocol;
        camera.username = username;
        camera.password = password;
        camera.width = width;
        camera.height = height;
        camera.fps = fps;
        camera.mjpeg_port = mjpeg_port;
        camera.enabled = enabled;

        // Add to in-memory configuration
        m_cameraConfigs.push_back(camera);

        // Save to database
        DatabaseManager dbManager;
        if (dbManager.initialize()) {
            std::string configJson = serializeCameraConfig(camera);
            if (!dbManager.saveCameraConfig(id, configJson)) {
                logWarn("Failed to save camera config to database for: " + id);
            }
        }

        // Create video source for TaskManager
        if (m_taskManager) {
            VideoSource source;
            source.id = id;
            source.name = name;
            source.url = url;
            source.protocol = protocol;
            source.username = username;
            source.password = password;
            source.width = width;
            source.height = height;
            source.fps = fps;
            source.enabled = enabled;

            if (m_taskManager->addVideoSource(source)) {
                logInfo("Successfully added video source: " + id);
            } else {
                logWarn("Failed to add video source to TaskManager: " + id);
            }
        }

        // Create success response
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Camera added successfully\","
             << "\"camera_id\":\"" << id << "\","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);
        logInfo("Successfully added camera: " + id);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to parse video source request: " + std::string(e.what()), 400);
    }
}

void CameraController::handleDeleteVideoSource(const std::string& request, std::string& response) {
    // TODO: Extract source ID from URL path and remove from TaskManager
    response = createJsonResponse("{\"message\":\"Delete video source endpoint not implemented yet\"}");
}

void CameraController::handleTestCameraConnection(const std::string& request, std::string& response) {
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

        logInfo("Testing camera connection: " + url);

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
                    logInfo("Camera connection test successful: " + url);
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
            logInfo("Camera connection test passed: " + url);
        } else {
            logWarn("Camera connection test failed: " + url + " - " + errorMessage);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to test camera connection: " + std::string(e.what()), 500);
    }
}

void CameraController::handleGetDiscoverDevices(const std::string& request, std::string& response) {
    try {
        if (!m_onvifManager || !m_onvifManager->isInitialized()) {
            response = createErrorResponse("ONVIF discovery not available", 503);
            return;
        }

        logInfo("Starting ONVIF device discovery...");

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
                 << "\"name\":\"" << device.name << "\","
                 << "\"manufacturer\":\"" << device.manufacturer << "\","
                 << "\"model\":\"" << device.model << "\","
                 << "\"firmware_version\":\"" << device.firmwareVersion << "\","
                 << "\"serial_number\":\"" << device.serialNumber << "\","
                 << "\"hardware_id\":\"" << device.uuid << "\","  // Use uuid as hardware_id
                 << "\"ip_address\":\"" << device.ipAddress << "\","
                 << "\"port\":" << device.port << ","
                 << "\"service_url\":\"" << device.serviceUrl << "\","
                 << "\"stream_uri\":\"" << device.streamUri << "\","
                 << "\"requires_auth\":" << (device.requiresAuth ? "true" : "false")
                 << "}";
        }

        json << "],"
             << "\"scan_duration_ms\":" << 5000 << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("ONVIF discovery completed, found " + std::to_string(devices.size()) + " devices");

    } catch (const std::exception& e) {
        response = createErrorResponse("ONVIF discovery failed: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostAddDiscoveredDevice(const std::string& request, std::string& response) {
    try {
        // Parse the discovered device information from request
        std::string deviceId = parseJsonField(request, "device_id");
        std::string name = parseJsonField(request, "name");
        std::string ipAddress = parseJsonField(request, "ip_address");
        std::string username = parseJsonField(request, "username");
        std::string password = parseJsonField(request, "password");

        if (deviceId.empty() || ipAddress.empty()) {
            response = createErrorResponse("device_id and ip_address are required", 400);
            return;
        }

        // Create camera configuration from discovered device
        std::string cameraId = "camera_" + deviceId;
        std::string url = "rtsp://" + ipAddress + ":554/1/1"; // Default RTSP URL

        if (!username.empty() && !password.empty()) {
            url = "rtsp://" + username + ":" + password + "@" + ipAddress + ":554/1/1";
        }

        // Create camera config
        CameraConfig camera;
        camera.id = cameraId;
        camera.name = name.empty() ? ("Camera " + deviceId) : name;
        camera.url = url;
        camera.protocol = "rtsp";
        camera.username = username;
        camera.password = password;
        camera.width = 1920;
        camera.height = 1080;
        camera.fps = 25;
        camera.mjpeg_port = 8161; // Default MJPEG port
        camera.enabled = true;

        // Add to in-memory configuration
        m_cameraConfigs.push_back(camera);

        // Save to database
        DatabaseManager dbManager;
        if (dbManager.initialize()) {
            std::string configJson = serializeCameraConfig(camera);
            if (!dbManager.saveCameraConfig(cameraId, configJson)) {
                logWarn("Failed to save discovered camera config to database for: " + cameraId);
            }
        }

        // Add to TaskManager
        if (m_taskManager) {
            VideoSource source;
            source.id = cameraId;
            source.name = camera.name;
            source.url = camera.url;
            source.protocol = camera.protocol;
            source.username = camera.username;
            source.password = camera.password;
            source.width = camera.width;
            source.height = camera.height;
            source.fps = camera.fps;
            source.enabled = camera.enabled;

            if (m_taskManager->addVideoSource(source)) {
                logInfo("Successfully added discovered camera: " + cameraId);
            } else {
                logWarn("Failed to add discovered camera to TaskManager: " + cameraId);
            }
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Discovered device added successfully\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"device_id\":\"" << deviceId << "\","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);
        logInfo("Successfully added discovered device as camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to add discovered device: " + std::string(e.what()), 500);
    }
}
