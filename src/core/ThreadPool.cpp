#include "ThreadPool.h"
#include <algorithm>

namespace AISecurityVision {

ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) {
        numThreads = std::max(1u, std::thread::hardware_concurrency());
    }
    
    LOG_INFO() << "[ThreadPool] Initializing thread pool with " << numThreads << " worker threads";
    
    // Create worker threads
    m_workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&ThreadPool::workerThread, this);
    }
    
    LOG_INFO() << "[ThreadPool] Thread pool initialized successfully";
}

ThreadPool::~ThreadPool() {
    LOG_INFO() << "[ThreadPool] Shutting down thread pool";
    shutdown();
}

void ThreadPool::workerThread() {
    LOG_DEBUG() << "[ThreadPool] Worker thread " << std::this_thread::get_id() << " started";
    
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // Wait for task or shutdown signal
            m_condition.wait(lock, [this] {
                return m_stop.load() || !m_tasks.empty();
            });
            
            // Check for shutdown conditions
            if (m_stop.load()) {
                if (m_forceStop.load() || m_tasks.empty()) {
                    LOG_DEBUG() << "[ThreadPool] Worker thread " << std::this_thread::get_id() << " shutting down";
                    break;
                }
                // Continue processing remaining tasks for graceful shutdown
            }
            
            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
                m_activeTasks.fetch_add(1);
            }
        }
        
        // Execute task outside of lock
        if (task) {
            try {
                task();
                m_completedTasks.fetch_add(1);
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadPool] Worker thread " << std::this_thread::get_id() 
                           << " caught exception: " << e.what();
                m_failedTasks.fetch_add(1);
            } catch (...) {
                LOG_ERROR() << "[ThreadPool] Worker thread " << std::this_thread::get_id() 
                           << " caught unknown exception";
                m_failedTasks.fetch_add(1);
            }
            
            m_activeTasks.fetch_sub(1);
        }
    }
    
    LOG_DEBUG() << "[ThreadPool] Worker thread " << std::this_thread::get_id() << " terminated";
}

size_t ThreadPool::getThreadCount() const {
    return m_workers.size();
}

size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_tasks.size();
}

bool ThreadPool::isShuttingDown() const {
    return m_stop.load();
}

void ThreadPool::shutdown() {
    LOG_INFO() << "[ThreadPool] Initiating graceful shutdown";
    
    // Signal shutdown
    m_stop.store(true);
    m_condition.notify_all();
    
    // Wait for all worker threads to complete
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // Log final statistics
    size_t completed = m_completedTasks.load();
    size_t failed = m_failedTasks.load();
    size_t remaining = getQueueSize();
    
    LOG_INFO() << "[ThreadPool] Shutdown complete. Statistics: "
               << "Completed: " << completed << ", "
               << "Failed: " << failed << ", "
               << "Remaining: " << remaining;
    
    if (remaining > 0) {
        LOG_WARN() << "[ThreadPool] " << remaining << " tasks were not completed during shutdown";
    }
}

void ThreadPool::forceShutdown() {
    LOG_WARN() << "[ThreadPool] Initiating force shutdown";
    
    // Signal immediate shutdown
    m_stop.store(true);
    m_forceStop.store(true);
    m_condition.notify_all();
    
    // Clear pending tasks
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        size_t discarded = m_tasks.size();
        std::queue<std::function<void()>> empty;
        m_tasks.swap(empty);
        
        if (discarded > 0) {
            LOG_WARN() << "[ThreadPool] Discarded " << discarded << " pending tasks during force shutdown";
        }
    }
    
    // Wait for worker threads with timeout
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    LOG_INFO() << "[ThreadPool] Force shutdown complete";
}

} // namespace AISecurityVision
