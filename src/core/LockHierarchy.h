#pragma once

#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include <cassert>
#include "Logger.h"

namespace AISecurityVision {

/**
 * @brief Lock hierarchy levels to prevent deadlocks
 * 
 * Locks must be acquired in ascending order of their hierarchy level.
 * This prevents circular dependencies that can cause deadlocks.
 * 
 * Hierarchy (lowest to highest):
 * 1. MJPEG_PORT_MANAGER - Resource allocation level
 * 2. CROSS_CAMERA_TRACKING - Cross-component coordination
 * 3. ALARM_TRIGGER - Alarm system management
 * 4. TASK_MANAGER - System-wide pipeline management
 * 5. VIDEO_PIPELINE - Individual pipeline operations
 * 6. PERSON_STATS - High-level statistics processing
 */
enum class LockLevel : int {
    MJPEG_PORT_MANAGER = 1,
    CROSS_CAMERA_TRACKING = 2,
    ALARM_TRIGGER = 3,
    TASK_MANAGER = 4,
    VIDEO_PIPELINE = 5,
    PERSON_STATS = 6
};

/**
 * @brief Thread-safe lock hierarchy enforcer
 * 
 * This class tracks lock acquisition order per thread and enforces
 * that locks are acquired in the correct hierarchical order to
 * prevent deadlocks.
 * 
 * Features:
 * - Per-thread lock order tracking
 * - Deadlock detection and prevention
 * - Debug logging for lock violations
 * - Runtime assertion for development
 * - Minimal overhead in release builds
 */
class LockHierarchyEnforcer {
public:
    // Singleton access
    static LockHierarchyEnforcer& getInstance();
    
    // Delete copy constructor and assignment operator
    LockHierarchyEnforcer(const LockHierarchyEnforcer&) = delete;
    LockHierarchyEnforcer& operator=(const LockHierarchyEnforcer&) = delete;
    
    /**
     * @brief Check if acquiring a lock at the given level is safe
     * @param level Lock level to acquire
     * @param lockName Name of the lock for debugging
     * @return true if safe to acquire, false if would cause deadlock
     */
    bool canAcquireLock(LockLevel level, const std::string& lockName);
    
    /**
     * @brief Record that a lock has been acquired
     * @param level Lock level acquired
     * @param lockName Name of the lock for debugging
     */
    void recordLockAcquired(LockLevel level, const std::string& lockName);
    
    /**
     * @brief Record that a lock has been released
     * @param level Lock level released
     * @param lockName Name of the lock for debugging
     */
    void recordLockReleased(LockLevel level, const std::string& lockName);
    
    /**
     * @brief Get current lock level for this thread
     * @return Current highest lock level held by this thread
     */
    LockLevel getCurrentLockLevel() const;
    
    /**
     * @brief Check if thread holds any locks
     * @return true if thread holds locks
     */
    bool hasLocksHeld() const;
    
    /**
     * @brief Get debug information about locks held by current thread
     * @return String describing held locks
     */
    std::string getHeldLocksDebugInfo() const;
    
    /**
     * @brief Enable/disable lock hierarchy checking
     * @param enabled true to enable checking
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if lock hierarchy checking is enabled
     * @return true if enabled
     */
    bool isEnabled() const;

private:
    // Private constructor for singleton
    LockHierarchyEnforcer() = default;
    ~LockHierarchyEnforcer() = default;
    
    // Per-thread lock tracking
    struct ThreadLockInfo {
        std::vector<std::pair<LockLevel, std::string>> heldLocks;
        LockLevel currentMaxLevel = static_cast<LockLevel>(0);
    };
    
    // Thread-local storage for lock tracking
    mutable std::mutex m_mutex;
    std::unordered_map<std::thread::id, ThreadLockInfo> m_threadLocks;
    std::atomic<bool> m_enabled{true};
    
