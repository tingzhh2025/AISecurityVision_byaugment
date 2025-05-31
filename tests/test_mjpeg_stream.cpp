/**
 * @file test_mjpeg_stream.cpp
 * @brief Simple test program to verify YOLOv8 inference and MJPEG streaming
 * 
 * This program creates a video pipeline with YOLOv8 detection and MJPEG streaming
 * to test the complete inference -> visualization -> streaming workflow.
 */

#include "src/core/VideoPipeline.h"
#include "src/core/Logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize logger
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    
    std::cout << "=== YOLOv8 MJPEG Stream Test ===" << std::endl;
    std::cout << "This test will:" << std::endl;
    std::cout << "1. Create a video pipeline with RTSP input" << std::endl;
    std::cout << "2. Run YOLOv8 RKNN inference on each frame" << std::endl;
    std::cout << "3. Stream processed video via MJPEG on port 8161" << std::endl;
    std::cout << "4. You can view the stream at: http://localhost:8161" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Create video source configuration
    VideoSource source;
    source.id = "test_camera";
    source.name = "Test RTSP Camera";
    source.url = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
    source.protocol = "rtsp";
    source.username = "admin";
    source.password = "sharpi1688";
    source.width = 1920;
    source.height = 1080;
    source.fps = 25;
    source.mjpeg_port = 8161;
    source.enabled = true;
    
    std::cout << "Creating video pipeline for: " << source.url << std::endl;
    
    // Create and initialize pipeline
    auto pipeline = std::make_unique<VideoPipeline>(source);
    
    if (!pipeline->initialize()) {
        std::cerr << "Failed to initialize video pipeline!" << std::endl;
        return -1;
    }
    
    std::cout << "Pipeline initialized successfully!" << std::endl;
    
    // Enable optimized detection (RKNN)
    pipeline->setOptimizedDetectionEnabled(true);
    pipeline->setDetectionEnabled(true);
    pipeline->setStreamingEnabled(true);
    
    std::cout << "Starting video pipeline..." << std::endl;
    pipeline->start();
    
    if (!pipeline->isRunning()) {
        std::cerr << "Failed to start video pipeline!" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Pipeline started successfully!" << std::endl;
    std::cout << "🎥 MJPEG stream available at: http://localhost:8161" << std::endl;
    std::cout << "🤖 YOLOv8 RKNN inference enabled" << std::endl;
    std::cout << "📊 Press Ctrl+C to stop..." << std::endl;
    
    // Main monitoring loop
    auto startTime = std::chrono::steady_clock::now();
    int statusCount = 0;
    
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (!pipeline->isRunning()) {
            std::cerr << "Pipeline stopped unexpectedly!" << std::endl;
            break;
        }
        
        // Print status every 30 seconds
        statusCount++;
        if (statusCount % 6 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            
            std::cout << "\n=== Status Update (Runtime: " << elapsed << "s) ===" << std::endl;
            std::cout << "🎥 Pipeline: " << (pipeline->isRunning() ? "Running" : "Stopped") << std::endl;
            std::cout << "💚 Health: " << (pipeline->isHealthy() ? "Healthy" : "Unhealthy") << std::endl;
            std::cout << "📈 Frame Rate: " << std::fixed << std::setprecision(1) << pipeline->getFrameRate() << " FPS" << std::endl;
            std::cout << "📊 Processed Frames: " << pipeline->getProcessedFrames() << std::endl;
            std::cout << "❌ Dropped Frames: " << pipeline->getDroppedFrames() << std::endl;
            std::cout << "🌐 Stream URL: http://localhost:8161" << std::endl;
            
            if (!pipeline->getLastError().empty()) {
                std::cout << "⚠️  Last Error: " << pipeline->getLastError() << std::endl;
            }
            
            std::cout << "=================================" << std::endl;
        }
    }
    
    std::cout << "\nStopping pipeline..." << std::endl;
    pipeline->stop();
    
    std::cout << "Pipeline stopped. Final statistics:" << std::endl;
    std::cout << "📊 Total Processed Frames: " << pipeline->getProcessedFrames() << std::endl;
    std::cout << "❌ Total Dropped Frames: " << pipeline->getDroppedFrames() << std::endl;
    std::cout << "📈 Average Frame Rate: " << std::fixed << std::setprecision(1) << pipeline->getFrameRate() << " FPS" << std::endl;
    
    std::cout << "Test completed!" << std::endl;
    return 0;
}
