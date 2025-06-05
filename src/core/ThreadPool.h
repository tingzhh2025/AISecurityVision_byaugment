#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include "Logger.h"

namespace AISecurityVision {

/**
 * @brief Thread-safe thread pool for managing asynchronous operations
 * 
 * This class provides a safe alternative to detached threads with proper
 * resource management, error handling, and graceful shutdown capabilities.
 * 
 * Features:
 * - Configurable number of worker threads
 * - Task queue with proper synchronization
 * - Future-based result handling
 * - Graceful shutdown with task completion
 * - Exception safety and error propagation
 * - Resource cleanup and leak prevention
 */
class ThreadPool {
public:
    /**
     * @brief Constructor
     * @param numThreads Number of worker threads (default: hardware concurrency)
     */
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    
    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~ThreadPool();
    
    // Delete copy constructor and assignment operator
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    /**
     * @brief Submit a task to the thread pool
     * @param f Function to execute
     * @param args Arguments to pass to the function
     * @return Future object for result retrieval
     */
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    /**
     * @brief Submit a task without return value tracking
     * @param f Function to execute
     * @param args Arguments to pass to the function
     */
    template<class F, class... Args>
    void submitDetached(F&& f, Args&&... args);
    
    /**
     * @brief Get number of active worker threads
     * @return Number of worker threads
     */
    size_t getThreadCount() const;
    
    /**
     * @brief Get number of pending tasks in queue
     * @return Number of queued tasks
     */
    size_t getQueueSize() const;
    
    /**
     * @brief Check if thread pool is shutting down
     * @return true if shutdown initiated
     */
    bool isShuttingDown() const;
    
    /**
     * @brief Initiate graceful shutdown
     * Waits for all current tasks to complete
     */
    void shutdown();
    
    /**
     * @brief Force immediate shutdown
     * Cancels pending tasks and stops workers
     */
    void forceShutdown();

private:
    // Worker thread function
    void workerThread();
    
    // Thread management
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    
    // Synchronization
    mutable std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_forceStop{false};
    
    // Statistics
    std::atomic<size_t> m_activeTasks{0};
    std::atomic<size_t> m_completedTasks{0};
    std::atomic<size_t> m_failedTasks{0};
};

// Template implementation
template<class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    if (m_stop.load()) {
        throw std::runtime_error("ThreadPool is shutting down - cannot submit new tasks");
    }
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        
        if (m_stop.load()) {
            throw std::runtime_error("ThreadPool is shutting down - cannot submit new tasks");
        }
        
        m_tasks.emplace([task]() {
            try {
                (*task)();
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadPool] Task execution failed: " << e.what();
            } catch (...) {
                LOG_ERROR() << "[ThreadPool] Task execution failed with unknown exception";
            }
        });
    }
    
    m_condition.notify_one();
    return result;
}

template<class F, class... Args>
void ThreadPool::submitDetached(F&& f, Args&&... args) {
    if (m_stop.load()) {
        LOG_WARN() << "[ThreadPool] Cannot submit detached task - pool is shutting down";
        return;
    }
    
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        
        if (m_stop.load()) {
            LOG_WARN() << "[ThreadPool] Cannot submit detached task - pool is shutting down";
            return;
        }
        
        m_tasks.emplace([f = std::forward<F>(f), args...]() mutable {
            try {
                f(args...);
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadPool] Detached task execution failed: " << e.what();
            } catch (...) {
                LOG_ERROR() << "[ThreadPool] Detached task execution failed with unknown exception";
            }
        });
    }
    
    m_condition.notify_one();
}

} // namespace AISecurityVision
