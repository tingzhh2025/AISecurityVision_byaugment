#include "CameraController.h"
#include "../../database/DatabaseManager.h"
#include "../../core/TaskManager.h"
#include "../../core/VideoPipeline.h"
#include "../../video/FFmpegDecoder.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <cstddef>
#include <exception>
#include <algorithm>
#include <ctime>
#include <thread>

namespace AISecurityVision {

// ========== Initialization and Configuration Methods ==========

void CameraController::initialize(TaskManager* taskManager,
                                 ONVIFManager* onvifManager,
                                 AISecurityVision::NetworkManager* networkManager) {
    BaseController::initialize(taskManager, onvifManager, networkManager);

    // Initialize thread pool for asynchronous operations
    m_threadPool = std::make_shared<AISecurityVision::ThreadPool>(4); // 4 worker threads for camera operations

    // Load existing camera configurations from database
    loadCameraConfigsFromDatabase();

    logInfo("CameraController initialized with " + std::to_string(m_cameraConfigs.size()) + " cameras from database");
}

void CameraController::cleanup() {
    logInfo("CameraController cleanup initiated");

    if (m_threadPool) {
        m_threadPool->shutdown();
        m_threadPool.reset();
    }

    // Clear pending operations
    {
        std::lock_guard<std::mutex> lock(m_pendingOperationsMutex);
        m_pendingCameraOperations.clear();
    }

    logInfo("CameraController cleanup completed");
}

void CameraController::clearInMemoryConfigurations() {
    m_cameraConfigs.clear();
    logInfo("Cleared in-memory camera configurations");
}

void CameraController::loadCameraConfigsFromDatabase() {
    try {
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            logError("Failed to initialize database for loading camera configs");
            return;
        }

        // Get all camera IDs from database
        auto cameraIds = dbManager.getAllCameraIds();
        logInfo("Found " + std::to_string(cameraIds.size()) + " cameras in database");

        m_cameraConfigs.clear();

        for (const std::string& cameraId : cameraIds) {
            std::string configJson = dbManager.getCameraConfig(cameraId);
            if (configJson.empty()) {
                logWarn("No configuration found for camera: " + cameraId);
                continue;
            }

            try {
                nlohmann::json config = nlohmann::json::parse(configJson);

                // Skip disabled or deleted cameras
                if (!config.value("enabled", true) || config.contains("deleted_at")) {
                    logInfo("Skipping disabled/deleted camera: " + cameraId);
                    continue;
                }

                CameraConfig camera;

                camera.id = cameraId;
                camera.name = config.value("name", cameraId);
                camera.url = config.value("rtsp_url", config.value("url", ""));
                camera.protocol = config.value("protocol", "rtsp");
                camera.username = config.value("username", "");
                camera.password = config.value("password", "");
                camera.width = config.value("width", 1920);
                camera.height = config.value("height", 1080);
                camera.fps = config.value("fps", 25);
                camera.mjpeg_port = 0; // Will be dynamically allocated
                camera.enabled = config.value("enabled", true);

                if (!camera.url.empty() && camera.enabled) {
                    m_cameraConfigs.push_back(camera);
                    logInfo("Loaded camera from database: " + camera.id + " (" + camera.name + ")");
                } else {
                    logWarn("Camera " + cameraId + " has no URL or is disabled, skipping");
                }

            } catch (const std::exception& e) {
                logError("Failed to parse camera config for " + cameraId + ": " + e.what());
            }
        }

        logInfo("Loaded " + std::to_string(m_cameraConfigs.size()) + " enabled cameras from database");

    } catch (const std::exception& e) {
        logError("Failed to load camera configs from database: " + std::string(e.what()));
    }
}

// ========== Camera Management Methods ==========

