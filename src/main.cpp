#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include "core/TaskManager.h"
#include "core/VideoPipeline.h"
#include "api/APIService.h"

#include "core/Logger.h"
using namespace AISecurityVision;
// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    LOG_INFO() << "\n[Main] Received signal " << signal << ", shutting down...";
    g_running.store(false);
}

void printUsage(const char* programName) {
    LOG_INFO() << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -p, --port       API server port (default: 8080)\n"
              << "  -c, --config     Configuration file path\n"
              << "  -v, --verbose    Enable verbose logging\n"
              << "  --test           Run in test mode with sample video\n"
              << "  --optimized      Run with optimized multi-threaded RKNN detection\n"
              << "  --cameras        Use real RTSP cameras for testing\n"
              << "  --threads N      Number of detection threads (default: 3)\n";
}

int main(int argc, char* argv[]) {
    LOG_INFO() << "=== AI Security Vision System ===";
    LOG_INFO() << "Version: 1.0.0";
    LOG_INFO() << "Build: " << __DATE__ << " " << __TIME__;
    LOG_INFO() << "===================================";

    // Parse command line arguments
    int apiPort = 8080;
    std::string configFile;
    bool verbose = false;
    bool testMode = false;
    bool optimizedMode = false;
    bool useRealCameras = false;
    int detectionThreads = 3;

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
            verbose = true;
        } else if (arg == "--test") {
            testMode = true;
        } else if (arg == "--optimized") {
            optimizedMode = true;
        } else if (arg == "--cameras") {
            useRealCameras = true;
        } else if (arg == "--threads") {
            if (i + 1 < argc) {
                detectionThreads = std::atoi(argv[++i]);
                if (detectionThreads < 1 || detectionThreads > 8) {
                    LOG_ERROR() << "Error: Detection threads must be between 1 and 8";
                    return 1;
                }
            } else {
                LOG_ERROR() << "Error: Number of threads required";
                return 1;
            }
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
        // Initialize TaskManager
        LOG_INFO() << "[Main] Initializing TaskManager...";
        TaskManager& taskManager = TaskManager::getInstance();
        taskManager.start();

        // Initialize API Service
        LOG_INFO() << "[Main] Starting API service on port " << apiPort << "...";
        APIService apiService(apiPort);
        if (!apiService.start()) {
            LOG_ERROR() << "[Main] Failed to start API service";
            return 1;
        }

        // Test mode: Add video sources
        if (testMode || useRealCameras) {
            if (useRealCameras) {
                LOG_INFO() << "[Main] Running with real RTSP cameras...";
                if (optimizedMode) {
                    LOG_INFO() << "[Main] Using optimized multi-threaded RKNN detection with "
                              << detectionThreads << " threads";
                }

                // Add real RTSP cameras
                std::vector<VideoSource> cameras;

                // Camera 1
                VideoSource camera1;
                camera1.id = "camera_01";
                camera1.name = "Security Camera 1";
                camera1.url = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
                camera1.protocol = "rtsp";
                camera1.width = 1920;
                camera1.height = 1080;
                camera1.fps = 25;
                camera1.enabled = true;
                cameras.push_back(camera1);

                // Camera 2
                VideoSource camera2;
                camera2.id = "camera_02";
                camera2.name = "Security Camera 2";
                camera2.url = "rtsp://admin:sharpi1688@192.168.1.3:554/1/1";
                camera2.protocol = "rtsp";
                camera2.width = 1920;
                camera2.height = 1080;
                camera2.fps = 25;
                camera2.enabled = true;
                cameras.push_back(camera2);

                // Add cameras to TaskManager
                for (const auto& camera : cameras) {
                    LOG_INFO() << "[Main] Adding camera: " << camera.id << " (" << camera.url << ")";

                    if (taskManager.addVideoSource(camera)) {
                        LOG_INFO() << "[Main] Camera added successfully: " << camera.id;

                        // Configure optimized detection if enabled
                        if (optimizedMode) {
                            auto pipeline = taskManager.getPipeline(camera.id);
                            if (pipeline) {
                                pipeline->setOptimizedDetectionEnabled(true);
                                pipeline->setDetectionThreads(detectionThreads);
                                LOG_INFO() << "[Main] Optimized detection enabled for " << camera.id
                                          << " with " << detectionThreads << " threads";
                            }
                        }
                    } else {
                        LOG_ERROR() << "[Main] Failed to add camera: " << camera.id;
                    }
                }

            } else {
                LOG_INFO() << "[Main] Running in test mode...";

                VideoSource testSource;
                testSource.id = "test_camera_01";
                testSource.url = "rtsp://admin:admin123@192.168.1.100:554/stream1";
                testSource.protocol = "rtsp";
                testSource.width = 1920;
                testSource.height = 1080;
                testSource.fps = 25;
                testSource.enabled = true;

                if (taskManager.addVideoSource(testSource)) {
                    LOG_INFO() << "[Main] Test video source added successfully";
                } else {
                    LOG_ERROR() << "[Main] Failed to add test video source";
                }
            }
        }

        LOG_INFO() << "[Main] System started successfully!";
        LOG_INFO() << "[Main] API endpoints available at http://localhost:" << apiPort;

        // Display MJPEG stream URLs
        if (useRealCameras || testMode) {
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

            // Print system status (more frequently for camera mode)
            static int statusCounter = 0;
            int statusInterval = (useRealCameras || optimizedMode) ? 10 : 30; // 10s for camera mode, 30s for normal

            if (++statusCounter >= statusInterval) {
                statusCounter = 0;

                auto activePipelines = taskManager.getActivePipelines();
                LOG_INFO() << "\n[Main] === System Status ===";
                LOG_INFO() << "🖥️  Active Pipelines: " << activePipelines.size();
                LOG_INFO() << "🖥️  CPU Usage: " << taskManager.getCpuUsage() << "%";
                LOG_INFO() << "🎮 GPU Memory: " << taskManager.getGpuMemoryUsage();

                if (verbose || useRealCameras || optimizedMode) {
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

        apiService.stop();
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
