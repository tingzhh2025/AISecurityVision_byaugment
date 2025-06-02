#include "TaskManager.h"
#include "VideoPipeline.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <cmath>

// NVML includes for GPU monitoring
#ifdef HAVE_NVML
#include <nvml.h>
#endif

// System includes for monitoring
#ifdef __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include "../core/Logger.h"
using namespace AISecurityVision;
#endif

TaskManager& TaskManager::getInstance() {
    static TaskManager instance;
    return instance;
}

TaskManager::TaskManager() {
    LOG_INFO() << "[TaskManager] Initializing TaskManager singleton";

    // Record system start time
    m_systemStartTime = std::chrono::steady_clock::now();

    // Initialize GPU monitoring
    if (!initializeGpuMonitoring()) {
        LOG_INFO() << "[TaskManager] GPU monitoring not available";
    }
}

TaskManager::~TaskManager() {
    stop();
    cleanupGpuMonitoring();
}

void TaskManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load()) {
        LOG_INFO() << "[TaskManager] Already running";
        return;
    }

    m_running.store(true);
    m_monitoringThread = std::thread(&TaskManager::monitoringThread, this);

    LOG_INFO() << "[TaskManager] Started successfully";
}

void TaskManager::stop() {
    LOG_INFO() << "[TaskManager] Stopping...";

    m_running.store(false);

    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }

    // Stop all pipelines
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, pipeline] : m_pipelines) {
        if (pipeline) {
            pipeline->stop();
        }
    }
    m_pipelines.clear();

    LOG_INFO() << "[TaskManager] Stopped successfully";
}

bool TaskManager::isRunning() const {
    return m_running.load();
}

bool TaskManager::addVideoSource(const VideoSource& source) {
    if (!source.isValid()) {
        LOG_ERROR() << "[TaskManager] Invalid video source: " << source.toString();
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_pipelines.size() >= MAX_PIPELINES) {
        LOG_ERROR() << "[TaskManager] Maximum pipeline limit reached: " << MAX_PIPELINES;
        return false;
    }

    if (m_pipelines.find(source.id) != m_pipelines.end()) {
        LOG_ERROR() << "[TaskManager] Pipeline already exists for source: " << source.id;
        return false;
    }

    try {
        auto pipeline = std::make_shared<VideoPipeline>(source);
        if (pipeline->initialize()) {
            m_pipelines[source.id] = pipeline;
            pipeline->start();

            LOG_INFO() << "[TaskManager] Added video source: " << source.id
                      << " (" << source.protocol << ")";
            return true;
        } else {
            LOG_ERROR() << "[TaskManager] Failed to initialize pipeline for: " << source.id;
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR() << "[TaskManager] Exception creating pipeline: " << e.what();
        return false;
    }
}

bool TaskManager::removeVideoSource(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(sourceId);
    if (it == m_pipelines.end()) {
        LOG_ERROR() << "[TaskManager] Pipeline not found: " << sourceId;
        return false;
    }

    // Stop and cleanup pipeline
    if (it->second) {
        it->second->stop();
    }
    m_pipelines.erase(it);

    LOG_INFO() << "[TaskManager] Removed video source: " << sourceId;
    return true;
}

std::vector<std::string> TaskManager::getActivePipelines() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> active;
    for (const auto& [id, pipeline] : m_pipelines) {
        if (pipeline && pipeline->isRunning()) {
            active.push_back(id);
        }
    }
    return active;
}

size_t TaskManager::getActivePipelineCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pipelines.size();
}

double TaskManager::getCpuUsage() const {
    return m_cpuUsage.load();
}

std::string TaskManager::getGpuMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gpuMemUsage;
}

double TaskManager::getGpuUtilization() const {
    return m_gpuUtilization.load();
}

double TaskManager::getGpuTemperature() const {
    return m_gpuTemperature.load();
}

std::shared_ptr<VideoPipeline> TaskManager::getPipeline(const std::string& sourceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(sourceId);
    return (it != m_pipelines.end()) ? it->second : nullptr;
}