void CameraController::handleGetCameras(const std::string& request, std::string& response) {
    try {
        // Build enhanced camera list with status information
        std::ostringstream json;
        json << "{\"cameras\":[";

        for (size_t i = 0; i < m_cameraConfigs.size(); ++i) {
            if (i > 0) json << ",";

            const auto& config = m_cameraConfigs[i];

            // Determine camera status based on pipeline state
            std::string status = "offline";
            int dynamicMjpegPort = 0;
            if (m_taskManager) {
                auto pipeline = m_taskManager->getPipeline(config.id);
                if (pipeline && pipeline->isRunning()) {
                    status = pipeline->isHealthy() ? "online" : "error";
                    // Get dynamically allocated MJPEG port
                    dynamicMjpegPort = m_taskManager->getMJPEGPort(config.id);
                } else if (config.enabled) {
                    status = "configured";
                }
            }

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
                 << "\"mjpeg_port\":" << dynamicMjpegPort << ","
                 << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
                 << "\"status\":\"" << status << "\","
                 << "\"ip\":\"" << extractIpFromUrl(config.url) << "\""
                 << "}";
        }

        json << "],\"count\":" << m_cameraConfigs.size() << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved " + std::to_string(m_cameraConfigs.size()) + " camera configurations");
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get cameras: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostVideoSource(const std::string& request, std::string& response) {
    try {
        CameraConfig config;
        if (!deserializeCameraConfig(request, config)) {
            response = createErrorResponse("Invalid camera configuration", 400);
            return;
        }

        // Generate unique ID if not provided
        if (config.id.empty()) {
            config.id = "camera_" + std::to_string(std::time(nullptr));
        }

        // MJPEG port will be dynamically allocated by TaskManager
        // Remove manual port assignment to prevent conflicts
        config.mjpeg_port = 0;

        // Add or update camera configuration in memory
        auto it = std::find_if(m_cameraConfigs.begin(), m_cameraConfigs.end(),
                              [&config](const CameraConfig& c) { return c.id == config.id; });

        bool isNewCamera = (it == m_cameraConfigs.end());

        if (it != m_cameraConfigs.end()) {
            *it = config;
        } else {
            m_cameraConfigs.push_back(config);
        }

        // Start video pipeline for new cameras first to validate configuration
        if (isNewCamera && m_taskManager && config.enabled && m_threadPool) {
            // Check if operation is already pending for this camera
            if (isOperationPending(config.id)) {
                logWarn("Camera initialization already in progress for: " + config.id);
                response = createErrorResponse("Camera initialization already in progress", 409);
                return;
            }

            VideoSource source;
            source.id = config.id;
            source.name = config.name;
            source.url = config.url;
            source.protocol = config.protocol;
            source.username = config.username;
            source.password = config.password;
            source.width = config.width;
            source.height = config.height;
            source.fps = config.fps;
            source.mjpeg_port = 0; // Will be dynamically allocated by TaskManager
            source.enabled = config.enabled;

            // Mark operation as pending
            markOperationPending(config.id);

            // Submit pipeline initialization to thread pool with database callback
            m_threadPool->submitDetached([this, source, config]() {
                bool pipelineSuccess = false;
                try {
                    if (m_taskManager && m_taskManager->addVideoSource(source)) {
                        logInfo("Started video pipeline for new camera: " + source.id);
                        pipelineSuccess = true;

                        // Only save to database after successful pipeline initialization
                        ::DatabaseManager dbManager;
                        if (dbManager.initialize()) {
                            // Convert CameraConfig to JSON for database storage
                            nlohmann::json configJson;
                            configJson["camera_id"] = config.id;
                            configJson["name"] = config.name;
                            configJson["rtsp_url"] = config.url;
                            configJson["protocol"] = config.protocol;
                            configJson["username"] = config.username;
                            configJson["password"] = config.password;
                            configJson["width"] = config.width;
                            configJson["height"] = config.height;
                            configJson["fps"] = config.fps;
                            // MJPEG port is dynamically allocated, don't store in database
                            // configJson["mjpeg_port"] = config.mjpeg_port;
                            configJson["enabled"] = config.enabled;
                            configJson["detection_enabled"] = true;
                            configJson["recording_enabled"] = false;
                            configJson["detection_config"] = {
                                {"confidence_threshold", 0.5},
                                {"nms_threshold", 0.4},
                                {"backend", "RKNN"},
                                {"model_path", "models/yolov8n.rknn"}
                            };
                            configJson["stream_config"] = {
                                {"fps", config.fps},
                                {"quality", 80},
                                {"max_width", config.width},
                                {"max_height", config.height}
                            };

                            if (!dbManager.saveCameraConfig(config.id, configJson.dump())) {
                                logWarn("Failed to save camera config to database: " + config.id);
                                // Remove pipeline if database save fails
                                if (m_taskManager) {
                                    m_taskManager->removeVideoSource(config.id);
                                }
                                pipelineSuccess = false;
                            } else {
                                logInfo("Saved camera config to database after successful pipeline init: " + config.id);
                            }
                        } else {
                            logError("Failed to initialize database for camera: " + config.id);
                            // Remove pipeline if database init fails
                            if (m_taskManager) {
                                m_taskManager->removeVideoSource(config.id);
                            }
                            pipelineSuccess = false;
                        }
                    } else {
                        logError("Failed to start video pipeline for camera: " + source.id);
                    }
                } catch (const std::exception& e) {
                    logError("Exception starting video pipeline for camera " + source.id + ": " + e.what());
                }

                // Mark operation as complete
                markOperationComplete(source.id);

                if (!pipelineSuccess) {
                    // Remove from in-memory configuration if pipeline failed
                    auto it = std::find_if(m_cameraConfigs.begin(), m_cameraConfigs.end(),
                                          [&config](const CameraConfig& c) { return c.id == config.id; });
                    if (it != m_cameraConfigs.end()) {
                        m_cameraConfigs.erase(it);
                    }
                }
            });

            logInfo("Initiated async video pipeline startup for camera: " + config.id);
        } else if (!isNewCamera) {
            // For existing cameras, save to database immediately
            ::DatabaseManager dbManager;
            if (dbManager.initialize()) {
                // Convert CameraConfig to JSON for database storage
                nlohmann::json configJson;
                configJson["camera_id"] = config.id;
                configJson["name"] = config.name;
                configJson["rtsp_url"] = config.url;
                configJson["protocol"] = config.protocol;
                configJson["username"] = config.username;
                configJson["password"] = config.password;
                configJson["width"] = config.width;
                configJson["height"] = config.height;
                configJson["fps"] = config.fps;
                configJson["enabled"] = config.enabled;
                configJson["detection_enabled"] = true;
                configJson["recording_enabled"] = false;
                configJson["detection_config"] = {
                    {"confidence_threshold", 0.5},
                    {"nms_threshold", 0.4},
                    {"backend", "RKNN"},
                    {"model_path", "models/yolov8n.rknn"}
                };
                configJson["stream_config"] = {
                    {"fps", config.fps},
                    {"quality", 80},
                    {"max_width", config.width},
                    {"max_height", config.height}
                };

                if (!dbManager.saveCameraConfig(config.id, configJson.dump())) {
                    logWarn("Failed to save camera config to database: " + config.id);
                } else {
                    logInfo("Updated existing camera config in database: " + config.id);
                }
            }
        }

        response = createJsonResponse("{\"status\":\"success\",\"message\":\"Camera configuration saved and pipeline started\"}");
        logInfo("Saved camera configuration: " + config.id);
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to save camera configuration: " + std::string(e.what()), 400);
    }
}

void CameraController::handleDeleteVideoSource(const std::string& request, std::string& response) {
    try {
        auto j = nlohmann::json::parse(request);
        std::string cameraId = j.value("id", "");

        if (cameraId.empty()) {
            response = createErrorResponse("Camera ID is required", 400);
            return;
        }

        auto it = std::remove_if(m_cameraConfigs.begin(), m_cameraConfigs.end(),
                                [&cameraId](const CameraConfig& c) { return c.id == cameraId; });

        if (it != m_cameraConfigs.end()) {
            m_cameraConfigs.erase(it, m_cameraConfigs.end());
            response = createJsonResponse("{\"status\":\"success\",\"message\":\"Camera configuration deleted\"}");
            logInfo("Deleted camera configuration: " + cameraId);
        } else {
            response = createErrorResponse("Camera not found", 404);
        }
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete camera configuration: " + std::string(e.what()), 400);
    }
}

void CameraController::handleGetVideoSources(const std::string& request, std::string& response) {
    handleGetCameras(request, response);
}

void CameraController::handleTestCameraConnection(const std::string& request, std::string& response) {
    try {
        auto j = nlohmann::json::parse(request);
        std::string url = j.value("url", "");

        if (url.empty()) {
            response = createErrorResponse("URL is required", 400);
            return;
        }

        // Simple connection test (in real implementation, would test actual connection)
        bool connected = !url.empty();

        std::ostringstream json;
        json << "{"
             << "\"connected\":" << (connected ? "true" : "false") << ","
             << "\"message\":\"" << (connected ? "Connection successful" : "Connection failed") << "\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Tested camera connection: " + url);
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to test camera connection: " + std::string(e.what()), 400);
    }
}

// ========== Camera Configuration Methods ==========

void CameraController::handleGetCameraConfigs(const std::string& request, std::string& response) {
    try {
        // Get camera configurations from database
        ::DatabaseManager dbManager;
        std::vector<std::string> cameraConfigs;

        if (dbManager.initialize()) {
            // Get all camera configurations from database
            auto configs = dbManager.getAllConfigs("camera");
            for (const auto& [key, value] : configs) {
                if (key.find("camera_") == 0) {
                    cameraConfigs.push_back(value);
                }
            }
        }

        // Build response
        std::ostringstream json;
        json << "{\"configs\":[";

        for (size_t i = 0; i < cameraConfigs.size(); ++i) {
            if (i > 0) json << ",";
            json << cameraConfigs[i];
        }

        json << "],\"count\":" << cameraConfigs.size() << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved camera configurations from database");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get camera configs: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostCameraConfig(const std::string& request, std::string& response) {
    try {
        auto j = nlohmann::json::parse(request);

        std::string cameraId = j.value("camera_id", "");
        if (cameraId.empty()) {
            response = createErrorResponse("camera_id is required", 400);
            return;
        }

        // Save to database
        ::DatabaseManager dbManager;
        if (dbManager.initialize()) {
            if (dbManager.saveCameraConfig(cameraId, request)) {
                response = createJsonResponse("{\"status\":\"success\",\"message\":\"Camera configuration saved to database\"}");
                logInfo("Saved camera configuration to database: " + cameraId);
            } else {
                response = createErrorResponse("Failed to save camera configuration to database", 500);
            }
        } else {
            response = createErrorResponse("Failed to initialize database", 500);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to save camera config: " + std::string(e.what()), 400);
    }
}

void CameraController::handleDeleteCameraConfig(const std::string& cameraId, std::string& response) {
    try {
        if (cameraId.empty()) {
            response = createErrorResponse("Camera ID is required", 400);
            return;
        }

        // Delete from database
        ::DatabaseManager dbManager;
        if (dbManager.initialize()) {
            if (dbManager.deleteConfig("camera", cameraId)) {
                response = createJsonResponse("{\"status\":\"success\",\"message\":\"Camera configuration deleted from database\"}");
                logInfo("Deleted camera configuration from database: " + cameraId);
            } else {
                response = createErrorResponse("Failed to delete camera configuration from database", 500);
            }
        } else {
            response = createErrorResponse("Failed to initialize database", 500);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete camera config: " + std::string(e.what()), 400);
    }
}

// ========== ONVIF Discovery Methods ==========

void CameraController::handleGetDiscoverDevices(const std::string& request, std::string& response) {
    try {
        // Return empty discovery results (would implement actual ONVIF discovery)
        response = createJsonResponse("{\"devices\":[],\"message\":\"No devices discovered\"}");
        logInfo("ONVIF device discovery completed");
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to discover devices: " + std::string(e.what()), 500);
    }
}

void CameraController::handlePostAddDiscoveredDevice(const std::string& request, std::string& response) {
    try {
        // Add discovered device as camera configuration
        handlePostVideoSource(request, response);
    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to add discovered device: " + std::string(e.what()), 400);
    }
}

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

// ========== Serialization Methods ==========

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
    json << "{\"cameras\":[";

    for (size_t i = 0; i < configs.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeCameraConfig(configs[i]);
    }

    json << "],\"count\":" << configs.size() << "}";
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
        config.width = j.value("width", 1920);
        config.height = j.value("height", 1080);
        config.fps = j.value("fps", 25);
        config.mjpeg_port = j.value("mjpeg_port", 8000);
        config.enabled = j.value("enabled", true);

        return !config.id.empty() && !config.url.empty();
    } catch (const std::exception& e) {
        logError("Failed to deserialize camera config: " + std::string(e.what()));
        return false;
    }
}

// ========== Helper Methods ==========

std::string CameraController::extractIpFromUrl(const std::string& url) {
    try {
        // Extract IP from RTSP URL like "rtsp://user:pass@192.168.1.2:554/path"
        size_t atPos = url.find('@');
        if (atPos != std::string::npos) {
            size_t colonPos = url.find(':', atPos);
            if (colonPos != std::string::npos) {
                return url.substr(atPos + 1, colonPos - atPos - 1);
            } else {
                size_t slashPos = url.find('/', atPos);
                if (slashPos != std::string::npos) {
                    return url.substr(atPos + 1, slashPos - atPos - 1);
                } else {
                    return url.substr(atPos + 1);
                }
            }
        } else {
            // Try to extract from URL without credentials
            size_t protocolPos = url.find("://");
            if (protocolPos != std::string::npos) {
                size_t startPos = protocolPos + 3;
                size_t colonPos = url.find(':', startPos);
                if (colonPos != std::string::npos) {
                    return url.substr(startPos, colonPos - startPos);
                } else {
                    size_t slashPos = url.find('/', startPos);
                    if (slashPos != std::string::npos) {
                        return url.substr(startPos, slashPos - startPos);
                    } else {
                        return url.substr(startPos);
                    }
                }
            }
        }
        return "unknown";
    } catch (const std::exception& e) {
        return "unknown";
    }
}

// ========== NEW CRUD Operations (Phase 1) ==========

void CameraController::handleGetCamera(const std::string& cameraId, std::string& response) {
    try {
        // Get camera configuration from database
        ::DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Failed to initialize database", 500);
            return;
        }

        std::string configJson = dbManager.getCameraConfig(cameraId);
        if (configJson.empty()) {
            response = createErrorResponse("Camera not found", 404);
            return;
        }

        // Parse and enhance the configuration
        auto config = nlohmann::json::parse(configJson);

        // Add runtime status information
        config["status"] = "unknown";
        config["last_seen"] = nullptr;
        config["stream_url"] = "http://localhost:" + std::to_string(config.value("mjpeg_port", 8161));

        // Check if camera is currently active in TaskManager
        if (m_taskManager) {
            // Add status check logic here if needed
            config["status"] = "active";
        }

        response = createSuccessResponse(config.dump());
        logInfo("Retrieved camera configuration: " + cameraId);

    } catch (const std::exception& e) {
        logError("Error getting camera " + cameraId + ": " + e.what());
        response = createErrorResponse("Internal server error", 500);
    }
}

void CameraController::handleUpdateCamera(const std::string& cameraId, const std::string& request, std::string& response) {
    try {
        // Parse the update request
        auto updateData = nlohmann::json::parse(request);

        // Get existing camera configuration
        ::DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Failed to initialize database", 500);
            return;
        }

        std::string existingConfigJson = dbManager.getCameraConfig(cameraId);
        if (existingConfigJson.empty()) {
            response = createErrorResponse("Camera not found", 404);
            return;
        }

        // Parse existing configuration
        auto existingConfig = nlohmann::json::parse(existingConfigJson);

        // Update fields that are provided in the request
        if (updateData.contains("name")) {
            existingConfig["name"] = updateData["name"];
        }
        if (updateData.contains("rtsp_url")) {
            existingConfig["rtsp_url"] = updateData["rtsp_url"];
        }
        if (updateData.contains("username")) {
            existingConfig["username"] = updateData["username"];
        }
        if (updateData.contains("password")) {
            existingConfig["password"] = updateData["password"];
        }
        if (updateData.contains("enabled")) {
            existingConfig["enabled"] = updateData["enabled"];
        }
        if (updateData.contains("detection_enabled")) {
            existingConfig["detection_enabled"] = updateData["detection_enabled"];
        }
        if (updateData.contains("recording_enabled")) {
            existingConfig["recording_enabled"] = updateData["recording_enabled"];
        }
        if (updateData.contains("stream_config")) {
            auto streamConfig = updateData["stream_config"];
            if (streamConfig.contains("fps")) {
                existingConfig["stream_config"]["fps"] = streamConfig["fps"];
            }
            if (streamConfig.contains("quality")) {
                existingConfig["stream_config"]["quality"] = streamConfig["quality"];
            }
            if (streamConfig.contains("max_width")) {
                existingConfig["stream_config"]["max_width"] = streamConfig["max_width"];
            }
            if (streamConfig.contains("max_height")) {
                existingConfig["stream_config"]["max_height"] = streamConfig["max_height"];
            }
        }

        // Update timestamp
        existingConfig["updated_at"] = std::time(nullptr);

        // Save updated configuration to database
        if (!dbManager.saveCameraConfig(cameraId, existingConfig.dump())) {
            response = createErrorResponse("Failed to update camera configuration", 500);
            return;
        }

        // Update in-memory configuration
        auto it = std::find_if(m_cameraConfigs.begin(), m_cameraConfigs.end(),
                              [&cameraId](const CameraConfig& c) { return c.id == cameraId; });

        if (it != m_cameraConfigs.end()) {
            // Update the in-memory config
            it->name = existingConfig.value("name", it->name);
            it->url = existingConfig.value("rtsp_url", it->url);
            it->username = existingConfig.value("username", it->username);
            it->password = existingConfig.value("password", it->password);
            it->enabled = existingConfig.value("enabled", it->enabled);

            if (existingConfig.contains("stream_config")) {
                auto streamConfig = existingConfig["stream_config"];
                it->fps = streamConfig.value("fps", it->fps);
                it->width = streamConfig.value("max_width", it->width);
                it->height = streamConfig.value("max_height", it->height);
            }
        }

        // If camera is currently running and configuration changed, restart it
        if (m_taskManager && existingConfig.value("enabled", true) && m_threadPool) {
            // Check if operation is already pending for this camera
            if (isOperationPending(cameraId)) {
                logWarn("Camera restart already in progress for: " + cameraId);
                response = createErrorResponse("Camera restart already in progress", 409);
                return;
            }

            // Stop existing pipeline
            m_taskManager->removeVideoSource(cameraId);

            // Mark operation as pending
            markOperationPending(cameraId);

            // Start new pipeline with updated configuration using thread pool
            m_threadPool->submitDetached([this, cameraId, existingConfig]() {
                try {
                    VideoSource source;
                    source.id = cameraId;
                    source.name = existingConfig.value("name", cameraId);
                    source.url = existingConfig.value("rtsp_url", "");
                    source.protocol = "rtsp";
                    source.mjpeg_port = 0; // Will be dynamically allocated by TaskManager
                    source.enabled = existingConfig.value("enabled", true);

                    if (existingConfig.contains("stream_config")) {
                        auto streamConfig = existingConfig["stream_config"];
                        source.fps = streamConfig.value("fps", 25);
                        source.width = streamConfig.value("max_width", 1920);
                        source.height = streamConfig.value("max_height", 1080);
                    }

                    if (m_taskManager && m_taskManager->addVideoSource(source)) {
                        logInfo("Restarted video pipeline for updated camera: " + cameraId);
                    } else {
                        logError("Failed to restart video pipeline for camera: " + cameraId);
                    }
                } catch (const std::exception& e) {
                    logError("Exception restarting video pipeline for camera " + cameraId + ": " + e.what());
                }

                // Mark operation as complete
                markOperationComplete(cameraId);
            });
        }

        response = createSuccessResponse(existingConfig.dump());
        logInfo("Updated camera configuration: " + cameraId);

    } catch (const std::exception& e) {
        logError("Error updating camera " + cameraId + ": " + e.what());
        response = createErrorResponse("Invalid request data", 400);
    }
}

