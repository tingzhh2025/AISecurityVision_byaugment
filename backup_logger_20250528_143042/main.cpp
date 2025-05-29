#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include "core/TaskManager.h"
#include "core/VideoPipeline.h"
#include "api/APIService.h"

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -p, --port       API server port (default: 8080)\n"
              << "  -c, --config     Configuration file path\n"
              << "  -v, --verbose    Enable verbose logging\n"
              << "  --test           Run in test mode with sample video\n"
              << "  --optimized      Run with optimized multi-threaded RKNN detection\n"
              << "  --cameras        Use real RTSP cameras for testing\n"
              << "  --threads N      Number of detection threads (default: 3)\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== AI Security Vision System ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "===================================" << std::endl;

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
                std::cerr << "Error: Port number required" << std::endl;
                return 1;
            }
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: Config file path required" << std::endl;
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
                    std::cerr << "Error: Detection threads must be between 1 and 8" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Number of threads required" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Initialize TaskManager
        std::cout << "[Main] Initializing TaskManager..." << std::endl;
        TaskManager& taskManager = TaskManager::getInstance();
        taskManager.start();

        // Initialize API Service
        std::cout << "[Main] Starting API service on port " << apiPort << "..." << std::endl;
        APIService apiService(apiPort);
        if (!apiService.start()) {
            std::cerr << "[Main] Failed to start API service" << std::endl;
            return 1;
        }

        // Test mode: Add video sources
        if (testMode || useRealCameras) {
            if (useRealCameras) {
                std::cout << "[Main] Running with real RTSP cameras..." << std::endl;
                if (optimizedMode) {
                    std::cout << "[Main] Using optimized multi-threaded RKNN detection with "
                              << detectionThreads << " threads" << std::endl;
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
                    std::cout << "[Main] Adding camera: " << camera.id << " (" << camera.url << ")" << std::endl;

                    if (taskManager.addVideoSource(camera)) {
                        std::cout << "[Main] Camera added successfully: " << camera.id << std::endl;

                        // Configure optimized detection if enabled
                        if (optimizedMode) {
                            auto pipeline = taskManager.getPipeline(camera.id);
                            if (pipeline) {
                                pipeline->setOptimizedDetectionEnabled(true);
                                pipeline->setDetectionThreads(detectionThreads);
                                std::cout << "[Main] Optimized detection enabled for " << camera.id
                                          << " with " << detectionThreads << " threads" << std::endl;
                            }
                        }
                    } else {
                        std::cout << "[Main] Failed to add camera: " << camera.id << std::endl;
                    }
                }

            } else {
                std::cout << "[Main] Running in test mode..." << std::endl;

                VideoSource testSource;
                testSource.id = "test_camera_01";
                testSource.url = "rtsp://admin:admin123@192.168.1.100:554/stream1";
                testSource.protocol = "rtsp";
                testSource.width = 1920;
                testSource.height = 1080;
                testSource.fps = 25;
                testSource.enabled = true;

                if (taskManager.addVideoSource(testSource)) {
                    std::cout << "[Main] Test video source added successfully" << std::endl;
                } else {
                    std::cout << "[Main] Failed to add test video source" << std::endl;
                }
            }
        }

        std::cout << "[Main] System started successfully!" << std::endl;
        std::cout << "[Main] API endpoints available at http://localhost:" << apiPort << std::endl;

        // Display MJPEG stream URLs
        if (useRealCameras || testMode) {
            std::cout << "\n[Main] === MJPEG Video Streams ===" << std::endl;
            auto activePipelines = taskManager.getActivePipelines();
            for (const auto& pipelineId : activePipelines) {
                auto pipeline = taskManager.getPipeline(pipelineId);
                if (pipeline) {
                    std::cout << "[Main] 📺 " << pipelineId << ": " << pipeline->getStreamUrl() << std::endl;
                }
            }
            std::cout << "[Main] ================================" << std::endl;
        }

        std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;

        // Main loop
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Print system status (more frequently for camera mode)
            static int statusCounter = 0;
            int statusInterval = (useRealCameras || optimizedMode) ? 10 : 30; // 10s for camera mode, 30s for normal

            if (++statusCounter >= statusInterval) {
                statusCounter = 0;

                auto activePipelines = taskManager.getActivePipelines();
                std::cout << "\n[Main] === System Status ===" << std::endl;
                std::cout << "🖥️  Active Pipelines: " << activePipelines.size() << std::endl;
                std::cout << "🖥️  CPU Usage: " << taskManager.getCpuUsage() << "%" << std::endl;
                std::cout << "🎮 GPU Memory: " << taskManager.getGpuMemoryUsage() << std::endl;

                if (verbose || useRealCameras || optimizedMode) {
                    for (const auto& pipelineId : activePipelines) {
                        auto pipeline = taskManager.getPipeline(pipelineId);
                        if (pipeline) {
                            std::cout << "🎥 Pipeline " << pipelineId << ":" << std::endl;
                            std::cout << "  📈 FPS: " << std::fixed << std::setprecision(1)
                                      << pipeline->getFrameRate() << std::endl;
                            std::cout << "  🎯 Processed: " << pipeline->getProcessedFrames() << " frames" << std::endl;
                            std::cout << "  ❌ Dropped: " << pipeline->getDroppedFrames() << " frames" << std::endl;
                            std::cout << "  🧠 Optimized: " << (pipeline->isOptimizedDetectionEnabled() ? "Yes" : "No") << std::endl;
                            if (pipeline->isOptimizedDetectionEnabled()) {
                                std::cout << "  🔄 Threads: " << pipeline->getDetectionThreads() << std::endl;
                            }
                            std::cout << "  🌐 Stream: " << pipeline->getStreamUrl() << std::endl;
                            std::cout << "  👥 Clients: " << pipeline->getConnectedClients() << std::endl;
                            std::cout << "  ❤️  Healthy: " << (pipeline->isHealthy() ? "Yes" : "No") << std::endl;

                            if (!pipeline->getLastError().empty()) {
                                std::cout << "  ⚠️  Last Error: " << pipeline->getLastError() << std::endl;
                            }
                            std::cout << std::endl;
                        }
                    }
                }
                std::cout << "================================" << std::endl;
            }
        }

        // Graceful shutdown
        std::cout << "[Main] Shutting down..." << std::endl;

        apiService.stop();
        taskManager.stop();

        std::cout << "[Main] Shutdown complete" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Main] Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
