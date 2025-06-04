#include "NetworkController.h"
#include "../../network/NetworkManager.h"
#include <nlohmann/json.hpp>
#include <sstream>

using namespace AISecurityVision;

void NetworkController::handleGetNetworkInterfaces(const std::string& request, std::string& response) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        auto interfaces = m_networkManager->getAllInterfaces();
        response = createJsonResponse(serializeNetworkInterfaceList(interfaces));

        logInfo("Retrieved " + std::to_string(interfaces.size()) + " network interfaces");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get network interfaces: " + std::string(e.what()), 500);
    }
}

void NetworkController::handleGetNetworkInterface(const std::string& request, std::string& response, const std::string& interfaceName) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        if (interfaceName.empty()) {
            response = createErrorResponse("Interface name is required", 400);
            return;
        }

        auto interface = m_networkManager->getInterface(interfaceName);
        if (interface.name.empty()) {
            response = createErrorResponse("Interface not found: " + interfaceName, 404);
            return;
        }

        response = createJsonResponse(serializeNetworkInterface(interface));
        logInfo("Retrieved network interface: " + interfaceName);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get network interface: " + std::string(e.what()), 500);
    }
}

void NetworkController::handlePostNetworkInterface(const std::string& request, std::string& response, const std::string& interfaceName) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        AISecurityVision::NetworkConfiguration config;
        if (!deserializeNetworkConfiguration(request, config)) {
            response = createErrorResponse("Invalid network configuration", 400);
            return;
        }

        config.interfaceName = interfaceName;

        if (!m_networkManager->configureInterface(config)) {
            response = createErrorResponse("Failed to configure interface: " + m_networkManager->getLastError(), 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Interface configured successfully\","
             << "\"interface\":\"" << interfaceName << "\","
             << "\"configured_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Configured network interface: " + interfaceName);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to configure network interface: " + std::string(e.what()), 500);
    }
}

void NetworkController::handlePostNetworkInterfaceEnable(const std::string& request, std::string& response, const std::string& interfaceName) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        if (interfaceName.empty()) {
            response = createErrorResponse("Interface name is required", 400);
            return;
        }

        if (!m_networkManager->setInterfaceEnabled(interfaceName, true)) {
            response = createErrorResponse("Failed to enable interface: " + m_networkManager->getLastError(), 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Interface enabled successfully\","
             << "\"interface\":\"" << interfaceName << "\","
             << "\"enabled_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Enabled network interface: " + interfaceName);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to enable network interface: " + std::string(e.what()), 500);
    }
}

void NetworkController::handlePostNetworkInterfaceDisable(const std::string& request, std::string& response, const std::string& interfaceName) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        if (interfaceName.empty()) {
            response = createErrorResponse("Interface name is required", 400);
            return;
        }

        if (!m_networkManager->setInterfaceEnabled(interfaceName, false)) {
            response = createErrorResponse("Failed to disable interface: " + m_networkManager->getLastError(), 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Interface disabled successfully\","
             << "\"interface\":\"" << interfaceName << "\","
             << "\"disabled_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Disabled network interface: " + interfaceName);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to disable network interface: " + std::string(e.what()), 500);
    }
}

void NetworkController::handleGetNetworkStats(const std::string& request, std::string& response) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        auto stats = m_networkManager->getNetworkStats();
        auto interfaces = m_networkManager->getAllInterfaces();

        // Calculate totals from interfaces
        uint64_t totalRxBytes = 0, totalTxBytes = 0;
        int totalInterfaces = interfaces.size();
        int activeInterfaces = 0;

        for (const auto& interface : interfaces) {
            totalRxBytes += interface.bytesReceived;
            totalTxBytes += interface.bytesSent;
            if (interface.isUp && interface.isConnected) {
                activeInterfaces++;
            }
        }

        std::ostringstream json;
        json << "{"
             << "\"total_interfaces\":" << totalInterfaces << ","
             << "\"active_interfaces\":" << activeInterfaces << ","
             << "\"total_rx_bytes\":" << totalRxBytes << ","
             << "\"total_tx_bytes\":" << totalTxBytes << ","
             << "\"total_rx_packets\":0,"  // Not available in current implementation
             << "\"total_tx_packets\":0,"  // Not available in current implementation
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved network statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get network stats: " + std::string(e.what()), 500);
    }
}

void NetworkController::handlePostNetworkTest(const std::string& request, std::string& response) {
    try {
        if (!m_networkManager) {
            response = createErrorResponse("Network manager not initialized", 500);
            return;
        }

        std::string host = parseJsonField(request, "host");
        int timeout = parseJsonInt(request, "timeout", 5);

        if (host.empty()) {
            response = createErrorResponse("host is required", 400);
            return;
        }

        bool success = m_networkManager->pingTest(host, timeout);

        std::ostringstream json;
        json << "{"
             << "\"test_result\":" << (success ? "true" : "false") << ","
             << "\"host\":\"" << host << "\","
             << "\"timeout\":" << timeout << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Network test completed for host: " + host + " (result: " + (success ? "success" : "failed") + ")");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to perform network test: " + std::string(e.what()), 500);
    }
}

std::string NetworkController::serializeNetworkInterface(const AISecurityVision::NetworkInterface& interface) {
    std::ostringstream json;
    json << "{"
         << "\"name\":\"" << interface.name << "\","
         << "\"type\":\"" << interface.type << "\","
         << "\"status\":\"" << interface.status << "\","
         << "\"ip_address\":\"" << interface.ipAddress << "\","
         << "\"netmask\":\"" << interface.netmask << "\","
         << "\"gateway\":\"" << interface.gateway << "\","
         << "\"mac_address\":\"" << interface.macAddress << "\","
         << "\"mtu\":1500,"  // Default MTU, not available in current NetworkInterface
         << "\"rx_bytes\":" << interface.bytesReceived << ","
         << "\"tx_bytes\":" << interface.bytesSent << ","
         << "\"rx_packets\":0,"  // Not available in current NetworkInterface
         << "\"tx_packets\":0"   // Not available in current NetworkInterface
         << "}";
    return json.str();
}

std::string NetworkController::serializeNetworkInterfaceList(const std::vector<AISecurityVision::NetworkInterface>& interfaces) {
    std::ostringstream json;
    json << "{"
         << "\"interfaces\":[";

    for (size_t i = 0; i < interfaces.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeNetworkInterface(interfaces[i]);
    }

    json << "],"
         << "\"total\":" << interfaces.size() << ","
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
         << "}";

    return json.str();
}

bool NetworkController::deserializeNetworkConfiguration(const std::string& json, AISecurityVision::NetworkConfiguration& config) {
    try {
        auto j = nlohmann::json::parse(json);

        config.interfaceName = j.value("interface_name", "");
        config.isDhcp = j.value("dhcp", true);
        config.ipAddress = j.value("ip_address", "");
        config.netmask = j.value("netmask", "");
        config.gateway = j.value("gateway", "");
        config.dns1 = j.value("dns1", "");
        config.dns2 = j.value("dns2", "");
        // Note: mtu field not available in NetworkConfiguration struct

        return true;
    } catch (const std::exception& e) {
        logError("Failed to deserialize network configuration: " + std::string(e.what()));
        return false;
    }
}