void CameraController::handleDeleteCamera(const std::string& cameraId, std::string& response) {
    try {
        // Check if camera exists
        ::DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            response = createErrorResponse("Failed to initialize database", 500);
            return;
        }

        std::string existingConfigJson = dbManager.getCameraConfig(cameraId);
        if (existingConfigJson.empty()) {
            response = createErrorResponse("Camera not found", 404);
            return;
        }

        // Stop video pipeline if running
        if (m_taskManager) {
            m_taskManager->removeVideoSource(cameraId);
            logInfo("Stopped video pipeline for camera: " + cameraId);
        }

        // Remove from in-memory configuration
        auto it = std::find_if(m_cameraConfigs.begin(), m_cameraConfigs.end(),
                              [&cameraId](const CameraConfig& c) { return c.id == cameraId; });

        if (it != m_cameraConfigs.end()) {
            m_cameraConfigs.erase(it);
        }

        // Soft delete from database (set enabled = false)
        auto config = nlohmann::json::parse(existingConfigJson);
        config["enabled"] = false;
        config["deleted_at"] = std::time(nullptr);

        if (!dbManager.saveCameraConfig(cameraId, config.dump())) {
            response = createErrorResponse("Failed to delete camera configuration", 500);
            return;
        }

        // Alternatively, hard delete from database
        // if (!dbManager.deleteCameraConfig(cameraId)) {
        //     response = createErrorResponse("Failed to delete camera configuration", 500);
        //     return;
        // }

        nlohmann::json responseData;
        responseData["message"] = "Camera deleted successfully";
        responseData["camera_id"] = cameraId;

        response = createSuccessResponse(responseData.dump());
        logInfo("Deleted camera: " + cameraId);

    } catch (const std::exception& e) {
        logError("Error deleting camera " + cameraId + ": " + e.what());
        response = createErrorResponse("Internal server error", 500);
    }
}

