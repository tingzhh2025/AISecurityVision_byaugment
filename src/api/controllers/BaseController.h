#pragma once

#include <string>
#include <memory>
#include <vector>
#include <httplib.h>
#include <opencv2/opencv.hpp>
#include "../../core/Logger.h"

// Forward declarations
class TaskManager;
class ONVIFManager;
namespace AISecurityVision {
    class NetworkManager;
}

namespace AISecurityVision {

/**
 * @brief Base class for all API controllers
 *
 * Provides common functionality including:
 * - HTTP response utilities
 * - JSON parsing and serialization
 * - Error handling
 * - Logging patterns
 * - CORS header management
 * - Access to shared system components
 */
class BaseController {
public:
    BaseController() = default;
    virtual ~BaseController() = default;

    // Disable copy constructor and assignment operator
    BaseController(const BaseController&) = delete;
    BaseController& operator=(const BaseController&) = delete;

    // Initialize controller with shared components
    virtual void initialize(TaskManager* taskManager,
                          ONVIFManager* onvifManager,
                          AISecurityVision::NetworkManager* networkManager);

protected:
    // HTTP response utilities
    std::string createJsonResponse(const std::string& data, int statusCode = 200);
    std::string createErrorResponse(const std::string& error, int statusCode = 400);
    std::string createSuccessResponse(const std::string& message = "Operation completed successfully");

    // JSON parsing utilities
    std::string parseJsonField(const std::string& json, const std::string& field);
    int parseJsonInt(const std::string& json, const std::string& field, int defaultValue = 0);
    float parseJsonFloat(const std::string& json, const std::string& field, float defaultValue = 0.0f);
    bool parseJsonBool(const std::string& json, const std::string& field, bool defaultValue = false);

    // Utility methods
    std::string getCurrentTimestamp();
    void setCorsHeaders(httplib::Response& res);
    std::string stripHttpHeaders(const std::string& response);

    // Validation utilities
    bool isValidCameraId(const std::string& cameraId);
    bool isValidJson(const std::string& json);

    // Logging utilities with controller context
    void logInfo(const std::string& message, const std::string& context = "");
    void logWarn(const std::string& message, const std::string& context = "");
    void logError(const std::string& message, const std::string& context = "");
    void logDebug(const std::string& message, const std::string& context = "");

    // Access to shared system components
    TaskManager* m_taskManager = nullptr;
    ONVIFManager* m_onvifManager = nullptr;
    AISecurityVision::NetworkManager* m_networkManager = nullptr;

private:
    // Get controller name for logging (implemented by derived classes)
    virtual std::string getControllerName() const = 0;
};

} // namespace AISecurityVision