    // Helper methods
    ThreadLockInfo& getThreadLockInfo();
    const ThreadLockInfo& getThreadLockInfo() const;
    std::string lockLevelToString(LockLevel level) const;
};

/**
 * @brief RAII lock wrapper that enforces hierarchy
 * 
 * This class provides automatic lock hierarchy checking and
 * ensures proper cleanup even in exception scenarios.
 */
template<typename MutexType>
class HierarchicalLock {
public:
    /**
     * @brief Constructor - acquires lock with hierarchy checking
     * @param mutex Mutex to lock
     * @param level Hierarchy level of this lock
     * @param name Name of the lock for debugging
     */
    HierarchicalLock(MutexType& mutex, LockLevel level, const std::string& name)
        : m_mutex(mutex), m_level(level), m_name(name), m_locked(false) {
        
        auto& enforcer = LockHierarchyEnforcer::getInstance();
        
        if (enforcer.isEnabled()) {
            if (!enforcer.canAcquireLock(level, name)) {
                LOG_ERROR() << "[LockHierarchy] Potential deadlock detected! "
                           << "Cannot acquire lock '" << name << "' at level " << static_cast<int>(level)
                           << ". Current thread locks: " << enforcer.getHeldLocksDebugInfo();
                
                // In debug builds, assert to catch deadlocks early
                assert(false && "Lock hierarchy violation detected");
                
                // In release builds, log error but continue
                return;
            }
        }
        
        // Acquire the actual mutex
        m_mutex.lock();
        m_locked = true;
        
        if (enforcer.isEnabled()) {
            enforcer.recordLockAcquired(level, name);
        }
    }
    
    /**
     * @brief Destructor - releases lock and updates hierarchy
     */
    ~HierarchicalLock() {
        if (m_locked) {
            unlock();
        }
    }
    
    // Delete copy constructor and assignment operator
    HierarchicalLock(const HierarchicalLock&) = delete;
    HierarchicalLock& operator=(const HierarchicalLock&) = delete;
    
    /**
     * @brief Manually unlock the mutex
     */
    void unlock() {
        if (m_locked) {
            auto& enforcer = LockHierarchyEnforcer::getInstance();

            if (enforcer.isEnabled()) {
                enforcer.recordLockReleased(m_level, m_name);
            }

            m_mutex.unlock();
            m_locked = false;
        }
    }

    /**
     * @brief Manually lock the mutex (for compatibility with std::unique_lock interface)
     */
    void lock() {
        if (!m_locked) {
            auto& enforcer = LockHierarchyEnforcer::getInstance();

            if (enforcer.isEnabled()) {
                if (!enforcer.canAcquireLock(m_level, m_name)) {
                    LOG_ERROR() << "[LockHierarchy] Potential deadlock detected! "
                               << "Cannot acquire lock '" << m_name << "' at level " << static_cast<int>(m_level);
                    assert(false && "Lock hierarchy violation detected");
                    return;
                }
            }

            m_mutex.lock();
            m_locked = true;

            if (enforcer.isEnabled()) {
                enforcer.recordLockAcquired(m_level, m_name);
            }
        }
    }

    /**
     * @brief Check if lock is currently held
     * @return true if locked
     */
    bool isLocked() const {
        return m_locked;
    }

    /**
     * @brief Check if this lock owns the mutex (for compatibility with std::unique_lock)
     * @return true if locked
     */
    bool owns_lock() const {
        return m_locked;
    }

private:
    MutexType& m_mutex;
    LockLevel m_level;
    std::string m_name;
    bool m_locked;
};

// Convenience typedefs
using HierarchicalMutexLock = HierarchicalLock<std::mutex>;
using HierarchicalRecursiveLock = HierarchicalLock<std::recursive_mutex>;

// Convenience macros for common lock patterns
#define HIERARCHICAL_LOCK(mutex, level, name) \
    AISecurityVision::HierarchicalMutexLock lock_##__LINE__(mutex, level, name)

#define HIERARCHICAL_LOCK_GUARD(mutex, level, name) \
    AISecurityVision::HierarchicalMutexLock lock_guard_##__LINE__(mutex, level, name)

} // namespace AISecurityVision