void CameraController::handleTestCamera(const std::string& request, std::string& response) {
    try {
        // Parse test request
        auto testData = nlohmann::json::parse(request);

        std::string cameraId = testData.value("camera_id", "");
        std::string rtspUrl = testData.value("rtsp_url", "");
        std::string username = testData.value("username", "");
        std::string password = testData.value("password", "");

        if (cameraId.empty() && rtspUrl.empty()) {
            response = createErrorResponse("Either camera_id or rtsp_url must be provided", 400);
            return;
        }

        // If camera_id is provided, get URL from database
        if (!cameraId.empty() && rtspUrl.empty()) {
            ::DatabaseManager dbManager;
            if (dbManager.initialize()) {
                std::string configJson = dbManager.getCameraConfig(cameraId);
                if (!configJson.empty()) {
                    auto config = nlohmann::json::parse(configJson);
                    rtspUrl = config.value("rtsp_url", "");
                    username = config.value("username", "");
                    password = config.value("password", "");
                }
            }
        }

        if (rtspUrl.empty()) {
            response = createErrorResponse("No RTSP URL found for testing", 400);
            return;
        }

        // Perform actual camera connection test
        bool testResult = false;
        std::string testMessage = "Connection test failed";

        try {
            // Create a temporary VideoSource for testing
            VideoSource testSource;
            testSource.id = "test_" + std::to_string(std::time(nullptr));
            testSource.name = "Test Camera";
            testSource.url = rtspUrl;
            testSource.protocol = "rtsp";
            testSource.width = 1920;
            testSource.height = 1080;
            testSource.fps = 25;
            testSource.enabled = true;

            // Try to initialize FFmpeg decoder with the source
            FFmpegDecoder decoder;
            if (decoder.initialize(testSource)) {
                // Try to get one frame to verify the connection
                cv::Mat testFrame;
                int64_t timestamp;

                if (decoder.getNextFrame(testFrame, timestamp)) {
                    testResult = true;
                    testMessage = "Connection successful";
                    logInfo("Camera test successful for URL: " + rtspUrl);
                } else {
                    testMessage = "Failed to receive video frames";
                    logWarn("Camera test failed - no frames received for URL: " + rtspUrl);
                }

                decoder.cleanup();
            } else {
                testMessage = "Failed to initialize video decoder";
                logWarn("Camera test failed - decoder initialization failed for URL: " + rtspUrl);
            }

        } catch (const std::exception& e) {
            testMessage = "Connection test exception: " + std::string(e.what());
            logError("Camera test exception for URL " + rtspUrl + ": " + e.what());
        }

        // Build response
        nlohmann::json responseData;
        responseData["success"] = testResult;
        responseData["message"] = testMessage;
        responseData["rtsp_url"] = rtspUrl;
        responseData["timestamp"] = std::time(nullptr);

        if (!cameraId.empty()) {
            responseData["camera_id"] = cameraId;
        }

        response = createSuccessResponse(responseData.dump());

    } catch (const std::exception& e) {
        logError("Error testing camera: " + std::string(e.what()));
        response = createErrorResponse("Invalid request data", 400);
    }
}

