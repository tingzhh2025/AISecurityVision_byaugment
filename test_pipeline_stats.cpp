#include <iostream>
#include <chrono>
#include <thread>
#include "src/core/TaskManager.h"
#include "src/core/VideoPipeline.h"

void printPipelineStats(const TaskManager::PipelineStats& stats) {
    std::cout << "Pipeline Stats for: " << stats.sourceId << std::endl;
    std::cout << "  Protocol: " << stats.protocol << std::endl;
    std::cout << "  URL: " << stats.url << std::endl;
    std::cout << "  Running: " << (stats.isRunning ? "Yes" : "No") << std::endl;
    std::cout << "  Healthy: " << (stats.isHealthy ? "Yes" : "No") << std::endl;
    std::cout << "  Frame Rate: " << stats.frameRate << " fps" << std::endl;
    std::cout << "  Processed Frames: " << stats.processedFrames << std::endl;
    std::cout << "  Dropped Frames: " << stats.droppedFrames << std::endl;
    std::cout << "  Uptime: " << stats.uptime << " seconds" << std::endl;
    std::cout << "  Last Error: " << stats.lastError << std::endl;
    std::cout << std::endl;
}

void printSystemStats(const TaskManager::SystemStats& stats) {
    std::cout << "=== System Statistics ===" << std::endl;
    std::cout << "Total Pipelines: " << stats.totalPipelines << std::endl;
    std::cout << "Running Pipelines: " << stats.runningPipelines << std::endl;
    std::cout << "Healthy Pipelines: " << stats.healthyPipelines << std::endl;
    std::cout << "Total Frame Rate: " << stats.totalFrameRate << " fps" << std::endl;
    std::cout << "Total Processed Frames: " << stats.totalProcessedFrames << std::endl;
    std::cout << "Total Dropped Frames: " << stats.totalDroppedFrames << std::endl;
    std::cout << "System Uptime: " << stats.systemUptime << " seconds" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Resource Usage ===" << std::endl;
    std::cout << "CPU Usage: " << stats.cpuUsage << "%" << std::endl;
    std::cout << "GPU Memory: " << stats.gpuMemUsage << std::endl;
    std::cout << "GPU Utilization: " << stats.gpuUtilization << "%" << std::endl;
    std::cout << "GPU Temperature: " << stats.gpuTemperature << "°C" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Pipeline Statistics Test ===" << std::endl;
    
    // Get TaskManager instance
    TaskManager& taskManager = TaskManager::getInstance();
    
    // Start the task manager
    taskManager.start();
    
    // Create test video sources
    VideoSource source1;
    source1.id = "test_camera_1";
    source1.protocol = "rtsp";
    source1.url = "rtsp://test.example.com/stream1";
    source1.width = 1920;
    source1.height = 1080;
    source1.fps = 30;
    source1.enabled = true;
    
    VideoSource source2;
    source2.id = "test_camera_2";
    source2.protocol = "rtsp";
    source2.url = "rtsp://test.example.com/stream2";
    source2.width = 1280;
    source2.height = 720;
    source2.fps = 25;
    source2.enabled = true;
    
    std::cout << "Adding test video sources..." << std::endl;
    
    // Add video sources (these will fail to initialize but will create pipeline objects)
    bool added1 = taskManager.addVideoSource(source1);
    bool added2 = taskManager.addVideoSource(source2);
    
    std::cout << "Source 1 added: " << (added1 ? "Success" : "Failed") << std::endl;
    std::cout << "Source 2 added: " << (added2 ? "Success" : "Failed") << std::endl;
    std::cout << std::endl;
    
    // Wait a moment for initialization
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test individual pipeline statistics
    std::cout << "=== Individual Pipeline Statistics ===" << std::endl;
    auto stats1 = taskManager.getPipelineStats("test_camera_1");
    auto stats2 = taskManager.getPipelineStats("test_camera_2");
    
    if (!stats1.sourceId.empty()) {
        printPipelineStats(stats1);
    } else {
        std::cout << "No stats available for test_camera_1" << std::endl;
    }
    
    if (!stats2.sourceId.empty()) {
        printPipelineStats(stats2);
    } else {
        std::cout << "No stats available for test_camera_2" << std::endl;
    }
    
    // Test all pipeline statistics
    std::cout << "=== All Pipeline Statistics ===" << std::endl;
    auto allStats = taskManager.getAllPipelineStats();
    
    std::cout << "Found " << allStats.size() << " pipelines:" << std::endl;
    for (const auto& stats : allStats) {
        printPipelineStats(stats);
    }
    
    // Test system statistics
    auto systemStats = taskManager.getSystemStats();
    printSystemStats(systemStats);
    
    // Test statistics over time
    std::cout << "=== Monitoring Statistics Over Time ===" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto currentStats = taskManager.getSystemStats();
        std::cout << "Time " << (i + 1) << ":" << std::endl;
        std::cout << "  CPU Usage: " << currentStats.cpuUsage << "%" << std::endl;
        std::cout << "  GPU Utilization: " << currentStats.gpuUtilization << "%" << std::endl;
        std::cout << "  System Uptime: " << currentStats.systemUptime << "s" << std::endl;
        std::cout << "  Active Pipelines: " << currentStats.runningPipelines << "/" << currentStats.totalPipelines << std::endl;
    }
    
    // Cleanup
    std::cout << std::endl << "Cleaning up..." << std::endl;
    taskManager.removeVideoSource("test_camera_1");
    taskManager.removeVideoSource("test_camera_2");
    taskManager.stop();
    
    std::cout << "Pipeline statistics test completed!" << std::endl;
    return 0;
}
