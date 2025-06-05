#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <fstream>
#include "core/TaskManager.h"
#include "core/VideoPipeline.h"
#include "api/APIService.h"
#include "database/DatabaseManager.h"
#include "nlohmann/json.hpp"

#include "core/Logger.h"
using namespace AISecurityVision;
using json = nlohmann::json;
// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    static int signalCount = 0;
    signalCount++;

    LOG_INFO() << "\n[Main] Received signal " << signal << " (count: " << signalCount << "), shutting down...";
    g_running.store(false);

    // Force exit after 3 signals
    if (signalCount >= 3) {
        LOG_ERROR() << "[Main] Force exit after multiple signals";
        std::exit(1);
    }
}

struct CameraConfig {
    std::string id;
    std::string name;
    std::string rtsp_url;
    int mjpeg_port;
    bool enabled;
    bool detection_enabled;
    bool recording_enabled;
    struct {
        float confidence_threshold;
        float nms_threshold;
        std::string backend;
        std::string model_path;
    } detection_config;
    struct {
        int fps;
        int quality;
        int max_width;
        int max_height;
    } stream_config;
};

std::vector<CameraConfig> loadCameraConfig(const std::string& configPath) {
    std::vector<CameraConfig> cameras;

    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            LOG_ERROR() << "[Config] Failed to open config file: " << configPath;
            return cameras;
        }

        json config;
        file >> config;

        if (!config.contains("cameras")) {
            LOG_ERROR() << "[Config] No 'cameras' section found in config file";
            return cameras;
        }

        for (const auto& cam : config["cameras"]) {
            CameraConfig camera;
            camera.id = cam.value("id", "");
            camera.name = cam.value("name", "");
            camera.rtsp_url = cam.value("rtsp_url", "");
            camera.mjpeg_port = cam.value("mjpeg_port", 8000);
            camera.enabled = cam.value("enabled", true);
            camera.detection_enabled = cam.value("detection_enabled", true);
            camera.recording_enabled = cam.value("recording_enabled", false);

            if (cam.contains("detection_config")) {
                const auto& det = cam["detection_config"];
                camera.detection_config.confidence_threshold = det.value("confidence_threshold", 0.5f);
                camera.detection_config.nms_threshold = det.value("nms_threshold", 0.4f);
                camera.detection_config.backend = det.value("backend", "RKNN");
                camera.detection_config.model_path = det.value("model_path", "models/yolov8n.rknn");
            }

            if (cam.contains("stream_config")) {
                const auto& stream = cam["stream_config"];
                camera.stream_config.fps = stream.value("fps", 25);
                camera.stream_config.quality = stream.value("quality", 80);
                camera.stream_config.max_width = stream.value("max_width", 1280);
                camera.stream_config.max_height = stream.value("max_height", 720);
            }

            cameras.push_back(camera);
            LOG_INFO() << "[Config] Loaded camera: " << camera.id
                      << " (port: " << camera.mjpeg_port << ")";
        }

        LOG_INFO() << "[Config] Successfully loaded " << cameras.size() << " cameras from " << configPath;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[Config] Error parsing config file: " << e.what();
    }

    return cameras;
}