void TaskManager::monitoringThread() {
    LOG_INFO() << "[TaskManager] Enhanced monitoring thread started with 1s precision";

    // Initialize timing variables
    auto nextUpdateTime = std::chrono::steady_clock::now();
    const auto interval = std::chrono::milliseconds(MONITORING_INTERVAL_MS);

    // Set thread priority (Linux-specific)
#ifdef __linux__
    // Attempt to set higher priority for monitoring thread
    struct sched_param param;
    param.sched_priority = 1;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // Fallback to nice priority if real-time scheduling fails
        if (nice(-5) == -1) {
            LOG_WARN() << "[TaskManager] Warning: Could not set thread priority";
        }
    }
#endif

    while (m_running.load()) {
        auto cycleStartTime = std::chrono::steady_clock::now();

        try {
            // Update CPU usage
            CpuStats currentStats;
            if (readCpuStats(currentStats)) {
                if (m_cpuStatsInitialized) {
                    double cpuUsage = calculateCpuUsage(m_lastCpuStats, currentStats);
                    m_cpuUsage.store(cpuUsage);
                } else {
                    m_cpuStatsInitialized = true;
                }
                m_lastCpuStats = currentStats;
            }

            // Update GPU metrics
            updateGpuMetrics();

            // Check pipeline health
            std::vector<std::string> failedPipelines;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto& [id, pipeline] : m_pipelines) {
                    if (pipeline && !pipeline->isHealthy()) {
                        failedPipelines.push_back(id);
                    }
                }
            }

            // Cleanup failed pipelines
            for (const auto& id : failedPipelines) {
                LOG_ERROR() << "[TaskManager] Cleaning up failed pipeline: " << id;
                removeVideoSource(id);
            }

            // Task 75: Periodic cross-camera tracking cleanup
            if (m_crossCameraTrackingEnabled.load()) {
                std::lock_guard<std::mutex> crossCameraLock(m_crossCameraMutex);
                cleanupExpiredTracks();
                updateCrossCameraTrackingStats();
            }

            // Update monitoring performance metrics
            auto cycleEndTime = std::chrono::steady_clock::now();
            auto cycleDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                cycleEndTime - cycleStartTime).count() / 1000.0; // Convert to milliseconds

            uint64_t cycles = m_monitoringCycles.fetch_add(1) + 1;

            // Update average monitoring time (exponential moving average)
            double currentAvg = m_avgMonitoringTime.load();
            double newAvg = (currentAvg * 0.9) + (cycleDuration * 0.1);
            m_avgMonitoringTime.store(newAvg);

            // Update max monitoring time
            double currentMax = m_maxMonitoringTime.load();
            if (cycleDuration > currentMax) {
                m_maxMonitoringTime.store(cycleDuration);
            }

            // Check monitoring health (warn if cycle takes too long)
            bool healthy = cycleDuration < (MONITORING_INTERVAL_MS * 0.8); // 80% of interval
            m_monitoringHealthy.store(healthy);

            if (!healthy) {
                LOG_WARN() << "[TaskManager] Warning: Monitoring cycle took " << cycleDuration
                         << "ms (target: " << MONITORING_INTERVAL_MS << "ms)";
            }

            m_lastMonitoringTime = cycleEndTime;

        } catch (const std::exception& e) {
            LOG_ERROR() << "[TaskManager] Monitoring error: " << e.what();
            m_monitoringHealthy.store(false);
        }

        // Precise timing: sleep until next scheduled update
        nextUpdateTime += interval;
        auto now = std::chrono::steady_clock::now();

        if (nextUpdateTime > now) {
            std::this_thread::sleep_until(nextUpdateTime);
        } else {
            // We're behind schedule, adjust next update time
            nextUpdateTime = now + interval;
            LOG_WARN() << "[TaskManager] Warning: Monitoring thread behind schedule";
        }
    }

    LOG_INFO() << "[TaskManager] Enhanced monitoring thread stopped after "
              << m_monitoringCycles.load() << " cycles";
}

bool TaskManager::readCpuStats(CpuStats& stats) const {
#ifdef __linux__
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        LOG_ERROR() << "[TaskManager] Failed to open /proc/stat";
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        LOG_ERROR() << "[TaskManager] Failed to read from /proc/stat";
        return false;
    }

    // Parse the first line which contains overall CPU stats
    // Format: cpu user nice system idle iowait irq softirq steal guest guest_nice
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;

    if (cpu_label != "cpu") {
        LOG_ERROR() << "[TaskManager] Invalid /proc/stat format";
        return false;
    }

    if (!(iss >> stats.user >> stats.nice >> stats.system >> stats.idle
             >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal)) {
        LOG_ERROR() << "[TaskManager] Failed to parse CPU stats";
        return false;
    }

    return true;
