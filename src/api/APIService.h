#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <httplib.h>

// Forward declarations
class TaskManager;
class ONVIFManager;

// Network forward declarations
namespace AISecurityVision {
    class NetworkManager;
    class CameraController;
    class SystemController;
    class PersonStatsController;
    class AlertController;
    class NetworkController;
}

/**
 * @brief HTTP API Service for system control and monitoring
 *
 * Refactored modular architecture using controller pattern.
 * Delegates requests to specialized controllers:
 * - CameraController: Camera and video source management
 * - SystemController: System status and configuration
 * - PersonStatsController: Person analytics and statistics
 * - AlertController: Alert and alarm management
 * - NetworkController: Network interface management
 */
class APIService {
public:
    explicit APIService(int port = 8080);
    ~APIService();

    // Service control
    bool start();
    void stop();
    bool isRunning() const;

    // Configuration
    void setPort(int port);
    int getPort() const;

    // Delegate to CameraController for configuration management
    void clearInMemoryConfigurations();

private:
    // HTTP server implementation
    void serverThread();
    void setupRoutes();

    // Utility methods for response handling
    std::string stripHttpHeaders(const std::string& response);

    // Member variables
    int m_port;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;

    // HTTP server implementation
    std::unique_ptr<httplib::Server> m_httpServer;

    // Shared system components
    // Note: TaskManager is a singleton, accessed via TaskManager::getInstance()
    std::unique_ptr<ONVIFManager> m_onvifManager;
    std::unique_ptr<AISecurityVision::NetworkManager> m_networkManager;

    // Controller instances
    std::unique_ptr<AISecurityVision::CameraController> m_cameraController;
    std::unique_ptr<AISecurityVision::SystemController> m_systemController;
    std::unique_ptr<AISecurityVision::PersonStatsController> m_personStatsController;
    std::unique_ptr<AISecurityVision::AlertController> m_alertController;
    std::unique_ptr<AISecurityVision::NetworkController> m_networkController;
};