std::vector<CameraConfig> loadCameraConfigFromDatabase() {
    std::vector<CameraConfig> cameras;

    try {
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            LOG_ERROR() << "[Config] Failed to initialize database for config loading";
            return cameras;
        }

        // Get all camera IDs from database
        auto cameraIds = dbManager.getAllCameraIds();
        LOG_INFO() << "[Config] Found " << cameraIds.size() << " cameras in database";

        for (const std::string& cameraId : cameraIds) {
            std::string configJson = dbManager.getCameraConfig(cameraId);
            if (configJson.empty()) {
                LOG_WARN() << "[Config] No configuration found for camera: " << cameraId;
                continue;
            }

            try {
                json config = json::parse(configJson);
                CameraConfig camera;

                camera.id = cameraId;
                camera.name = config.value("name", cameraId);
                camera.rtsp_url = config.value("rtsp_url", config.value("url", ""));
                camera.mjpeg_port = config.value("mjpeg_port", 8000);
                camera.enabled = config.value("enabled", true);
                camera.detection_enabled = config.value("detection_enabled", true);
                camera.recording_enabled = config.value("recording_enabled", false);

                if (config.contains("detection_config")) {
                    const auto& det = config["detection_config"];
                    camera.detection_config.confidence_threshold = det.value("confidence_threshold", 0.5f);
                    camera.detection_config.nms_threshold = det.value("nms_threshold", 0.4f);
                    camera.detection_config.backend = det.value("backend", "RKNN");
                    camera.detection_config.model_path = det.value("model_path", "models/yolov8n.rknn");
                } else {
                    // Set default detection config
                    camera.detection_config.confidence_threshold = 0.5f;
                    camera.detection_config.nms_threshold = 0.4f;
                    camera.detection_config.backend = "RKNN";
                    camera.detection_config.model_path = "models/yolov8n.rknn";
                }

                if (config.contains("stream_config")) {
                    const auto& stream = config["stream_config"];
                    camera.stream_config.fps = stream.value("fps", 25);
                    camera.stream_config.quality = stream.value("quality", 80);
                    camera.stream_config.max_width = stream.value("max_width", 1280);
                    camera.stream_config.max_height = stream.value("max_height", 720);
                } else {
                    // Set default stream config
                    camera.stream_config.fps = 25;
                    camera.stream_config.quality = 80;
                    camera.stream_config.max_width = 1280;
                    camera.stream_config.max_height = 720;
                }

                if (!camera.rtsp_url.empty()) {
                    cameras.push_back(camera);
                    LOG_INFO() << "[Config] Loaded camera from database: " << camera.id << " (" << camera.name << ")";
                } else {
                    LOG_WARN() << "[Config] Camera " << cameraId << " has no RTSP URL, skipping";
                }

            } catch (const std::exception& e) {
                LOG_ERROR() << "[Config] Failed to parse camera config for " << cameraId << ": " << e.what();
            }
        }

        LOG_INFO() << "[Config] Loaded " << cameras.size() << " cameras from database";

    } catch (const std::exception& e) {
        LOG_ERROR() << "[Config] Failed to load camera configs from database: " << e.what();
    }

    return cameras;
}

// System configuration structure
struct SystemConfig {
    bool optimized_detection = false;
    int detection_threads = 3;
    bool verbose_logging = false;
    int status_interval = 30; // seconds

    // Default constructor with sensible defaults
    SystemConfig() = default;
};

// Load system configuration from database
SystemConfig loadSystemConfig() {
    SystemConfig config;

    try {
        DatabaseManager dbManager;
        if (!dbManager.initialize()) {
            LOG_WARN() << "[Config] Failed to initialize database for system config loading, using defaults";
            return config;
        }

        // Load system configuration from database
        config.optimized_detection = (dbManager.getConfig("system", "optimized_detection", "false") == "true");
        config.detection_threads = std::stoi(dbManager.getConfig("system", "detection_threads", "3"));
        config.verbose_logging = (dbManager.getConfig("system", "verbose_logging", "false") == "true");
        config.status_interval = std::stoi(dbManager.getConfig("system", "status_interval", "30"));

        // Validate detection threads range
        if (config.detection_threads < 1 || config.detection_threads > 8) {
            LOG_WARN() << "[Config] Invalid detection_threads value: " << config.detection_threads
                      << ", using default: 3";
            config.detection_threads = 3;
        }

        LOG_INFO() << "[Config] Loaded system configuration from database:";
        LOG_INFO() << "  - Optimized detection: " << (config.optimized_detection ? "enabled" : "disabled");
        LOG_INFO() << "  - Detection threads: " << config.detection_threads;
        LOG_INFO() << "  - Verbose logging: " << (config.verbose_logging ? "enabled" : "disabled");
        LOG_INFO() << "  - Status interval: " << config.status_interval << "s";

    } catch (const std::exception& e) {
        LOG_ERROR() << "[Config] Error loading system config from database: " << e.what();
        LOG_INFO() << "[Config] Using default system configuration";
    }

    return config;
}