#else
    // Non-Linux systems - return dummy data
    stats = CpuStats{};
    return false;
#endif
}

double TaskManager::calculateCpuUsage(const CpuStats& prev, const CpuStats& curr) const {
    unsigned long long prevTotal = prev.total();
    unsigned long long currTotal = curr.total();
    unsigned long long prevActive = prev.active();
    unsigned long long currActive = curr.active();

    unsigned long long totalDiff = currTotal - prevTotal;
    unsigned long long activeDiff = currActive - prevActive;

    if (totalDiff == 0) {
        return 0.0;
    }

    double cpuUsage = (static_cast<double>(activeDiff) / static_cast<double>(totalDiff)) * 100.0;

    // Clamp to reasonable range
    if (cpuUsage < 0.0) cpuUsage = 0.0;
    if (cpuUsage > 100.0) cpuUsage = 100.0;

    return cpuUsage;
}

// GPU monitoring implementation
bool TaskManager::initializeGpuMonitoring() {
#ifdef HAVE_NVML
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        LOG_ERROR() << "[TaskManager] Failed to initialize NVML: " << nvmlErrorString(result);
        return false;
    }

    result = nvmlDeviceGetCount(&m_gpuDeviceCount);
    if (result != NVML_SUCCESS) {
        LOG_ERROR() << "[TaskManager] Failed to get GPU device count: " << nvmlErrorString(result);
        nvmlShutdown();
        return false;
    }

    if (m_gpuDeviceCount == 0) {
        LOG_INFO() << "[TaskManager] No NVIDIA GPUs found";
        nvmlShutdown();
        return false;
    }

    // Get handle for the first GPU
    nvmlDevice_t device;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        LOG_ERROR() << "[TaskManager] Failed to get GPU device handle: " << nvmlErrorString(result);
        nvmlShutdown();
        return false;
    }

    // Store device handle (cast to void* for header compatibility)
    m_gpuDevice = reinterpret_cast<void*>(device);
    m_nvmlInitialized = true;

    // Get GPU name for logging
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
    if (result == NVML_SUCCESS) {
        LOG_INFO() << "[TaskManager] GPU monitoring initialized for: " << name;
    } else {
        LOG_INFO() << "[TaskManager] GPU monitoring initialized (unknown device)";
    }

    return true;
#else
    LOG_INFO() << "[TaskManager] NVML not available - GPU monitoring disabled";
    return false;
#endif
}

void TaskManager::cleanupGpuMonitoring() {
#ifdef HAVE_NVML
    if (m_nvmlInitialized) {
        nvmlShutdown();
        m_nvmlInitialized = false;
        m_gpuDevice = nullptr;
        LOG_INFO() << "[TaskManager] GPU monitoring cleanup complete";
    }
#endif
}

bool TaskManager::updateGpuMetrics() {
#ifdef HAVE_NVML
    if (!m_nvmlInitialized || !m_gpuDevice) {
        // Fallback to placeholder values
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gpuMemUsage = "N/A";
        m_gpuUtilization.store(0.0);
        m_gpuTemperature.store(0.0);
        return false;
    }

    nvmlDevice_t device = reinterpret_cast<nvmlDevice_t>(m_gpuDevice);
    nvmlReturn_t result;

    // Get memory information
    nvmlMemory_t memInfo;
    result = nvmlDeviceGetMemoryInfo(device, &memInfo);
    if (result == NVML_SUCCESS) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Convert bytes to MB
        unsigned long long usedMB = memInfo.used / (1024 * 1024);
        unsigned long long totalMB = memInfo.total / (1024 * 1024);

        std::ostringstream oss;
        oss << usedMB << "MB / " << totalMB << "MB";
        m_gpuMemUsage = oss.str();
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gpuMemUsage = "Error";
    }

    // Get GPU utilization
    nvmlUtilization_t utilization;
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    if (result == NVML_SUCCESS) {
        m_gpuUtilization.store(static_cast<double>(utilization.gpu));
    } else {
        m_gpuUtilization.store(0.0);
    }

    // Get GPU temperature
    unsigned int temperature;
    result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);
    if (result == NVML_SUCCESS) {
        m_gpuTemperature.store(static_cast<double>(temperature));
    } else {
        m_gpuTemperature.store(0.0);
    }

    return true;
