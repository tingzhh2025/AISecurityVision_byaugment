#include "TaskManager.h"
#include "VideoPipeline.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <algorithm>

// System includes for monitoring
#ifdef __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

TaskManager& TaskManager::getInstance() {
    static TaskManager instance;
    return instance;
}

TaskManager::TaskManager() {
    std::cout << "[TaskManager] Initializing TaskManager singleton" << std::endl;
}

TaskManager::~TaskManager() {
    stop();
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

std::shared_ptr<VideoPipeline> TaskManager::getPipeline(const std::string& sourceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(sourceId);
    return (it != m_pipelines.end()) ? it->second : nullptr;
}

void TaskManager::monitoringThread() {
    std::cout << "[TaskManager] Monitoring thread started" << std::endl;

    while (m_running.load()) {
        try {
            // Update CPU usage
            // TODO: Implement proper CPU monitoring
            m_cpuUsage.store(0.0);

            // Update GPU memory usage
            // TODO: Implement NVML GPU monitoring
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_gpuMemUsage = "0MB / 0MB";
            }

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

        } catch (const std::exception& e) {
            std::cerr << "[TaskManager] Monitoring error: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(MONITORING_INTERVAL_MS));
    }

    std::cout << "[TaskManager] Monitoring thread stopped" << std::endl;
}

// VideoSource implementation is now in VideoPipeline.cpp
