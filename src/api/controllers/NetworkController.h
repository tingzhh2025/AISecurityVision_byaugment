#pragma once

#include "BaseController.h"
#include <string>
#include <vector>

// Forward declarations
namespace AISecurityVision {
    struct NetworkInterface;
    struct NetworkConfiguration;
}

namespace AISecurityVision {

/**
 * @brief Controller for network-related configurations
 * 
 * Handles network management functionality including:
 * - Network interface management
 * - Port configurations
 * - Connection settings
 * - Network testing
 */
class NetworkController : public BaseController {
public:
    NetworkController() = default;
    ~NetworkController() override = default;

    // Network interface management handlers
    void handleGetNetworkInterfaces(const std::string& request, std::string& response);
    void handleGetNetworkInterface(const std::string& request, std::string& response, const std::string& interfaceName);
    void handlePostNetworkInterface(const std::string& request, std::string& response, const std::string& interfaceName);
    void handlePostNetworkInterfaceEnable(const std::string& request, std::string& response, const std::string& interfaceName);
    void handlePostNetworkInterfaceDisable(const std::string& request, std::string& response, const std::string& interfaceName);
    void handleGetNetworkStats(const std::string& request, std::string& response);
    void handlePostNetworkTest(const std::string& request, std::string& response);

private:
    std::string getControllerName() const override { return "NetworkController"; }

    // Network interface serialization
    std::string serializeNetworkInterface(const AISecurityVision::NetworkInterface& interface);
    std::string serializeNetworkInterfaceList(const std::vector<AISecurityVision::NetworkInterface>& interfaces);
    std::string serializeNetworkConfiguration(const AISecurityVision::NetworkConfiguration& config);
    bool deserializeNetworkConfiguration(const std::string& json, AISecurityVision::NetworkConfiguration& config);
};

} // namespace AISecurityVision