// Load person statistics configuration from database for a camera
void loadPersonStatsConfig(const std::string& cameraId, TaskManager& taskManager) {
    try {
        DatabaseManager dbManager;
        if (!dbManager.initialize("aibox.db")) {
            LOG_WARN() << "[Config] Failed to initialize database for person stats config loading";
            return;
        }

        std::string configKey = "person_stats_" + cameraId;
        std::string savedConfig = dbManager.getConfig("person_statistics", configKey, "");

        if (!savedConfig.empty()) {
            auto pipeline = taskManager.getPipeline(cameraId);
            if (!pipeline) {
                LOG_WARN() << "[Config] Pipeline not found for camera: " << cameraId;
                return;
            }

            try {
                nlohmann::json configJson = nlohmann::json::parse(savedConfig);
                bool enabled = configJson.value("enabled", false);
                float genderThreshold = configJson.value("gender_threshold", 0.7f);
                float ageThreshold = configJson.value("age_threshold", 0.6f);
                int batchSize = configJson.value("batch_size", 4);
                bool enableCaching = configJson.value("enable_caching", true);

                // Apply configuration to pipeline
                pipeline->setPersonStatsEnabled(enabled);
                pipeline->setPersonStatsConfig(genderThreshold, ageThreshold, batchSize, enableCaching);

                LOG_INFO() << "[Config] Loaded person stats config for camera: " << cameraId
                           << " (enabled=" << enabled << ", gender_threshold=" << genderThreshold
                           << ", age_threshold=" << ageThreshold << ")";
            } catch (const std::exception& e) {
                LOG_WARN() << "[Config] Failed to parse person stats config for camera " << cameraId << ": " << e.what();
            }
        } else {
            LOG_DEBUG() << "[Config] No saved person stats config found for camera: " << cameraId;
        }
    } catch (const std::exception& e) {
        LOG_ERROR() << "[Config] Error loading person stats config for camera " << cameraId << ": " << e.what();
    }
}

void printUsage(const char* programName) {
    LOG_INFO() << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -p, --port       API server port (default: 8080)\n"
              << "  -c, --config     Configuration file path (fallback if database empty)\n"
              << "  -v, --verbose    Enable verbose logging\n"
              << "\nNote: All operational settings (cameras, detection, optimization)\n"
              << "      are now loaded from the database configuration.\n";
}

