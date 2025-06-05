#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <queue>

namespace AISecurityVision {

/**
 * @brief MJPEG Port Manager for dynamic port allocation
 * 
 * This class manages automatic allocation of MJPEG streaming ports
 * in the range 8090-8105 (16 ports total) for up to 16 cameras.
 * 
 * Features:
 * - Dynamic port allocation when cameras come online
 * - Automatic port release when cameras go offline
 * - Thread-safe port management
 * - Port reuse for optimal resource utilization
 * - Database synchronization for persistent storage
 */
class MJPEGPortManager {
public:
    // Port range constants
    static constexpr int MIN_PORT = 8090;
    static constexpr int MAX_PORT = 8105;
    static constexpr int MAX_CAMERAS = 16;

    // Singleton access
    static MJPEGPortManager& getInstance();

    // Delete copy constructor and assignment operator
    MJPEGPortManager(const MJPEGPortManager&) = delete;
    MJPEGPortManager& operator=(const MJPEGPortManager&) = delete;

    /**
     * @brief Allocate a port for a camera
     * @param cameraId Unique camera identifier
     * @return Allocated port number, or -1 if no ports available
     */
    int allocatePort(const std::string& cameraId);

    /**
     * @brief Release a port for a camera
     * @param cameraId Unique camera identifier
     * @return true if port was released, false if camera not found
     */
    bool releasePort(const std::string& cameraId);

    /**
     * @brief Get the allocated port for a camera
     * @param cameraId Unique camera identifier
     * @return Port number, or -1 if camera not found
     */
    int getPort(const std::string& cameraId) const;

    /**
     * @brief Check if a camera has an allocated port
     * @param cameraId Unique camera identifier
     * @return true if camera has allocated port
     */
    bool hasPort(const std::string& cameraId) const;

    /**
     * @brief Get all allocated ports and their camera IDs
     * @return Map of camera ID to port number
     */
    std::unordered_map<std::string, int> getAllAllocations() const;

    /**
     * @brief Get number of available ports
     * @return Number of unallocated ports
     */
    int getAvailablePortCount() const;

    /**
     * @brief Get number of allocated ports
     * @return Number of allocated ports
     */
    int getAllocatedPortCount() const;

    /**
     * @brief Check if port range is full
     * @return true if all ports are allocated
     */
    bool isFull() const;

    /**
     * @brief Clear all port allocations
     * Used for system reset or testing
     */
    void clearAllAllocations();

    /**
     * @brief Get list of available ports
     * @return Vector of unallocated port numbers
     */
    std::vector<int> getAvailablePorts() const;

    /**
     * @brief Reserve a specific port for a camera (if available)
     * @param cameraId Unique camera identifier
     * @param port Specific port to reserve
     * @return true if port was reserved, false if already in use
     */
    bool reserveSpecificPort(const std::string& cameraId, int port);

private:
    // Private constructor for singleton
    MJPEGPortManager();
    ~MJPEGPortManager() = default;

    // Thread safety
    mutable std::mutex m_mutex;

    // Port allocation tracking
    std::unordered_map<std::string, int> m_cameraToPort;  // camera ID -> port
    std::unordered_map<int, std::string> m_portToCamera;  // port -> camera ID
    std::unordered_set<int> m_allocatedPorts;             // set of allocated ports
    std::queue<int> m_availablePorts;                     // queue of available ports

    // Internal methods
    void initializeAvailablePorts();
    bool isValidPort(int port) const;
    int getNextAvailablePort();
    void returnPortToPool(int port);
};

} // namespace AISecurityVision
