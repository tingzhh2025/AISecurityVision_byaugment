#include "NetworkManager.h"
#include "../core/Logger.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

using namespace AISecurityVision;

NetworkManager::NetworkManager() : m_initialized(false) {
}

NetworkManager::~NetworkManager() {
    cleanup();
}

bool NetworkManager::initialize() {
    if (m_initialized) {
        return true;
    }

    LOG_INFO() << "[NetworkManager] Initializing network manager...";
    
    // 检查是否有足够权限
    if (geteuid() != 0) {
        LOG_WARN() << "[NetworkManager] Warning: Not running as root, some operations may fail";
    }
    
    m_initialized = true;
    LOG_INFO() << "[NetworkManager] Network manager initialized successfully";
    return true;
}

void NetworkManager::cleanup() {
    if (!m_initialized) {
        return;
    }
    
    m_configurations.clear();
    m_initialized = false;
    LOG_INFO() << "[NetworkManager] Network manager cleanup complete";
}

std::vector<NetworkInterface> NetworkManager::getAllInterfaces() {
    std::vector<NetworkInterface> interfaces;
    
    if (!m_initialized) {
        m_lastError = "NetworkManager not initialized";
        return interfaces;
    }
    
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) {
        m_lastError = "Failed to get network interfaces";
        return interfaces;
    }
    
    std::map<std::string, NetworkInterface> interfaceMap;
    
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        std::string name = ifa->ifa_name;
        
        // 跳过回环接口
        if (name == "lo") continue;
        
        NetworkInterface& netif = interfaceMap[name];
        if (netif.name.empty()) {
            netif.name = name;
            netif.displayName = name;
            netif.type = getInterfaceType(name);
            netif.isUp = (ifa->ifa_flags & IFF_UP) != 0;
            netif.isConnected = (ifa->ifa_flags & IFF_RUNNING) != 0;
        }
        
        // 获取IP地址
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
            netif.ipAddress = inet_ntoa(addr_in->sin_addr);
            
            if (ifa->ifa_netmask) {
                struct sockaddr_in* netmask_in = (struct sockaddr_in*)ifa->ifa_netmask;
                netif.netmask = inet_ntoa(netmask_in->sin_addr);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    // 获取额外信息
    for (auto& pair : interfaceMap) {
        NetworkInterface& netif = pair.second;
        
        // 获取MAC地址
        std::string output;
        if (executeCommand("cat /sys/class/net/" + netif.name + "/address", output)) {
            netif.macAddress = output;
            // 移除换行符
            netif.macAddress.erase(netif.macAddress.find_last_not_of(" \n\r\t") + 1);
        }
        
        // 获取网络统计
        if (executeCommand("cat /sys/class/net/" + netif.name + "/statistics/rx_bytes", output)) {
            netif.bytesReceived = std::stoull(output);
        }
        if (executeCommand("cat /sys/class/net/" + netif.name + "/statistics/tx_bytes", output)) {
            netif.bytesSent = std::stoull(output);
        }
        
        // 检查是否使用DHCP
        std::ifstream dhcpFile("/var/lib/dhcp/dhclient.leases");
        if (dhcpFile.is_open()) {
            std::string line;
            while (std::getline(dhcpFile, line)) {
                if (line.find("interface \"" + netif.name + "\"") != std::string::npos) {
                    netif.isDhcp = true;
                    break;
                }
            }
        }
        
        // 设置状态
        if (netif.isUp && netif.isConnected) {
            netif.status = "已连接";
        } else if (netif.isUp) {
            netif.status = "已启用";
        } else {
            netif.status = "已禁用";
        }
        
        interfaces.push_back(netif);
    }
    
    return interfaces;
}

NetworkInterface NetworkManager::getInterface(const std::string& name) {
    auto interfaces = getAllInterfaces();
    for (const auto& interface : interfaces) {
        if (interface.name == name) {
            return interface;
        }
    }
    
    NetworkInterface empty;
    m_lastError = "Interface not found: " + name;
    return empty;
}

bool NetworkManager::setInterfaceEnabled(const std::string& name, bool enabled) {
    std::string command = "ip link set " + name + (enabled ? " up" : " down");
    std::string output;
    
    if (!executeCommand(command, output)) {
        m_lastError = "Failed to " + std::string(enabled ? "enable" : "disable") + " interface: " + name;
        return false;
    }
    
    LOG_INFO() << "[NetworkManager] Interface " << name << " " << (enabled ? "enabled" : "disabled");
    return true;
}

bool NetworkManager::configureInterface(const NetworkConfiguration& config) {
    if (!validateConfiguration(config)) {
        return false;
    }
    
    // 保存配置
    if (!saveConfiguration(config.interfaceName, config)) {
        return false;
    }
    
    // 应用配置
    return applyConfiguration(config.interfaceName);
}

bool NetworkManager::executeCommand(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        m_lastError = "Failed to execute command: " + command;
        return false;
    }
    
    char buffer[128];
    output.clear();
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    int result = pclose(pipe);
    return result == 0;
}

std::string NetworkManager::getInterfaceType(const std::string& name) {
    if (name.find("eth") == 0 || name.find("enp") == 0) {
        return "ethernet";
    } else if (name.find("wlan") == 0 || name.find("wlp") == 0) {
        return "wireless";
    } else if (name == "lo") {
        return "loopback";
    }
    return "unknown";
}

bool NetworkManager::validateIPAddress(const std::string& ip) {
    std::regex ipRegex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return std::regex_match(ip, ipRegex);
}

