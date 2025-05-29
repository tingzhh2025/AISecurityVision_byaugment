#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <iomanip>

// Include core components
#include "../src/core/VideoPipeline.h"
#include "../src/core/TaskManager.h"

// Global flag for graceful shutdown
volatile bool g_running = true;

void signalHandler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;
}

int main() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "🎉 === INTEGRATED OPTIMIZED AI VISION SYSTEM TEST ===" << std::endl;
    std::cout << "🧠 Multi-threaded RKNN YOLOv8 + MJPEG streaming with detection overlays" << std::endl;
    std::cout << "🎯 Testing full pipeline integration" << std::endl;

    try {
        // Initialize TaskManager
        std::cout << "\n[Main] Initializing TaskManager..." << std::endl;
        TaskManager& taskManager = TaskManager::getInstance();
        taskManager.start();

        // Create test video sources
        std::vector<VideoSource> testSources;

        // Camera 1
        VideoSource camera1;
        camera1.id = "camera_01";
        camera1.name = "Test Camera 1";
        camera1.url = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
        camera1.protocol = "rtsp";
        camera1.width = 1920;
        camera1.height = 1080;
        camera1.fps = 25;
        camera1.enabled = true;
        testSources.push_back(camera1);

        // Camera 2
        VideoSource camera2;
        camera2.id = "camera_02";
        camera2.name = "Test Camera 2";
        camera2.url = "rtsp://admin:sharpi1688@192.168.1.3:554/1/1";
        camera2.protocol = "rtsp";
        camera2.width = 1920;
        camera2.height = 1080;
        camera2.fps = 25;
        camera2.enabled = true;
        testSources.push_back(camera2);

        // Add video sources to TaskManager
        std::vector<std::string> pipelineIds;
        for (const auto& source : testSources) {
            std::cout << "\n[Main] Adding video source: " << source.id << std::endl;
            std::cout << "  URL: " << source.url << std::endl;

            if (taskManager.addVideoSource(source)) {
                std::cout << "✅ Video source added successfully: " << source.id << std::endl;
                pipelineIds.push_back(source.id);

                // Get the pipeline and configure it for optimized detection
                auto pipeline = taskManager.getPipeline(source.id);
                if (pipeline) {
                    // Enable optimized detection with 3 threads (for 3 NPU cores)
                    pipeline->setOptimizedDetectionEnabled(true);
                    pipeline->setDetectionThreads(3);
                    pipeline->setStreamingEnabled(true);

                    std::cout << "🧠 Optimized RKNN detection enabled with 3 threads" << std::endl;
                    std::cout << "🌐 MJPEG streaming enabled" << std::endl;
                }
            } else {
                std::cout << "❌ Failed to add video source: " << source.id << std::endl;
            }
        }

        if (pipelineIds.empty()) {
            std::cout << "❌ No pipelines created, exiting..." << std::endl;
            return -1;
        }

        std::cout << "\n🎯 === System Status ===" << std::endl;
        std::cout << "✅ " << pipelineIds.size() << " pipelines created and running" << std::endl;
        std::cout << "🧠 Multi-threaded RKNN YOLOv8 detection active" << std::endl;
        std::cout << "🌐 MJPEG streams with detection overlays:" << std::endl;

        // Display stream URLs
        for (const auto& pipelineId : pipelineIds) {
            auto pipeline = taskManager.getPipeline(pipelineId);
            if (pipeline) {
                std::cout << "  📺 " << pipelineId << ": " << pipeline->getStreamUrl() << std::endl;
            }
        }

        std::cout << "\n⏸️  Press Ctrl+C to stop the test..." << std::endl;
        std::cout << "📊 Performance stats will be displayed every 10 seconds..." << std::endl;

        // Main monitoring loop
        auto lastStatsTime = std::chrono::steady_clock::now();
        int statsCounter = 0;

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Print detailed stats every 10 seconds
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - lastStatsTime).count();

            if (elapsed >= 10.0) {
                std::cout << "\n📊 === Performance Statistics ===" << std::endl;

                for (const auto& pipelineId : pipelineIds) {
                    auto pipeline = taskManager.getPipeline(pipelineId);
                    if (pipeline) {
                        std::cout << "🎥 Pipeline " << pipelineId << ":" << std::endl;
                        std::cout << "  📈 FPS: " << std::fixed << std::setprecision(1)
                                  << pipeline->getFrameRate() << std::endl;
                        std::cout << "  🎯 Processed: " << pipeline->getProcessedFrames() << " frames" << std::endl;
                        std::cout << "  ❌ Dropped: " << pipeline->getDroppedFrames() << " frames" << std::endl;
                        std::cout << "  🧠 Optimized: " << (pipeline->isOptimizedDetectionEnabled() ? "Yes" : "No") << std::endl;
                        std::cout << "  🔄 Threads: " << pipeline->getDetectionThreads() << std::endl;
                        std::cout << "  🌐 Stream: " << pipeline->getStreamUrl() << std::endl;
                        std::cout << "  👥 Clients: " << pipeline->getConnectedClients() << std::endl;
                        std::cout << "  ❤️  Healthy: " << (pipeline->isHealthy() ? "Yes" : "No") << std::endl;

                        if (!pipeline->getLastError().empty()) {
                            std::cout << "  ⚠️  Last Error: " << pipeline->getLastError() << std::endl;
                        }
                        std::cout << std::endl;
                    }
                }

                // System-wide stats
                auto activePipelines = taskManager.getActivePipelines();
                std::cout << "🖥️  System CPU: " << taskManager.getCpuUsage() << "%" << std::endl;
                std::cout << "🎮 GPU Memory: " << taskManager.getGpuMemoryUsage() << std::endl;
                std::cout << "🔄 Active Pipelines: " << activePipelines.size() << std::endl;
                std::cout << "================================" << std::endl;

                lastStatsTime = now;
            }
        }

        // Graceful shutdown
        std::cout << "\n🛑 === Shutting Down ===" << std::endl;
        std::cout << "Stopping TaskManager..." << std::endl;
        taskManager.stop();

        std::cout << "✅ Shutdown complete" << std::endl;
        std::cout << "🎯 === Test Completed Successfully ===" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
