#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include "core/TaskManager.h"
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
              << "  -h, --help     Show this help message\n"
              << "  -p, --port     API server port (default: 8080)\n"
              << "  -c, --config   Configuration file path\n"
              << "  -v, --verbose  Enable verbose logging\n"
              << "  --test         Run in test mode with sample video\n"
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
        
        // Test mode: Add a sample video source
        if (testMode) {
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
        
        std::cout << "[Main] System started successfully!" << std::endl;
        std::cout << "[Main] API endpoints available at http://localhost:" << apiPort << std::endl;
        std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;
        
        // Main loop
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print system status every 30 seconds
            static int statusCounter = 0;
            if (++statusCounter >= 30) {
                statusCounter = 0;
                
                auto activePipelines = taskManager.getActivePipelines();
                std::cout << "[Main] Status: " << activePipelines.size() 
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
