#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace AISecurityVision {

struct NetworkInterface {
    std::string name;           // 网卡名称 (eth0, wlan0, etc.)
    std::string displayName;    // 显示名称
    bool isUp;                  // 是否启用
    bool isConnected;           // 是否连接
    std::string type;           // 类型 (ethernet, wireless, loopback)
    std::string macAddress;     // MAC地址
    
    // IP配置
    bool isDhcp;                // 是否使用DHCP
    std::string ipAddress;      // IP地址
    std::string netmask;        // 子网掩码
    std::string gateway;        // 网关
    std::string dns1;           // 主DNS
    std::string dns2;           // 备用DNS
    
    // 状态信息
    std::string status;         // 状态描述
    uint64_t bytesReceived;     // 接收字节数
    uint64_t bytesSent;         // 发送字节数
    int linkSpeed;              // 链路速度 (Mbps)
    
    // 无线网络特有
    std::string ssid;           // WiFi SSID
    int signalStrength;         // 信号强度 (%)
    std::string security;       // 安全类型
};

struct NetworkConfiguration {
    std::string interfaceName;
    bool enabled;
    bool isDhcp;
    std::string ipAddress;
    std::string netmask;
    std::string gateway;
    std::string dns1;
    std::string dns2;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // 初始化
    bool initialize();
    void cleanup();

    // 网络接口管理
    std::vector<NetworkInterface> getAllInterfaces();
    NetworkInterface getInterface(const std::string& name);
    bool setInterfaceEnabled(const std::string& name, bool enabled);
    
    // IP配置管理
    bool configureInterface(const NetworkConfiguration& config);
    bool setStaticIP(const std::string& interfaceName, 
                     const std::string& ip, 
                     const std::string& netmask, 
                     const std::string& gateway);
    bool setDHCP(const std::string& interfaceName);
    bool setDNS(const std::string& dns1, const std::string& dns2 = "");
    
    // 网络状态
    bool isInterfaceUp(const std::string& name);
    bool pingTest(const std::string& host, int timeout = 5);
    std::map<std::string, std::string> getNetworkStats();
    
    // 配置持久化
    bool saveConfiguration(const std::string& interfaceName, const NetworkConfiguration& config);
    bool loadConfiguration(const std::string& interfaceName, NetworkConfiguration& config);
    bool applyConfiguration(const std::string& interfaceName);
    
    // 验证
    bool validateIPAddress(const std::string& ip);
    bool validateNetmask(const std::string& netmask);
    bool validateConfiguration(const NetworkConfiguration& config);
    
    // 错误处理
    std::string getLastError() const { return m_lastError; }

private:
    // 内部方法
    bool executeCommand(const std::string& command, std::string& output);
    bool parseInterfaceInfo(const std::string& data, NetworkInterface& interface);
    std::string getInterfaceType(const std::string& name);
    bool writeNetworkConfig(const std::string& interfaceName, const NetworkConfiguration& config);
    bool restartNetworking();
    
    // 成员变量
    std::string m_lastError;
    std::map<std::string, NetworkConfiguration> m_configurations;
    bool m_initialized;
};

} // namespace AISecurityVision