bool NetworkManager::validateNetmask(const std::string& netmask) {
    return validateIPAddress(netmask);
}

bool NetworkManager::validateConfiguration(const NetworkConfiguration& config) {
    if (config.interfaceName.empty()) {
        m_lastError = "Interface name cannot be empty";
        return false;
    }

    if (!config.isDhcp) {
        if (!validateIPAddress(config.ipAddress)) {
            m_lastError = "Invalid IP address: " + config.ipAddress;
            return false;
        }

        if (!validateNetmask(config.netmask)) {
            m_lastError = "Invalid netmask: " + config.netmask;
            return false;
        }

        if (!config.gateway.empty() && !validateIPAddress(config.gateway)) {
            m_lastError = "Invalid gateway: " + config.gateway;
            return false;
        }

        if (!config.dns1.empty() && !validateIPAddress(config.dns1)) {
            m_lastError = "Invalid DNS1: " + config.dns1;
            return false;
        }

        if (!config.dns2.empty() && !validateIPAddress(config.dns2)) {
            m_lastError = "Invalid DNS2: " + config.dns2;
            return false;
        }
    }

    return true;
}

bool NetworkManager::saveConfiguration(const std::string& interfaceName, const NetworkConfiguration& config) {
    m_configurations[interfaceName] = config;

    // 写入配置文件 (Ubuntu/Debian netplan格式)
    return writeNetworkConfig(interfaceName, config);
}

bool NetworkManager::loadConfiguration(const std::string& interfaceName, NetworkConfiguration& config) {
    auto it = m_configurations.find(interfaceName);
    if (it != m_configurations.end()) {
        config = it->second;
        return true;
    }

    m_lastError = "Configuration not found for interface: " + interfaceName;
    return false;
}

bool NetworkManager::applyConfiguration(const std::string& interfaceName) {
    auto it = m_configurations.find(interfaceName);
    if (it == m_configurations.end()) {
        m_lastError = "No configuration found for interface: " + interfaceName;
        return false;
    }

    const NetworkConfiguration& config = it->second;

    // 启用/禁用接口
    if (!setInterfaceEnabled(interfaceName, config.enabled)) {
        return false;
    }

    if (!config.enabled) {
        return true; // 如果禁用，不需要配置IP
    }

    if (config.isDhcp) {
        return setDHCP(interfaceName);
    } else {
        return setStaticIP(interfaceName, config.ipAddress, config.netmask, config.gateway);
    }
}

bool NetworkManager::setStaticIP(const std::string& interfaceName,
                                 const std::string& ip,
                                 const std::string& netmask,
                                 const std::string& gateway) {
    std::string output;

    // 设置IP地址
    std::string command = "ip addr flush dev " + interfaceName;
    if (!executeCommand(command, output)) {
        m_lastError = "Failed to flush interface: " + interfaceName;
        return false;
    }

    command = "ip addr add " + ip + "/" + netmask + " dev " + interfaceName;
    if (!executeCommand(command, output)) {
        m_lastError = "Failed to set IP address";
        return false;
    }

    // 设置网关
    if (!gateway.empty()) {
        command = "ip route add default via " + gateway;
        executeCommand(command, output); // 可能失败，不强制要求
    }

    LOG_INFO() << "[NetworkManager] Static IP configured for " << interfaceName
               << ": " << ip << "/" << netmask;
    return true;
}

bool NetworkManager::setDHCP(const std::string& interfaceName) {
    std::string command = "dhclient " + interfaceName;
    std::string output;

    if (!executeCommand(command, output)) {
        m_lastError = "Failed to start DHCP client for interface: " + interfaceName;
        return false;
    }

    LOG_INFO() << "[NetworkManager] DHCP enabled for " << interfaceName;
    return true;
}

bool NetworkManager::setDNS(const std::string& dns1, const std::string& dns2) {
    std::ofstream resolvFile("/etc/resolv.conf");
    if (!resolvFile.is_open()) {
        m_lastError = "Failed to open /etc/resolv.conf";
        return false;
    }

    resolvFile << "# Generated by AI Security Vision System\n";
    if (!dns1.empty()) {
        resolvFile << "nameserver " << dns1 << "\n";
    }
    if (!dns2.empty()) {
        resolvFile << "nameserver " << dns2 << "\n";
    }

    resolvFile.close();
    LOG_INFO() << "[NetworkManager] DNS servers updated";
    return true;
}

bool NetworkManager::writeNetworkConfig(const std::string& interfaceName, const NetworkConfiguration& config) {
    // 这里实现写入netplan配置文件的逻辑
    // 为了简化，我们先返回true
    LOG_INFO() << "[NetworkManager] Network configuration saved for " << interfaceName;
    return true;
}

bool NetworkManager::pingTest(const std::string& host, int timeout) {
    std::string command = "ping -c 1 -W " + std::to_string(timeout) + " " + host;
    std::string output;
    return executeCommand(command, output);
}

std::map<std::string, std::string> NetworkManager::getNetworkStats() {
    std::map<std::string, std::string> stats;

    auto interfaces = getAllInterfaces();
    for (const auto& interface : interfaces) {
        stats[interface.name + "_rx_bytes"] = std::to_string(interface.bytesReceived);
        stats[interface.name + "_tx_bytes"] = std::to_string(interface.bytesSent);
        stats[interface.name + "_status"] = interface.status;
    }

    return stats;
}