int main(int argc, char* argv[]) {
    LOG_INFO() << "=== AI Security Vision System ===";
    LOG_INFO() << "Version: 1.0.0";
    LOG_INFO() << "Build: " << __DATE__ << " " << __TIME__;
    LOG_INFO() << "===================================";

    // Parse command line arguments
    int apiPort = 8080;
    std::string configFile;
    bool cmdLineVerbose = false; // Command-line verbose flag

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                apiPort = std::atoi(argv[++i]);
            } else {
                LOG_ERROR() << "Error: Port number required";
                return 1;
            }
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                LOG_ERROR() << "Error: Config file path required";
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            cmdLineVerbose = true;
        } else {
            LOG_ERROR() << "Error: Unknown argument: " << arg;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Load system configuration from database
        LOG_INFO() << "[Main] Loading system configuration...";
        SystemConfig systemConfig = loadSystemConfig();

        // Apply verbose logging setting (command-line overrides database)
        bool verbose = cmdLineVerbose || systemConfig.verbose_logging;
        if (verbose) {
            LOG_INFO() << "[Main] Verbose logging enabled";
        }

        // Initialize TaskManager
        LOG_INFO() << "[Main] Initializing TaskManager...";
        TaskManager& taskManager = TaskManager::getInstance();
        taskManager.start();

        // Initialize API Service
        LOG_INFO() << "[Main] Starting API service on port " << apiPort << "...";
        APIService apiService(apiPort);

        // Clear any in-memory configurations to ensure clean state
        apiService.clearInMemoryConfigurations();

        if (!apiService.start()) {
            LOG_ERROR() << "[Main] Failed to start API service";
            return 1;
        }

        // Reload camera configurations in API service after clearing
        LOG_INFO() << "[Main] Reloading camera configurations in API service...";
        apiService.reloadCameraConfigurations();

        // Load cameras from database first, then config file as fallback
        std::vector<VideoSource> cameras;

        // Always try to load from database first
        LOG_INFO() << "[Main] Attempting to load cameras from database...";
        auto databaseCameras = loadCameraConfigFromDatabase();

        if (!databaseCameras.empty()) {
            LOG_INFO() << "[Main] Found " << databaseCameras.size() << " cameras in database";

            // Convert CameraConfig to VideoSource
            for (const auto& camConfig : databaseCameras) {
                if (!camConfig.enabled) {
                    LOG_INFO() << "[Main] Skipping disabled camera: " << camConfig.id;
                    continue;
                }

                VideoSource camera;
                camera.id = camConfig.id;
                camera.name = camConfig.name;
                camera.url = camConfig.rtsp_url;
                camera.protocol = "rtsp";
                camera.width = camConfig.stream_config.max_width;
                camera.height = camConfig.stream_config.max_height;
                camera.fps = camConfig.stream_config.fps;
                camera.mjpeg_port = camConfig.mjpeg_port;
                camera.enabled = camConfig.enabled;

                cameras.push_back(camera);

                LOG_INFO() << "[Main] Configured camera from database: " << camera.id
                          << " -> MJPEG port: " << camConfig.mjpeg_port;
            }

        } else if (!configFile.empty()) {
            LOG_INFO() << "[Main] No cameras in database, loading from config file: " << configFile;
            auto cameraConfigs = loadCameraConfig(configFile);

            if (cameraConfigs.empty()) {
                LOG_WARN() << "[Main] No cameras loaded from config file";
            } else {
                // Convert CameraConfig to VideoSource
                for (const auto& camConfig : cameraConfigs) {
                    if (!camConfig.enabled) {
                        LOG_INFO() << "[Main] Skipping disabled camera: " << camConfig.id;
                        continue;
                    }

                    VideoSource camera;
                    camera.id = camConfig.id;
                    camera.name = camConfig.name;
                    camera.url = camConfig.rtsp_url;
                    camera.protocol = "rtsp";
                    camera.width = camConfig.stream_config.max_width;
                    camera.height = camConfig.stream_config.max_height;
                    camera.fps = camConfig.stream_config.fps;
                    camera.mjpeg_port = camConfig.mjpeg_port;
                    camera.enabled = camConfig.enabled;

                    cameras.push_back(camera);

                    LOG_INFO() << "[Main] Configured camera from file: " << camera.id
                              << " -> MJPEG port: " << camConfig.mjpeg_port;
                }
            }
        } else {
            LOG_INFO() << "[Main] No cameras configured in database or config file";
        }

        // Add cameras to TaskManager
        if (!cameras.empty()) {
            for (const auto& camera : cameras) {
                LOG_INFO() << "[Main] Adding camera: " << camera.id << " (" << camera.url << ")";

                if (taskManager.addVideoSource(camera)) {
                    LOG_INFO() << "[Main] Camera added successfully: " << camera.id;

                    // Configure optimized detection if enabled in system config
                    if (systemConfig.optimized_detection) {
                        auto pipeline = taskManager.getPipeline(camera.id);
                        if (pipeline) {
                            pipeline->setOptimizedDetectionEnabled(true);
                            pipeline->setDetectionThreads(systemConfig.detection_threads);
                            LOG_INFO() << "[Main] Optimized detection enabled for " << camera.id
                                      << " with " << systemConfig.detection_threads << " threads";
                        }
                    }

                    // Load person statistics configuration from database
                    loadPersonStatsConfig(camera.id, taskManager);
                } else {
                    LOG_ERROR() << "[Main] Failed to add camera: " << camera.id;
                }
            }
        } else {
            LOG_INFO() << "[Main] No cameras configured. System running in API-only mode.";
        }

        LOG_INFO() << "[Main] System started successfully!";
        LOG_INFO() << "[Main] API endpoints available at http://localhost:" << apiPort;

        // Display MJPEG stream URLs if cameras are configured
        if (!cameras.empty()) {
            LOG_INFO() << "\n[Main] === MJPEG Video Streams ===";
            auto activePipelines = taskManager.getActivePipelines();
            for (const auto& pipelineId : activePipelines) {
                auto pipeline = taskManager.getPipeline(pipelineId);
                if (pipeline) {
                    LOG_INFO() << "[Main] 📺 " << pipelineId << ": " << pipeline->getStreamUrl();
                }
            }
            LOG_INFO() << "[Main] ================================";
        }

        LOG_INFO() << "[Main] Press Ctrl+C to stop...";

        // Main loop
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Print system status using database-configured interval
            static int statusCounter = 0;
            int statusInterval = systemConfig.status_interval;

            if (++statusCounter >= statusInterval) {
                statusCounter = 0;

                auto activePipelines = taskManager.getActivePipelines();
                LOG_INFO() << "\n[Main] === System Status ===";
                LOG_INFO() << "🖥️  Active Pipelines: " << activePipelines.size();
                LOG_INFO() << "🖥️  CPU Usage: " << taskManager.getCpuUsage() << "%";
                LOG_INFO() << "🎮 GPU Memory: " << taskManager.getGpuMemoryUsage();

                if (verbose || systemConfig.optimized_detection) {
                    for (const auto& pipelineId : activePipelines) {
                        auto pipeline = taskManager.getPipeline(pipelineId);
                        if (pipeline) {
                            LOG_INFO() << "🎥 Pipeline " << pipelineId << ":";
                            LOG_INFO() << "  📈 FPS: " << std::fixed << std::setprecision(1)
                                      << pipeline->getFrameRate();
                            LOG_INFO() << "  🎯 Processed: " << pipeline->getProcessedFrames() << " frames";
                            LOG_INFO() << "  ❌ Dropped: " << pipeline->getDroppedFrames() << " frames";
                            LOG_INFO() << "  🧠 Optimized: " << (pipeline->isOptimizedDetectionEnabled() ? "Yes" : "No");
                            if (pipeline->isOptimizedDetectionEnabled()) {
                                LOG_INFO() << "  🔄 Threads: " << pipeline->getDetectionThreads();
                            }
                            LOG_INFO() << "  🌐 Stream: " << pipeline->getStreamUrl();
                            LOG_INFO() << "  👥 Clients: " << pipeline->getConnectedClients();
                            LOG_INFO() << "  ❤️  Healthy: " << (pipeline->isHealthy() ? "Yes" : "No");

                            if (!pipeline->getLastError().empty()) {
                                LOG_ERROR() << "  ⚠️  Last Error: " << pipeline->getLastError();
                            }
                            LOG_INFO() << std::endl;
                        }
                    }
                }
                LOG_INFO() << "================================";
            }
        }

        // Graceful shutdown
        LOG_INFO() << "[Main] Shutting down...";

        // Stop API service first
        LOG_INFO() << "[Main] Stopping API service...";
        apiService.stop();

        // Stop task manager and all pipelines
        LOG_INFO() << "[Main] Stopping task manager...";
        taskManager.stop();

        LOG_INFO() << "[Main] Shutdown complete";
        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[Main] Fatal error: " << e.what();
        return 1;
    } catch (...) {
        LOG_ERROR() << "[Main] Unknown fatal error occurred";
        return 1;
    }
}