#else
    // Fallback when NVML is not available
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gpuMemUsage = "NVML N/A";
    m_gpuUtilization.store(0.0);
    m_gpuTemperature.store(0.0);
    return false;
#endif
}

// Enhanced pipeline statistics implementation
std::vector<TaskManager::PipelineStats> TaskManager::getAllPipelineStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PipelineStats> stats;

    auto now = std::chrono::steady_clock::now();

    for (const auto& [id, pipeline] : m_pipelines) {
        if (pipeline) {
            PipelineStats pipelineStats;
            pipelineStats.sourceId = id;
            pipelineStats.protocol = pipeline->getSource().protocol;
            pipelineStats.url = pipeline->getSource().url;
            pipelineStats.isRunning = pipeline->isRunning();
            pipelineStats.isHealthy = pipeline->isHealthy();
            pipelineStats.frameRate = pipeline->getFrameRate();
            pipelineStats.processedFrames = pipeline->getProcessedFrames();
            pipelineStats.droppedFrames = pipeline->getDroppedFrames();
            pipelineStats.lastError = pipeline->getLastError();

            // Calculate uptime
            auto startTime = pipeline->getStartTime();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
            pipelineStats.uptime = duration.count() / 1000.0;

            stats.push_back(pipelineStats);
        }
    }

    return stats;
}

TaskManager::PipelineStats TaskManager::getPipelineStats(const std::string& sourceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(sourceId);
    if (it == m_pipelines.end() || !it->second) {
        // Return empty stats for non-existent pipeline
        return PipelineStats{};
    }

    auto pipeline = it->second;
    auto now = std::chrono::steady_clock::now();

    PipelineStats stats;
    stats.sourceId = sourceId;
    stats.protocol = pipeline->getSource().protocol;
    stats.url = pipeline->getSource().url;
    stats.isRunning = pipeline->isRunning();
    stats.isHealthy = pipeline->isHealthy();
    stats.frameRate = pipeline->getFrameRate();
    stats.processedFrames = pipeline->getProcessedFrames();
    stats.droppedFrames = pipeline->getDroppedFrames();
    stats.lastError = pipeline->getLastError();

    // Calculate uptime
    auto startTime = pipeline->getStartTime();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
    stats.uptime = duration.count() / 1000.0;

    return stats;
}

TaskManager::SystemStats TaskManager::getSystemStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    SystemStats stats;
    auto now = std::chrono::steady_clock::now();

    // System-wide counters
    stats.totalPipelines = m_pipelines.size();
    stats.runningPipelines = 0;
    stats.healthyPipelines = 0;
    stats.totalFrameRate = 0.0;
    stats.totalProcessedFrames = 0;
    stats.totalDroppedFrames = 0;

    // Aggregate pipeline statistics
    for (const auto& [id, pipeline] : m_pipelines) {
        if (pipeline) {
            if (pipeline->isRunning()) {
                stats.runningPipelines++;
            }
            if (pipeline->isHealthy()) {
                stats.healthyPipelines++;
            }
            stats.totalFrameRate += pipeline->getFrameRate();
            stats.totalProcessedFrames += pipeline->getProcessedFrames();
            stats.totalDroppedFrames += pipeline->getDroppedFrames();
        }
    }

    // System metrics
    stats.cpuUsage = m_cpuUsage.load();
    stats.gpuMemUsage = m_gpuMemUsage;
    stats.gpuUtilization = m_gpuUtilization.load();
    stats.gpuTemperature = m_gpuTemperature.load();

    // System uptime
    stats.systemStartTime = m_systemStartTime;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_systemStartTime);
    stats.systemUptime = duration.count() / 1000.0;

    return stats;
}

// Monitoring performance metrics methods
uint64_t TaskManager::getMonitoringCycles() const {
    return m_monitoringCycles.load();
}

double TaskManager::getAverageMonitoringTime() const {
    return m_avgMonitoringTime.load();
}

double TaskManager::getMaxMonitoringTime() const {
    return m_maxMonitoringTime.load();
}

bool TaskManager::isMonitoringHealthy() const {
    return m_monitoringHealthy.load();
}

