#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>

// Forward declarations
class VideoPipeline;

// VideoSource is now defined in VideoPipeline.h
struct VideoSource;

// Forward declarations for cross-camera tracking
struct CrossCameraTrack;
struct ReIDMatch;

/**
 * @brief Cross-camera track structure for global tracking
 */
struct CrossCameraTrack {
    int globalTrackId;                              // Global unique track ID
    std::string primaryCameraId;                    // Primary camera that first detected this track
    std::vector<float> reidFeatures;                // ReID feature vector
    std::unordered_map<std::string, int> localTrackIds; // Local track IDs per camera
    std::chrono::steady_clock::time_point lastSeen; // Last time this track was updated
    std::chrono::steady_clock::time_point firstSeen; // First detection time
    cv::Rect lastBbox;                              // Last known bounding box
    int classId;                                    // Object class
    float confidence;                               // Last confidence score
    bool isActive;                                  // Whether track is currently active

    CrossCameraTrack(int globalId, const std::string& cameraId, int localId,
                    const std::vector<float>& features, const cv::Rect& bbox,
                    int cls, float conf);

    void updateTrack(const std::string& cameraId, int localId,
                    const std::vector<float>& features, const cv::Rect& bbox,
                    float conf);

    bool hasCamera(const std::string& cameraId) const;
    int getLocalTrackId(const std::string& cameraId) const;
    double getTimeSinceLastSeen() const;
    bool isExpired(double maxAgeSeconds) const;
};

/**
 * @brief ReID matching result structure
 */
struct ReIDMatch {
    int globalTrackId;
    float similarity;
    std::string matchedCameraId;
    int matchedLocalTrackId;

    ReIDMatch(int globalId, float sim, const std::string& cameraId, int localId)
        : globalTrackId(globalId), similarity(sim), matchedCameraId(cameraId), matchedLocalTrackId(localId) {}
};

/**
 * @brief Singleton TaskManager for managing multiple VideoPipeline instances
 *
 * This class manages concurrent video stream processing pipelines,
 * handles video source addition/removal, and coordinates system resources.
 * Thread-safe implementation using mutex protection.
 *
 * Task 75: Enhanced with cross-camera tracking capabilities to share ReID features
 * between VideoPipeline instances for consistent tracking across multiple cameras.
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

    // Task 75: Cross-camera tracking methods
    void reportTrackUpdate(const std::string& cameraId, int localTrackId,
                          const std::vector<float>& reidFeatures, const cv::Rect& bbox,
                          int classId, float confidence);

    int getGlobalTrackId(const std::string& cameraId, int localTrackId) const;
    std::vector<CrossCameraTrack> getActiveCrossCameraTracks() const;
    std::vector<ReIDMatch> findReIDMatches(const std::vector<float>& features,
                                          const std::string& excludeCameraId = "") const;

    // Cross-camera tracking configuration
    void setCrossCameraTrackingEnabled(bool enabled);
    void setReIDSimilarityThreshold(float threshold);
    void setMaxTrackAge(double ageSeconds);
    void setCrossCameraMatchingEnabled(bool enabled);

    bool isCrossCameraTrackingEnabled() const;
    float getReIDSimilarityThreshold() const;
    double getMaxTrackAge() const;

    // Cross-camera tracking statistics
    size_t getGlobalTrackCount() const;
    size_t getActiveCrossCameraTrackCount() const;
    size_t getCrossCameraMatchCount() const;
    void resetCrossCameraTrackingStats();

    // Configuration constants
    static constexpr size_t MAX_PIPELINES = 16;
    static constexpr int MONITORING_INTERVAL_MS = 1000;

    // Cross-camera tracking constants
    static constexpr float DEFAULT_REID_SIMILARITY_THRESHOLD = 0.7f;
    static constexpr double DEFAULT_MAX_TRACK_AGE_SECONDS = 30.0;
    static constexpr size_t MAX_GLOBAL_TRACKS = 1000;

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

    // Task 75: Cross-camera tracking state
    mutable std::mutex m_crossCameraMutex;
    std::unordered_map<int, std::shared_ptr<CrossCameraTrack>> m_globalTracks;
    std::unordered_map<std::string, std::unordered_map<int, int>> m_localToGlobalTrackMap; // [cameraId][localId] -> globalId
    std::atomic<int> m_nextGlobalTrackId{1};

    // Cross-camera tracking configuration
    std::atomic<bool> m_crossCameraTrackingEnabled{true};
    std::atomic<bool> m_crossCameraMatchingEnabled{true};
    std::atomic<float> m_reidSimilarityThreshold{DEFAULT_REID_SIMILARITY_THRESHOLD};
    std::atomic<double> m_maxTrackAge{DEFAULT_MAX_TRACK_AGE_SECONDS};

    // Cross-camera tracking statistics
    mutable std::atomic<size_t> m_totalCrossCameraMatches{0};
    mutable std::atomic<size_t> m_activeCrossCameraTracks{0};

    // Internal cross-camera tracking methods
    int createNewGlobalTrack(const std::string& cameraId, int localTrackId,
                           const std::vector<float>& reidFeatures, const cv::Rect& bbox,
                           int classId, float confidence);

    std::shared_ptr<CrossCameraTrack> findBestMatch(const std::vector<float>& features,
                                                   const std::string& excludeCameraId) const;

    void cleanupExpiredTracks();
    float computeReIDSimilarity(const std::vector<float>& features1,
                               const std::vector<float>& features2) const;

    void updateCrossCameraTrackingStats();
};

// VideoSource is now defined in VideoPipeline.h
