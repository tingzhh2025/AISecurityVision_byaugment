#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

// Forward declarations
class VideoPipeline;
struct VideoSource;

/**
 * @brief Singleton TaskManager for managing multiple VideoPipeline instances
 * 
 * This class manages concurrent video stream processing pipelines,
 * handles video source addition/removal, and coordinates system resources.
 * Thread-safe implementation using mutex protection.
 */
class TaskManager {
public:
    // Singleton access
    static TaskManager& getInstance();
    
    // Delete copy constructor and assignment operator
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    
    // Destructor
    ~TaskManager();
    
    // Pipeline management
    bool addVideoSource(const VideoSource& source);
    bool removeVideoSource(const std::string& sourceId);
    std::vector<std::string> getActivePipelines() const;
    
    // System control
    void start();
    void stop();
    bool isRunning() const;
    
    // Statistics
    size_t getActivePipelineCount() const;
    double getCpuUsage() const;
    std::string getGpuMemoryUsage() const;
    
    // Pipeline access
    std::shared_ptr<VideoPipeline> getPipeline(const std::string& sourceId) const;

private:
    // Private constructor for singleton
    TaskManager();
    
    // Internal methods
    void monitoringThread();
    void cleanupPipeline(const std::string& sourceId);
    
    // Member variables
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<VideoPipeline>> m_pipelines;
    std::atomic<bool> m_running{false};
    std::thread m_monitoringThread;
    
    // System metrics
    mutable std::atomic<double> m_cpuUsage{0.0};
    mutable std::string m_gpuMemUsage;
    
    // Configuration
    static constexpr size_t MAX_PIPELINES = 16;
    static constexpr int MONITORING_INTERVAL_MS = 1000;
};

/**
 * @brief Video source configuration structure
 */
struct VideoSource {
    std::string id;
    std::string url;
    std::string protocol; // "rtsp", "onvif", "gb28181"
    std::string username;
    std::string password;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    bool enabled = true;
    
    // Validation
    bool isValid() const;
    std::string toString() const;
};