void TaskManager::resetMonitoringStats() {
    m_monitoringCycles.store(0);
    m_avgMonitoringTime.store(0.0);
    m_maxMonitoringTime.store(0.0);
    m_monitoringHealthy.store(true);
    LOG_INFO() << "[TaskManager] Monitoring statistics reset";
}

// Task 75: Cross-camera tracking implementation
void TaskManager::reportTrackUpdate(const std::string& cameraId, int localTrackId,
                                   const std::vector<float>& reidFeatures, const cv::Rect& bbox,
                                   int classId, float confidence) {
    if (!m_crossCameraTrackingEnabled.load() || reidFeatures.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_crossCameraMutex);

    // Check if this local track already has a global track ID
    auto cameraIt = m_localToGlobalTrackMap.find(cameraId);
    if (cameraIt != m_localToGlobalTrackMap.end()) {
        auto trackIt = cameraIt->second.find(localTrackId);
        if (trackIt != cameraIt->second.end()) {
            // Update existing global track
            int globalId = trackIt->second;
            auto globalTrackIt = m_globalTracks.find(globalId);
            if (globalTrackIt != m_globalTracks.end()) {
                globalTrackIt->second->updateTrack(cameraId, localTrackId, reidFeatures, bbox, confidence);
                return;
            }
        }
    }

    // Try to find a matching global track using ReID features
    if (m_crossCameraMatchingEnabled.load()) {
        auto bestMatch = findBestMatch(reidFeatures, cameraId);
        if (bestMatch) {
            // Match found - associate with existing global track
            bestMatch->updateTrack(cameraId, localTrackId, reidFeatures, bbox, confidence);
            m_localToGlobalTrackMap[cameraId][localTrackId] = bestMatch->globalTrackId;
            m_totalCrossCameraMatches.fetch_add(1);

            LOG_INFO() << "[TaskManager] Cross-camera match: camera " << cameraId
                      << " local track " << localTrackId << " -> global track "
                      << bestMatch->globalTrackId;
            return;
        }
    }

    // No match found - create new global track
    int globalId = createNewGlobalTrack(cameraId, localTrackId, reidFeatures, bbox, classId, confidence);
    m_localToGlobalTrackMap[cameraId][localTrackId] = globalId;
}

int TaskManager::getGlobalTrackId(const std::string& cameraId, int localTrackId) const {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);

    auto cameraIt = m_localToGlobalTrackMap.find(cameraId);
    if (cameraIt != m_localToGlobalTrackMap.end()) {
        auto trackIt = cameraIt->second.find(localTrackId);
        if (trackIt != cameraIt->second.end()) {
            return trackIt->second;
        }
    }
    return -1; // No global track ID found
}

std::vector<CrossCameraTrack> TaskManager::getActiveCrossCameraTracks() const {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);

    std::vector<CrossCameraTrack> activeTracks;
    for (const auto& [globalId, track] : m_globalTracks) {
        if (track && track->isActive && !track->isExpired(m_maxTrackAge.load())) {
            activeTracks.push_back(*track);
        }
    }
    return activeTracks;
}

std::vector<ReIDMatch> TaskManager::findReIDMatches(const std::vector<float>& features,
                                                   const std::string& excludeCameraId) const {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);

    std::vector<ReIDMatch> matches;
    float threshold = m_reidSimilarityThreshold.load();

    for (const auto& [globalId, track] : m_globalTracks) {
        if (!track || track->isExpired(m_maxTrackAge.load())) {
            continue;
        }

        // Skip tracks from the excluded camera
        if (!excludeCameraId.empty() && track->hasCamera(excludeCameraId)) {
            continue;
        }

        float similarity = computeReIDSimilarity(features, track->reidFeatures);
        if (similarity >= threshold) {
            // Find the camera and local track ID for this match
            for (const auto& [cameraId, localId] : track->localTrackIds) {
                if (cameraId != excludeCameraId) {
                    matches.emplace_back(globalId, similarity, cameraId, localId);
                    break; // Only add one match per global track
                }
            }
        }
    }

    // Sort by similarity (highest first)
    std::sort(matches.begin(), matches.end(),
              [](const ReIDMatch& a, const ReIDMatch& b) {
                  return a.similarity > b.similarity;
              });

    return matches;
}

// Cross-camera tracking configuration methods
void TaskManager::setCrossCameraTrackingEnabled(bool enabled) {
    m_crossCameraTrackingEnabled.store(enabled);
    LOG_INFO() << "[TaskManager] Cross-camera tracking " << (enabled ? "enabled" : "disabled");
}

