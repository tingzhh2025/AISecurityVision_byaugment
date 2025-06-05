#include "LockHierarchy.h"
#include <sstream>
#include <algorithm>

namespace AISecurityVision {

LockHierarchyEnforcer& LockHierarchyEnforcer::getInstance() {
    static LockHierarchyEnforcer instance;
    return instance;
}

bool LockHierarchyEnforcer::canAcquireLock(LockLevel level, const std::string& lockName) {
    if (!m_enabled.load()) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& threadInfo = getThreadLockInfo();
    
    // Check if we're trying to acquire a lock at a lower level than currently held
    if (!threadInfo.heldLocks.empty()) {
        LockLevel currentMaxLevel = threadInfo.currentMaxLevel;
        
        if (static_cast<int>(level) < static_cast<int>(currentMaxLevel)) {
            LOG_WARN() << "[LockHierarchy] Lock hierarchy violation detected! "
                      << "Attempting to acquire '" << lockName << "' at level " << static_cast<int>(level)
                      << " while holding locks at level " << static_cast<int>(currentMaxLevel);
            return false;
        }
        
        // Check for duplicate lock acquisition (potential recursive lock issue)
        for (const auto& heldLock : threadInfo.heldLocks) {
            if (heldLock.first == level && heldLock.second == lockName) {
                LOG_WARN() << "[LockHierarchy] Attempting to acquire already held lock: " << lockName;
                return false;
            }
        }
    }
    
    return true;
}

void LockHierarchyEnforcer::recordLockAcquired(LockLevel level, const std::string& lockName) {
    if (!m_enabled.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& threadInfo = getThreadLockInfo();
    
    threadInfo.heldLocks.emplace_back(level, lockName);
    
    // Update current max level
    if (threadInfo.heldLocks.empty() || static_cast<int>(level) > static_cast<int>(threadInfo.currentMaxLevel)) {
        threadInfo.currentMaxLevel = level;
    }
    
    LOG_DEBUG() << "[LockHierarchy] Thread " << std::this_thread::get_id() 
               << " acquired lock '" << lockName << "' at level " << static_cast<int>(level);
}

void LockHierarchyEnforcer::recordLockReleased(LockLevel level, const std::string& lockName) {
    if (!m_enabled.load()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& threadInfo = getThreadLockInfo();
    
    // Find and remove the lock from held locks
    auto it = std::find_if(threadInfo.heldLocks.begin(), threadInfo.heldLocks.end(),
        [level, &lockName](const std::pair<LockLevel, std::string>& heldLock) {
            return heldLock.first == level && heldLock.second == lockName;
        });
    
    if (it != threadInfo.heldLocks.end()) {
        threadInfo.heldLocks.erase(it);
        
        // Recalculate current max level
        if (threadInfo.heldLocks.empty()) {
            threadInfo.currentMaxLevel = static_cast<LockLevel>(0);
        } else {
            threadInfo.currentMaxLevel = std::max_element(threadInfo.heldLocks.begin(), 
                                                         threadInfo.heldLocks.end(),
                [](const auto& a, const auto& b) {
                    return static_cast<int>(a.first) < static_cast<int>(b.first);
                })->first;
        }
        
        LOG_DEBUG() << "[LockHierarchy] Thread " << std::this_thread::get_id() 
                   << " released lock '" << lockName << "' at level " << static_cast<int>(level);
    } else {
        LOG_WARN() << "[LockHierarchy] Attempted to release lock '" << lockName 
                  << "' that was not recorded as held";
    }
}

LockLevel LockHierarchyEnforcer::getCurrentLockLevel() const {
    if (!m_enabled.load()) {
        return static_cast<LockLevel>(0);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& threadInfo = getThreadLockInfo();
    return threadInfo.currentMaxLevel;
}

bool LockHierarchyEnforcer::hasLocksHeld() const {
    if (!m_enabled.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& threadInfo = getThreadLockInfo();
    return !threadInfo.heldLocks.empty();
}

std::string LockHierarchyEnforcer::getHeldLocksDebugInfo() const {
    if (!m_enabled.load()) {
        return "Lock hierarchy checking disabled";
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& threadInfo = getThreadLockInfo();
    
    if (threadInfo.heldLocks.empty()) {
        return "No locks held";
    }
    
    std::ostringstream oss;
    oss << "Held locks: ";
    for (size_t i = 0; i < threadInfo.heldLocks.size(); ++i) {
        if (i > 0) oss << ", ";
        const auto& heldLock = threadInfo.heldLocks[i];
        oss << heldLock.second << "(L" << static_cast<int>(heldLock.first) << ")";
    }
    
    return oss.str();
}

void LockHierarchyEnforcer::setEnabled(bool enabled) {
    m_enabled.store(enabled);
    LOG_INFO() << "[LockHierarchy] Lock hierarchy checking " 
               << (enabled ? "enabled" : "disabled");
}

bool LockHierarchyEnforcer::isEnabled() const {
    return m_enabled.load();
}

LockHierarchyEnforcer::ThreadLockInfo& LockHierarchyEnforcer::getThreadLockInfo() {
    std::thread::id threadId = std::this_thread::get_id();
    return m_threadLocks[threadId];
}

const LockHierarchyEnforcer::ThreadLockInfo& LockHierarchyEnforcer::getThreadLockInfo() const {
    std::thread::id threadId = std::this_thread::get_id();
    auto it = m_threadLocks.find(threadId);
    if (it != m_threadLocks.end()) {
        return it->second;
    }
    
    // Return empty thread info if not found
    static const ThreadLockInfo emptyInfo;
    return emptyInfo;
}

std::string LockHierarchyEnforcer::lockLevelToString(LockLevel level) const {
    switch (level) {
        case LockLevel::MJPEG_PORT_MANAGER: return "MJPEG_PORT_MANAGER";
        case LockLevel::CROSS_CAMERA_TRACKING: return "CROSS_CAMERA_TRACKING";
        case LockLevel::TASK_MANAGER: return "TASK_MANAGER";
        case LockLevel::VIDEO_PIPELINE: return "VIDEO_PIPELINE";
        case LockLevel::PERSON_STATS: return "PERSON_STATS";
        default: return "UNKNOWN";
    }
}

} // namespace AISecurityVision
