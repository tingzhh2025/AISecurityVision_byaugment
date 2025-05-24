#include "TaskManager.h"
#include "VideoPipeline.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <algorithm>

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
#endif

TaskManager& TaskManager::getInstance() {
    static TaskManager instance;
    return instance;
}

TaskManager::TaskManager() {
    std::cout << "[TaskManager] Initializing TaskManager singleton" << std::endl;

    // Record system start time
    m_systemStartTime = std::chrono::steady_clock::now();

    // Initialize GPU monitoring
    if (!initializeGpuMonitoring()) {
        std::cout << "[TaskManager] GPU monitoring not available" << std::endl;
    }
}

TaskManager::~TaskManager() {
    stop();
    cleanupGpuMonitoring();
}

void TaskManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load()) {
        std::cout << "[TaskManager] Already running" << std::endl;
        return;
    }

    m_running.store(true);
    m_monitoringThread = std::thread(&TaskManager::monitoringThread, this);

    std::cout << "[TaskManager] Started successfully" << std::endl;
}

void TaskManager::stop() {
    std::cout << "[TaskManager] Stopping..." << std::endl;

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

    std::cout << "[TaskManager] Stopped successfully" << std::endl;
}

bool TaskManager::isRunning() const {
    return m_running.load();
}

bool TaskManager::addVideoSource(const VideoSource& source) {
    if (!source.isValid()) {
        std::cerr << "[TaskManager] Invalid video source: " << source.toString() << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_pipelines.size() >= MAX_PIPELINES) {
        std::cerr << "[TaskManager] Maximum pipeline limit reached: " << MAX_PIPELINES << std::endl;
        return false;
    }

    if (m_pipelines.find(source.id) != m_pipelines.end()) {
        std::cerr << "[TaskManager] Pipeline already exists for source: " << source.id << std::endl;
        return false;
    }

    try {
        auto pipeline = std::make_shared<VideoPipeline>(source);
        if (pipeline->initialize()) {
            m_pipelines[source.id] = pipeline;
            pipeline->start();

            std::cout << "[TaskManager] Added video source: " << source.id
                      << " (" << source.protocol << ")" << std::endl;
            return true;
        } else {
            std::cerr << "[TaskManager] Failed to initialize pipeline for: " << source.id << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[TaskManager] Exception creating pipeline: " << e.what() << std::endl;
        return false;
    }
}

bool TaskManager::removeVideoSource(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(sourceId);
    if (it == m_pipelines.end()) {
        std::cerr << "[TaskManager] Pipeline not found: " << sourceId << std::endl;
        return false;
    }

    // Stop and cleanup pipeline
    if (it->second) {
        it->second->stop();
    }
    m_pipelines.erase(it);

    std::cout << "[TaskManager] Removed video source: " << sourceId << std::endl;
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
    std::cout << "[TaskManager] Enhanced monitoring thread started with 1s precision" << std::endl;

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
            std::cout << "[TaskManager] Warning: Could not set thread priority" << std::endl;
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
                std::cout << "[TaskManager] Cleaning up failed pipeline: " << id << std::endl;
                removeVideoSource(id);
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
                std::cerr << "[TaskManager] Warning: Monitoring cycle took " << cycleDuration
                         << "ms (target: " << MONITORING_INTERVAL_MS << "ms)" << std::endl;
            }

            m_lastMonitoringTime = cycleEndTime;

        } catch (const std::exception& e) {
            std::cerr << "[TaskManager] Monitoring error: " << e.what() << std::endl;
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
            std::cerr << "[TaskManager] Warning: Monitoring thread behind schedule" << std::endl;
        }
    }

    std::cout << "[TaskManager] Enhanced monitoring thread stopped after "
              << m_monitoringCycles.load() << " cycles" << std::endl;
}

bool TaskManager::readCpuStats(CpuStats& stats) const {
#ifdef __linux__
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        std::cerr << "[TaskManager] Failed to open /proc/stat" << std::endl;
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        std::cerr << "[TaskManager] Failed to read from /proc/stat" << std::endl;
        return false;
    }

    // Parse the first line which contains overall CPU stats
    // Format: cpu user nice system idle iowait irq softirq steal guest guest_nice
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;

    if (cpu_label != "cpu") {
        std::cerr << "[TaskManager] Invalid /proc/stat format" << std::endl;
        return false;
    }

    if (!(iss >> stats.user >> stats.nice >> stats.system >> stats.idle
             >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal)) {
        std::cerr << "[TaskManager] Failed to parse CPU stats" << std::endl;
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
        std::cout << "[TaskManager] Failed to initialize NVML: " << nvmlErrorString(result) << std::endl;
        return false;
    }

    result = nvmlDeviceGetCount(&m_gpuDeviceCount);
    if (result != NVML_SUCCESS) {
        std::cout << "[TaskManager] Failed to get GPU device count: " << nvmlErrorString(result) << std::endl;
        nvmlShutdown();
        return false;
    }

    if (m_gpuDeviceCount == 0) {
        std::cout << "[TaskManager] No NVIDIA GPUs found" << std::endl;
        nvmlShutdown();
        return false;
    }

    // Get handle for the first GPU
    nvmlDevice_t device;
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        std::cout << "[TaskManager] Failed to get GPU device handle: " << nvmlErrorString(result) << std::endl;
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
        std::cout << "[TaskManager] GPU monitoring initialized for: " << name << std::endl;
    } else {
        std::cout << "[TaskManager] GPU monitoring initialized (unknown device)" << std::endl;
    }

    return true;
#else
    std::cout << "[TaskManager] NVML not available - GPU monitoring disabled" << std::endl;
    return false;
#endif
}

void TaskManager::cleanupGpuMonitoring() {
#ifdef HAVE_NVML
    if (m_nvmlInitialized) {
        nvmlShutdown();
        m_nvmlInitialized = false;
        m_gpuDevice = nullptr;
        std::cout << "[TaskManager] GPU monitoring cleanup complete" << std::endl;
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
    std::cout << "[TaskManager] Monitoring statistics reset" << std::endl;
}

// VideoSource implementation is now in VideoPipeline.cpp
