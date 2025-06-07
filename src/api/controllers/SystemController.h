#pragma once

#include "BaseController.h"
#include <string>

namespace AISecurityVision {

/**
 * @brief Controller for system-wide operations
 * 
 * Handles system-level functionality including:
 * - System status and health monitoring
 * - Global configuration management
 * - System information retrieval
 * - Performance metrics
 * - Pipeline statistics
 */
class SystemController : public BaseController {
public:
    SystemController() = default;
    ~SystemController() override = default;

    // System status and monitoring endpoints
    void handleGetStatus(const std::string& request, std::string& response);
    void handleGetSystemInfo(const std::string& request, std::string& response);
    void handleGetSystemMetrics(const std::string& request, std::string& response);
    void handleGetPipelineStats(const std::string& request, std::string& response);
    void handleGetSystemStats(const std::string& request, std::string& response);

    // Configuration management endpoints
    void handleGetSystemConfig(const std::string& request, std::string& response);
    void handlePostSystemConfig(const std::string& request, std::string& response);
    void handleGetConfigCategory(const std::string& category, std::string& response);

    // Web dashboard handlers
    void handleGetDashboard(const std::string& request, std::string& response);
    void handleStaticFile(const std::string& request, std::string& response, const std::string& filePath);

private:
    std::string getControllerName() const override { return "SystemController"; }

    // System information utilities
    std::string getPlatformInfo() const;
    std::string getMemoryInfo() const;

    // File serving utilities
    std::string readFile(const std::string& filePath);
    std::string getMimeType(const std::string& filePath);
    std::string createFileResponse(const std::string& content, const std::string& mimeType, int statusCode = 200);
    bool fileExists(const std::string& filePath);
    std::string loadWebFile(const std::string& filePath);
};

} // namespace AISecurityVision