void TaskManager::setReIDSimilarityThreshold(float threshold) {
    if (threshold >= 0.0f && threshold <= 1.0f) {
        m_reidSimilarityThreshold.store(threshold);
        LOG_INFO() << "[TaskManager] ReID similarity threshold set to " << threshold;
    }
}

void TaskManager::setMaxTrackAge(double ageSeconds) {
    if (ageSeconds > 0.0) {
        m_maxTrackAge.store(ageSeconds);
        LOG_INFO() << "[TaskManager] Max track age set to " << ageSeconds << " seconds";
    }
}

void TaskManager::setCrossCameraMatchingEnabled(bool enabled) {
    m_crossCameraMatchingEnabled.store(enabled);
    LOG_INFO() << "[TaskManager] Cross-camera matching " << (enabled ? "enabled" : "disabled");
}

bool TaskManager::isCrossCameraTrackingEnabled() const {
    return m_crossCameraTrackingEnabled.load();
}

float TaskManager::getReIDSimilarityThreshold() const {
    return m_reidSimilarityThreshold.load();
}

double TaskManager::getMaxTrackAge() const {
    return m_maxTrackAge.load();
}

// Cross-camera tracking statistics methods
size_t TaskManager::getGlobalTrackCount() const {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);
    return m_globalTracks.size();
}

size_t TaskManager::getActiveCrossCameraTrackCount() const {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);

    size_t activeCount = 0;
    double maxAge = m_maxTrackAge.load();
    for (const auto& [globalId, track] : m_globalTracks) {
        if (track && track->isActive && !track->isExpired(maxAge)) {
            activeCount++;
        }
    }
    return activeCount;
}

size_t TaskManager::getCrossCameraMatchCount() const {
    return m_totalCrossCameraMatches.load();
}

void TaskManager::resetCrossCameraTrackingStats() {
    std::lock_guard<std::mutex> lock(m_crossCameraMutex);
    m_totalCrossCameraMatches.store(0);
    m_activeCrossCameraTracks.store(0);
    LOG_INFO() << "[TaskManager] Cross-camera tracking statistics reset";
}

// VideoSource implementation is now in VideoPipeline.cpp

// Task 75: CrossCameraTrack implementation
CrossCameraTrack::CrossCameraTrack(int globalId, const std::string& cameraId, int localId,
                                  const std::vector<float>& features, const cv::Rect& bbox,
                                  int cls, float conf)
    : globalTrackId(globalId), primaryCameraId(cameraId), reidFeatures(features),
      lastBbox(bbox), classId(cls), confidence(conf), isActive(true) {

    auto now = std::chrono::steady_clock::now();
    firstSeen = now;
    lastSeen = now;
    localTrackIds[cameraId] = localId;

    LOG_INFO() << "[CrossCameraTrack] Created global track " << globalTrackId
              << " for camera " << cameraId << " local track " << localId;
}

void CrossCameraTrack::updateTrack(const std::string& cameraId, int localId,
                                  const std::vector<float>& features, const cv::Rect& bbox,
                                  float conf) {
    lastSeen = std::chrono::steady_clock::now();
    lastBbox = bbox;
    confidence = conf;
    isActive = true;

    // Update ReID features (exponential moving average)
    if (!features.empty() && features.size() == reidFeatures.size()) {
        const float alpha = 0.3f; // Learning rate
        for (size_t i = 0; i < reidFeatures.size(); ++i) {
            reidFeatures[i] = alpha * features[i] + (1.0f - alpha) * reidFeatures[i];
        }
    } else if (!features.empty()) {
        reidFeatures = features; // Replace if dimensions don't match
    }

    // Update local track ID for this camera
    localTrackIds[cameraId] = localId;

    LOG_INFO() << "[CrossCameraTrack] Updated global track " << globalTrackId
              << " from camera " << cameraId << " local track " << localId;
}

bool CrossCameraTrack::hasCamera(const std::string& cameraId) const {
    return localTrackIds.find(cameraId) != localTrackIds.end();
}

int CrossCameraTrack::getLocalTrackId(const std::string& cameraId) const {
    auto it = localTrackIds.find(cameraId);
    return (it != localTrackIds.end()) ? it->second : -1;
}