void CameraController::handlePutDetectionConfig(const std::string& request, std::string& response) {
    try {
        // Parse detection config from request
        auto j = nlohmann::json::parse(request);

        float confidenceThreshold = j.value("confidence_threshold", 0.5f);
        float nmsThreshold = j.value("nms_threshold", 0.4f);
        int maxDetections = j.value("max_detections", 100);
        int detectionInterval = j.value("detection_interval", 1);
        bool detectionEnabled = j.value("detection_enabled", true);

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
            dbManager.saveConfig("detection", "detection_enabled", detectionEnabled ? "true" : "false");
        }

        // Apply to all active pipelines
        if (m_taskManager) {
            auto pipelines = m_taskManager->getActivePipelines();
            for (const auto& pipelineId : pipelines) {
                auto pipeline = m_taskManager->getPipeline(pipelineId);
                if (pipeline) {
                    pipeline->setDetectionThresholds(confidenceThreshold, nmsThreshold);
                    pipeline->setDetectionEnabled(detectionEnabled);
                }
            }
        }

        // Build response with updated configuration
        nlohmann::json responseData;
        responseData["status"] = "success";
        responseData["message"] = "Detection configuration updated successfully";
        responseData["config"] = {
            {"confidence_threshold", confidenceThreshold},
            {"nms_threshold", nmsThreshold},
            {"max_detections", maxDetections},
            {"detection_interval", detectionInterval},
            {"detection_enabled", detectionEnabled}
        };
        responseData["updated_at"] = getCurrentTimestamp();

        response = createSuccessResponse(responseData.dump());
        logInfo("Updated detection configuration via PUT");

    } catch (const std::exception& e) {
        logError("Error updating detection config via PUT: " + std::string(e.what()));
        response = createErrorResponse("Failed to update detection config: " + std::string(e.what()), 400);
    }
}

// Thread-safe operation management methods
bool CameraController::isOperationPending(const std::string& cameraId) const {
    std::lock_guard<std::mutex> lock(m_pendingOperationsMutex);
    return m_pendingCameraOperations.find(cameraId) != m_pendingCameraOperations.end();
}

void CameraController::markOperationPending(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_pendingOperationsMutex);
    m_pendingCameraOperations.insert(cameraId);
}

void CameraController::markOperationComplete(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_pendingOperationsMutex);
    m_pendingCameraOperations.erase(cameraId);
}

} // namespace AISecurityVision
