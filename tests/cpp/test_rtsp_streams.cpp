/**
 * @file test_rtsp_streams.cpp
 * @brief Test program for RTSP streams with AI Security Vision System
 *
 * This program tests the AI vision system with specific RTSP streams.
 */

#include "src/core/TaskManager.h"
#include "src/core/VideoPipeline.h"
#include "src/api/APIService.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[TestRTSP] Received signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "Options:\n"
              << "  -h, --help     Show this help message\n"
              << "  -p, --port     API server port (default: 8080)\n"
              << "  -v, --verbose  Enable verbose logging\n"
              << "  -t, --time     Test duration in seconds (default: 60)\n"
              << "\nThis program tests the AI vision system with RTSP streams:\n"
              << "  - rtsp://admin:sharpi1688@192.168.1.2:554/1/1\n"
              << "  - rtsp://admin:sharpi1688@192.168.1.3:554/1/1\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== RTSP Stream Test for AI Security Vision ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "===============================================" << std::endl;

    // Parse command line arguments
    int apiPort = 8080;
    bool verbose = false;
    int testDuration = 60;

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
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-t" || arg == "--time") {
            if (i + 1 < argc) {
                testDuration = std::atoi(argv[++i]);
            } else {
                std::cerr << "Error: Test duration required" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        // Initialize TaskManager
        std::cout << "[TestRTSP] Initializing TaskManager..." << std::endl;
        TaskManager& taskManager = TaskManager::getInstance();
        taskManager.start();

        // Initialize API Service
        std::cout << "[TestRTSP] Starting API service on port " << apiPort << "..." << std::endl;
        APIService apiService(apiPort);
        if (!apiService.start()) {
            std::cerr << "[TestRTSP] Failed to start API service" << std::endl;
            return 1;
        }

        // Add RTSP video sources
        std::cout << "[TestRTSP] Adding RTSP video sources..." << std::endl;

        // Camera 1: 192.168.1.2
        VideoSource camera1;
        camera1.id = "camera_192_168_1_2";
        camera1.url = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
        camera1.protocol = "rtsp";
        camera1.username = "admin";
        camera1.password = "sharpi1688";
        camera1.width = 1920;
        camera1.height = 1080;
        camera1.fps = 25;
        camera1.enabled = true;

        if (taskManager.addVideoSource(camera1)) {
            std::cout << "[TestRTSP] Camera 1 (192.168.1.2) added successfully" << std::endl;
        } else {
            std::cout << "[TestRTSP] Failed to add Camera 1 (192.168.1.2)" << std::endl;
        }

        // Camera 2: 192.168.1.3
        VideoSource camera2;
        camera2.id = "camera_192_168_1_3";
        camera2.url = "rtsp://admin:sharpi1688@192.168.1.3:554/1/1";
        camera2.protocol = "rtsp";
        camera2.username = "admin";
        camera2.password = "sharpi1688";
        camera2.width = 1920;
        camera2.height = 1080;
        camera2.fps = 25;
        camera2.enabled = true;

        if (taskManager.addVideoSource(camera2)) {
            std::cout << "[TestRTSP] Camera 2 (192.168.1.3) added successfully" << std::endl;
        } else {
            std::cout << "[TestRTSP] Failed to add Camera 2 (192.168.1.3)" << std::endl;
        }

        std::cout << "[TestRTSP] System started successfully!" << std::endl;
        std::cout << "[TestRTSP] API endpoints available at http://localhost:" << apiPort << std::endl;
        std::cout << "[TestRTSP] Test will run for " << testDuration << " seconds..." << std::endl;
        std::cout << "[TestRTSP] Press Ctrl+C to stop early..." << std::endl;

        // Main test loop
        auto startTime = std::chrono::steady_clock::now();
        int statusCounter = 0;

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Check if test duration has elapsed
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

            if (elapsed >= testDuration) {
                std::cout << "[TestRTSP] Test duration completed (" << testDuration << " seconds)" << std::endl;
                break;
            }

            // Print system status every 10 seconds
            if (++statusCounter >= 10) {
                statusCounter = 0;

                auto activePipelines = taskManager.getActivePipelines();
                std::cout << "[TestRTSP] Status (" << elapsed << "s): " << activePipelines.size()
                          << " active pipelines, CPU: " << taskManager.getCpuUsage()
                          << "%, GPU: " << taskManager.getGpuMemoryUsage() << std::endl;

                if (verbose) {
                    for (const auto& pipelineId : activePipelines) {
                        auto pipeline = taskManager.getPipeline(pipelineId);
                        if (pipeline) {
                            std::cout << "  Pipeline " << pipelineId
                                      << ": " << pipeline->getFrameRate() << " FPS, "
                                      << pipeline->getProcessedFrames() << " frames processed"
                                      << std::endl;
                        }
                    }
                }
            }
        }

        // Print final statistics
        auto activePipelines = taskManager.getActivePipelines();
        std::cout << "\n[TestRTSP] Final Statistics:" << std::endl;
        std::cout << "  Active pipelines: " << activePipelines.size() << std::endl;

        for (const auto& pipelineId : activePipelines) {
            auto pipeline = taskManager.getPipeline(pipelineId);
            if (pipeline) {
                std::cout << "  Pipeline " << pipelineId << ":" << std::endl;
                std::cout << "    Frame rate: " << pipeline->getFrameRate() << " FPS" << std::endl;
                std::cout << "    Processed frames: " << pipeline->getProcessedFrames() << std::endl;
                std::cout << "    Dropped frames: " << pipeline->getDroppedFrames() << std::endl;
            }
        }

        // Graceful shutdown
        std::cout << "[TestRTSP] Shutting down..." << std::endl;

        apiService.stop();
        taskManager.stop();

        std::cout << "[TestRTSP] Test completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[TestRTSP] Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[TestRTSP] Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