double CrossCameraTrack::getTimeSinceLastSeen() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSeen);
    return duration.count() / 1000.0; // Convert to seconds
}

bool CrossCameraTrack::isExpired(double maxAgeSeconds) const {
    return getTimeSinceLastSeen() > maxAgeSeconds;
}

// Task 75: Internal cross-camera tracking helper methods
int TaskManager::createNewGlobalTrack(const std::string& cameraId, int localTrackId,
                                     const std::vector<float>& reidFeatures, const cv::Rect& bbox,
                                     int classId, float confidence) {
    int globalId = m_nextGlobalTrackId.fetch_add(1);

    auto globalTrack = std::make_shared<CrossCameraTrack>(
        globalId, cameraId, localTrackId, reidFeatures, bbox, classId, confidence);

    m_globalTracks[globalId] = globalTrack;

    // Cleanup expired tracks if we're approaching the limit
    if (m_globalTracks.size() > MAX_GLOBAL_TRACKS * 0.8) {
        cleanupExpiredTracks();
    }

    LOG_INFO() << "[TaskManager] Created new global track " << globalId
              << " for camera " << cameraId << " local track " << localTrackId;

    return globalId;
}

std::shared_ptr<CrossCameraTrack> TaskManager::findBestMatch(const std::vector<float>& features,
                                                           const std::string& excludeCameraId) const {
    std::shared_ptr<CrossCameraTrack> bestMatch = nullptr;
    float bestSimilarity = 0.0f;
    float threshold = m_reidSimilarityThreshold.load();
    double maxAge = m_maxTrackAge.load();

    for (const auto& [globalId, track] : m_globalTracks) {
        if (!track || track->isExpired(maxAge)) {
            continue;
        }

        // Skip tracks from the same camera
        if (track->hasCamera(excludeCameraId)) {
            continue;
        }

        float similarity = computeReIDSimilarity(features, track->reidFeatures);
        if (similarity >= threshold && similarity > bestSimilarity) {
            bestSimilarity = similarity;
            bestMatch = track;
        }
    }

    return bestMatch;
}

void TaskManager::cleanupExpiredTracks() {
    double maxAge = m_maxTrackAge.load();
    auto it = m_globalTracks.begin();

    while (it != m_globalTracks.end()) {
        if (!it->second || it->second->isExpired(maxAge)) {
            // Remove from local-to-global mapping
            for (const auto& [cameraId, localId] : it->second->localTrackIds) {
                auto cameraIt = m_localToGlobalTrackMap.find(cameraId);
                if (cameraIt != m_localToGlobalTrackMap.end()) {
                    cameraIt->second.erase(localId);
                    if (cameraIt->second.empty()) {
                        m_localToGlobalTrackMap.erase(cameraIt);
                    }
                }
            }

            LOG_INFO() << "[TaskManager] Cleaned up expired global track " << it->first;
            it = m_globalTracks.erase(it);
        } else {
            ++it;
        }
    }
}

float TaskManager::computeReIDSimilarity(const std::vector<float>& features1,
                                        const std::vector<float>& features2) const {
    if (features1.empty() || features2.empty() || features1.size() != features2.size()) {
        return 0.0f;
    }

    // Compute cosine similarity
    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < features1.size(); ++i) {
        dotProduct += features1[i] * features2[i];
        norm1 += features1[i] * features1[i];
        norm2 += features2[i] * features2[i];
    }

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

void TaskManager::updateCrossCameraTrackingStats() {
    // This method can be called periodically to update statistics
    // Currently, statistics are updated in real-time, but this provides
    // a hook for future batch updates if needed
    size_t activeCount = getActiveCrossCameraTrackCount();
    m_activeCrossCameraTracks.store(activeCount);
}

// Detection category filtering implementation
void TaskManager::updateDetectionCategories(const std::vector<std::string>& enabledCategories) {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_INFO() << "[TaskManager] Updating detection categories for " << m_pipelines.size() << " pipelines";

    int updatedPipelines = 0;
    for (const auto& [id, pipeline] : m_pipelines) {
        if (pipeline) {
            // Update detection categories for this pipeline
            if (pipeline->updateDetectionCategories(enabledCategories)) {
                updatedPipelines++;
            }
        }
    }

    LOG_INFO() << "[TaskManager] Updated detection categories for " << updatedPipelines
               << " out of " << m_pipelines.size() << " pipelines";
}
