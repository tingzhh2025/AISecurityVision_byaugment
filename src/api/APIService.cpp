#include "APIService.h"
#include "controllers/CameraController.h"
#include "controllers/SystemController.h"
#include "controllers/PersonStatsController.h"
#include "controllers/AlertController.h"
#include "controllers/NetworkController.h"
#include "../core/TaskManager.h"
#include "../onvif/ONVIFDiscovery.h"
#include "../network/NetworkManager.h"
#include "../core/Logger.h"
#include <sstream>

using namespace AISecurityVision;

APIService::APIService(int port) : m_port(port), m_httpServer(std::make_unique<httplib::Server>()) {
    LOG_INFO() << "[APIService] Initializing API service on port " << port;

    // Initialize shared system components
    // Note: TaskManager is a singleton, we'll access it via getInstance() when needed
    m_onvifManager = std::make_unique<ONVIFManager>();
    m_networkManager = std::make_unique<AISecurityVision::NetworkManager>();

    // Initialize ONVIF manager
    if (!m_onvifManager->initialize()) {
        LOG_ERROR() << "[APIService] Warning: Failed to initialize ONVIF manager: "
                  << m_onvifManager->getLastError();
    } else {
        LOG_INFO() << "[APIService] ONVIF discovery manager initialized";
    }

    // Initialize Network manager
    if (!m_networkManager->initialize()) {
        LOG_ERROR() << "[APIService] Warning: Failed to initialize Network manager: "
                  << m_networkManager->getLastError();
    } else {
        LOG_INFO() << "[APIService] Network manager initialized";
    }

    // Initialize controllers
    m_cameraController = std::make_unique<CameraController>();
    m_systemController = std::make_unique<SystemController>();
    m_personStatsController = std::make_unique<PersonStatsController>();
    m_alertController = std::make_unique<AlertController>();
    m_networkController = std::make_unique<NetworkController>();

    // Initialize controllers with shared components
    // TaskManager is a singleton, pass reference to the instance
    TaskManager& taskManager = TaskManager::getInstance();
    m_cameraController->initialize(&taskManager, m_onvifManager.get(), m_networkManager.get());
    m_systemController->initialize(&taskManager, m_onvifManager.get(), m_networkManager.get());
    m_personStatsController->initialize(&taskManager, m_onvifManager.get(), m_networkManager.get());
    m_alertController->initialize(&taskManager, m_onvifManager.get(), m_networkManager.get());
    m_networkController->initialize(&taskManager, m_onvifManager.get(), m_networkManager.get());

    LOG_INFO() << "[APIService] All controllers initialized";

    // Setup HTTP routes
    setupRoutes();
}

APIService::~APIService() {
    stop();
    LOG_INFO() << "[APIService] API service destroyed";
}

bool APIService::start() {
    if (m_running.load()) {
        LOG_WARN() << "[APIService] Service already running";
        return false;
    }

    LOG_INFO() << "[APIService] Starting HTTP server on port " << m_port;

    m_running.store(true);
    m_serverThread = std::thread(&APIService::serverThread, this);

    LOG_INFO() << "[APIService] API service started successfully";
    return true;
}

void APIService::stop() {
    if (!m_running.load()) {
        return;
    }

    LOG_INFO() << "[APIService] Stopping API service...";
    m_running.store(false);

    if (m_httpServer) {
        m_httpServer->stop();
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    LOG_INFO() << "[APIService] API service stopped";
}

bool APIService::isRunning() const {
    return m_running.load();
}

void APIService::setPort(int port) {
    if (m_running.load()) {
        LOG_WARN() << "[APIService] Cannot change port while service is running";
        return;
    }
    m_port = port;
}

int APIService::getPort() const {
    return m_port;
}

void APIService::clearInMemoryConfigurations() {
    LOG_INFO() << "[APIService] Clearing in-memory camera configurations";
    if (m_cameraController) {
        m_cameraController->clearInMemoryConfigurations();
    }
    LOG_INFO() << "[APIService] In-memory camera configurations cleared";
}

void APIService::serverThread() {
    try {
        LOG_INFO() << "[APIService] Server thread starting on port " << m_port;

        if (!m_httpServer->listen("0.0.0.0", m_port)) {
            LOG_ERROR() << "[APIService] Failed to start HTTP server on port " << m_port;
            m_running.store(false);
            return;
        }

        LOG_INFO() << "[APIService] HTTP server started successfully";
    } catch (const std::exception& e) {
        LOG_ERROR() << "[APIService] Server thread exception: " << e.what();
        m_running.store(false);
    }
}

void APIService::setupRoutes() {
    if (!m_httpServer) {
        LOG_ERROR() << "[APIService] HTTP server not initialized";
        return;
    }

    LOG_INFO() << "[APIService] Setting up HTTP routes...";

    // Handle OPTIONS requests for CORS preflight
    m_httpServer->Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        LOG_INFO() << "[APIService] OPTIONS request for: " << req.path;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 200;
        LOG_INFO() << "[APIService] OPTIONS response sent";
    });

    // System endpoints - delegate to SystemController
    m_httpServer->Get("/api/system/status", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetStatus("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

    m_httpServer->Get("/api/system/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetSystemMetrics("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/system/pipeline-stats", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetPipelineStats("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetSystemStats("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/system/info", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetSystemInfo("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/system/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handleGetSystemConfig("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/system/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_systemController->handlePostSystemConfig(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    // Camera and video source management - delegate to CameraController
    m_httpServer->Post("/api/source/add", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handlePostVideoSource(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/source/list", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handleGetVideoSources("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/cameras", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handleGetCameras("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/cameras", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handlePostVideoSource(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/cameras/test-connection", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handleTestCameraConnection(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    // ONVIF discovery endpoints - delegate to CameraController
    m_httpServer->Get("/api/source/discover", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handleGetDiscoverDevices("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/source/add-discovered", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_cameraController->handlePostAddDiscoveredDevice(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    // Alert and alarm management - delegate to AlertController
    m_httpServer->Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_alertController->handleGetAlerts("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/alarms/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_alertController->handlePostAlarmConfig(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/alarms/config", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_alertController->handleGetAlarmConfigs("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/alarms/test", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_alertController->handlePostTestAlarm(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/alarms/status", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_alertController->handleGetAlarmStatus("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    // Network management - delegate to NetworkController
    m_httpServer->Get("/api/network/interfaces", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_networkController->handleGetNetworkInterfaces("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Get("/api/network/stats", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_networkController->handleGetNetworkStats("", response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    m_httpServer->Post("/api/network/test", [this](const httplib::Request& req, httplib::Response& res) {
        std::string response;
        m_networkController->handlePostNetworkTest(req.body, response);
        res.set_content(stripHttpHeaders(response), "application/json");
    });

    LOG_INFO() << "[APIService] HTTP routes configured successfully";
}

std::string APIService::stripHttpHeaders(const std::string& response) {
    size_t contentStart = response.find("\r\n\r\n");
    if (contentStart != std::string::npos) {
        return response.substr(contentStart + 4);
    }
    return response;
}

// Note: All handler methods have been moved to their respective controllers:
// - CameraController: Camera management, ONVIF discovery, face management, ROI/rules, streaming
// - SystemController: System status, metrics, configuration, web interface
// - PersonStatsController: Person analytics and statistics
// - AlertController: Alert and alarm management
// - NetworkController: Network interface management
