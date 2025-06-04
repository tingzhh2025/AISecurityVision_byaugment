#pragma once

#include "BaseController.h"
#include <string>
#include <vector>

// Forward declarations
struct AlarmConfig;

namespace AISecurityVision {

/**
 * @brief Controller for alert and notification management
 * 
 * Handles alert and alarm functionality including:
 * - Alert configuration
 * - Alert retrieval and filtering
 * - Notification settings
 * - Alert acknowledgment
 * - Alarm testing
 */
class AlertController : public BaseController {
public:
    AlertController() = default;
    ~AlertController() override = default;

    // Alarm configuration handlers
    void handlePostAlarmConfig(const std::string& request, std::string& response);
    void handleGetAlarmConfigs(const std::string& request, std::string& response);
    void handleGetAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handlePutAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handleDeleteAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handlePostTestAlarm(const std::string& request, std::string& response);
    void handleGetAlarmStatus(const std::string& request, std::string& response);

    // Alert retrieval handlers
    void handleGetAlerts(const std::string& request, std::string& response);

private:
    std::string getControllerName() const override { return "AlertController"; }

    // JSON serialization for alarm configurations
    std::string serializeAlarmConfig(const struct AlarmConfig& config);
    std::string serializeAlarmConfigList(const std::vector<struct AlarmConfig>& configs);

    // JSON deserialization for alarm configurations
    bool deserializeAlarmConfig(const std::string& json, struct AlarmConfig& config);
};

} // namespace AISecurityVision
