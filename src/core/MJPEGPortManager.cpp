#include "MJPEGPortManager.h"
#include "Logger.h"
#include <algorithm>

namespace AISecurityVision {

MJPEGPortManager& MJPEGPortManager::getInstance() {
    static MJPEGPortManager instance;
    return instance;
}

MJPEGPortManager::MJPEGPortManager() {
    initializeAvailablePorts();
    LOG_INFO() << "[MJPEGPortManager] Initialized with port range " << MIN_PORT << "-" << MAX_PORT;
}

void MJPEGPortManager::initializeAvailablePorts() {
    // Initialize available ports queue with all ports in range
    for (int port = MIN_PORT; port <= MAX_PORT; ++port) {
        m_availablePorts.push(port);
    }
}

bool MJPEGPortManager::isValidPort(int port) const {
    return port >= MIN_PORT && port <= MAX_PORT;
}

int MJPEGPortManager::allocatePort(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if camera already has a port
    auto it = m_cameraToPort.find(cameraId);
    if (it != m_cameraToPort.end()) {
        LOG_DEBUG() << "[MJPEGPortManager] Camera " << cameraId << " already has port " << it->second;
        return it->second;
    }

    // Check if we have available ports
    if (m_availablePorts.empty()) {
        LOG_ERROR() << "[MJPEGPortManager] No available ports for camera " << cameraId;
        return -1;
    }

    // Get next available port
    int port = getNextAvailablePort();
    if (port == -1) {
        LOG_ERROR() << "[MJPEGPortManager] Failed to get available port for camera " << cameraId;
        return -1;
    }

    // Allocate port to camera
    m_cameraToPort[cameraId] = port;
    m_portToCamera[port] = cameraId;
    m_allocatedPorts.insert(port);

    LOG_INFO() << "[MJPEGPortManager] Allocated port " << port << " to camera " << cameraId
               << " (" << getAllocatedPortCount() << "/" << MAX_CAMERAS << " ports used)";

    return port;
}

bool MJPEGPortManager::releasePort(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find camera's port
    auto it = m_cameraToPort.find(cameraId);
    if (it == m_cameraToPort.end()) {
        LOG_WARN() << "[MJPEGPortManager] Camera " << cameraId << " has no allocated port";
        return false;
    }

    int port = it->second;

    // Remove allocations
    m_cameraToPort.erase(it);
    m_portToCamera.erase(port);
    m_allocatedPorts.erase(port);

    // Return port to available pool
    returnPortToPool(port);

    LOG_INFO() << "[MJPEGPortManager] Released port " << port << " from camera " << cameraId
               << " (" << getAllocatedPortCount() << "/" << MAX_CAMERAS << " ports used)";

    return true;
}

int MJPEGPortManager::getPort(const std::string& cameraId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cameraToPort.find(cameraId);
    return (it != m_cameraToPort.end()) ? it->second : -1;
}

bool MJPEGPortManager::hasPort(const std::string& cameraId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cameraToPort.find(cameraId) != m_cameraToPort.end();
}

std::unordered_map<std::string, int> MJPEGPortManager::getAllAllocations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cameraToPort;
}

int MJPEGPortManager::getAvailablePortCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_availablePorts.size());
}

int MJPEGPortManager::getAllocatedPortCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_allocatedPorts.size());
}

bool MJPEGPortManager::isFull() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_allocatedPorts.size() >= MAX_CAMERAS;
}

void MJPEGPortManager::clearAllAllocations() {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_INFO() << "[MJPEGPortManager] Clearing all port allocations";

    m_cameraToPort.clear();
    m_portToCamera.clear();
    m_allocatedPorts.clear();

    // Reinitialize available ports
    while (!m_availablePorts.empty()) {
        m_availablePorts.pop();
    }
    initializeAvailablePorts();
}

std::vector<int> MJPEGPortManager::getAvailablePorts() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<int> available;
    std::queue<int> tempQueue = m_availablePorts;
    
    while (!tempQueue.empty()) {
        available.push_back(tempQueue.front());
        tempQueue.pop();
    }

    return available;
}

bool MJPEGPortManager::reserveSpecificPort(const std::string& cameraId, int port) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidPort(port)) {
        LOG_ERROR() << "[MJPEGPortManager] Invalid port " << port << " for camera " << cameraId;
        return false;
    }

    // Check if port is already allocated
    if (m_allocatedPorts.find(port) != m_allocatedPorts.end()) {
        LOG_ERROR() << "[MJPEGPortManager] Port " << port << " already allocated to camera " 
                   << m_portToCamera[port];
        return false;
    }

    // Check if camera already has a port
    auto it = m_cameraToPort.find(cameraId);
    if (it != m_cameraToPort.end()) {
        LOG_WARN() << "[MJPEGPortManager] Camera " << cameraId << " already has port " << it->second;
        return false;
    }

    // Remove port from available queue
    std::queue<int> tempQueue;
    bool found = false;
    while (!m_availablePorts.empty()) {
        int availablePort = m_availablePorts.front();
        m_availablePorts.pop();
        if (availablePort == port) {
            found = true;
        } else {
            tempQueue.push(availablePort);
        }
    }
    m_availablePorts = tempQueue;

    if (!found) {
        LOG_ERROR() << "[MJPEGPortManager] Port " << port << " not available for reservation";
        return false;
    }

    // Allocate specific port to camera
    m_cameraToPort[cameraId] = port;
    m_portToCamera[port] = cameraId;
    m_allocatedPorts.insert(port);

    LOG_INFO() << "[MJPEGPortManager] Reserved specific port " << port << " for camera " << cameraId;

    return true;
}

int MJPEGPortManager::getNextAvailablePort() {
    if (m_availablePorts.empty()) {
        return -1;
    }

    int port = m_availablePorts.front();
    m_availablePorts.pop();
    return port;
}

void MJPEGPortManager::returnPortToPool(int port) {
    if (isValidPort(port)) {
        m_availablePorts.push(port);
    }
}

} // namespace AISecurityVision
