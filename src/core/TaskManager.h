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

// VideoSource is now defined in VideoPipeline.h
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
    double getGpuUtilization() const;
    double getGpuTemperature() const;

    // Enhanced pipeline statistics
    struct PipelineStats {
        std::string sourceId;
        std::string protocol;
        std::string url;
        bool isRunning;
        bool isHealthy;
        double frameRate;
        size_t processedFrames;
        size_t droppedFrames;
        std::string lastError;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastFrameTime;
        double uptime; // in seconds
    };

    std::vector<PipelineStats> getAllPipelineStats() const;
    PipelineStats getPipelineStats(const std::string& sourceId) const;

    // System-wide statistics
    struct SystemStats {
        size_t totalPipelines;
        size_t runningPipelines;
        size_t healthyPipelines;
        double totalFrameRate;
        size_t totalProcessedFrames;
        size_t totalDroppedFrames;
        double cpuUsage;
        std::string gpuMemUsage;
        double gpuUtilization;
        double gpuTemperature;
        std::chrono::steady_clock::time_point systemStartTime;
        double systemUptime; // in seconds
    };

    SystemStats getSystemStats() const;

    // Monitoring performance metrics
    uint64_t getMonitoringCycles() const;
    double getAverageMonitoringTime() const;
    double getMaxMonitoringTime() const;
    bool isMonitoringHealthy() const;
    void resetMonitoringStats();

    // Pipeline access
    std::shared_ptr<VideoPipeline> getPipeline(const std::string& sourceId) const;

    // Configuration constants
    static constexpr size_t MAX_PIPELINES = 16;
    static constexpr int MONITORING_INTERVAL_MS = 1000;

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
    std::chrono::steady_clock::time_point m_systemStartTime;

    // System metrics
    mutable std::atomic<double> m_cpuUsage{0.0};
    mutable std::string m_gpuMemUsage;
    mutable std::atomic<double> m_gpuUtilization{0.0};
    mutable std::atomic<double> m_gpuTemperature{0.0};

    // CPU monitoring state
    struct CpuStats {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        unsigned long long total() const {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }
        unsigned long long active() const {
            return user + nice + system + irq + softirq + steal;
        }
    };
    mutable CpuStats m_lastCpuStats{};
    mutable bool m_cpuStatsInitialized{false};

    // Internal monitoring methods
    bool readCpuStats(CpuStats& stats) const;
    double calculateCpuUsage(const CpuStats& prev, const CpuStats& curr) const;

    // GPU monitoring methods
    bool initializeGpuMonitoring();
    void cleanupGpuMonitoring();
    bool updateGpuMetrics();

    // GPU monitoring state
    mutable bool m_nvmlInitialized{false};
    mutable unsigned int m_gpuDeviceCount{0};
    mutable void* m_gpuDevice{nullptr}; // nvmlDevice_t handle

    // Enhanced monitoring metrics
    mutable std::atomic<uint64_t> m_monitoringCycles{0};
    mutable std::atomic<double> m_avgMonitoringTime{0.0};
    mutable std::atomic<double> m_maxMonitoringTime{0.0};
    mutable std::chrono::steady_clock::time_point m_lastMonitoringTime;
    mutable std::atomic<bool> m_monitoringHealthy{true};
};

// VideoSource is now defined in VideoPipeline.h
