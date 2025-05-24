#pragma once

#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * @brief Simple MQTT client interface for basic publishing
 * 
 * This is a lightweight MQTT client implementation that provides
 * basic publish functionality for alarm delivery. It supports:
 * - TCP connection to MQTT broker
 * - QoS 0, 1, 2 message publishing
 * - Automatic reconnection
 * - Thread-safe operations
 * 
 * Note: This is a simplified implementation. For production use,
 * consider using Eclipse Paho MQTT C++ client library.
 */
class SimpleMQTTClient {
public:
    /**
     * @brief Construct MQTT client
     * @param broker Broker hostname or IP address
     * @param port Broker port (default: 1883)
     */
    SimpleMQTTClient(const std::string& broker, int port = 1883);
    
    /**
     * @brief Destructor - automatically disconnects
     */
    ~SimpleMQTTClient();

    /**
     * @brief Connect to MQTT broker
     * @param clientId Client identifier (auto-generated if empty)
     * @param username Username for authentication (optional)
     * @param password Password for authentication (optional)
     * @return true if connection successful
     */
    bool connect(const std::string& clientId = "", 
                const std::string& username = "", 
                const std::string& password = "");

    /**
     * @brief Disconnect from MQTT broker
     */
    void disconnect();

    /**
     * @brief Publish message to topic
     * @param topic Topic name
     * @param payload Message payload
     * @param qos Quality of Service (0, 1, or 2)
     * @param retain Retain flag
     * @return true if publish successful
     */
    bool publish(const std::string& topic, 
                const std::string& payload, 
                int qos = 0, 
                bool retain = false);

    /**
     * @brief Check if client is connected
     * @return true if connected to broker
     */
    bool isConnected() const;

    /**
     * @brief Get last error message
     * @return Error description
     */
    std::string getLastError() const;

    /**
     * @brief Set connection timeout
     * @param timeoutMs Timeout in milliseconds
     */
    void setConnectionTimeout(int timeoutMs);

    /**
     * @brief Set keep alive interval
     * @param keepAliveSeconds Keep alive interval in seconds
     */
    void setKeepAlive(int keepAliveSeconds);

    /**
     * @brief Enable/disable automatic reconnection
     * @param enable Enable auto-reconnect
     * @param intervalMs Reconnect interval in milliseconds
     */
    void setAutoReconnect(bool enable, int intervalMs = 5000);

private:
    // Connection parameters
    std::string m_broker;
    int m_port;
    std::string m_clientId;
    std::string m_username;
    std::string m_password;
    
    // Connection state
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_shouldReconnect{false};
    int m_socket{-1};
    
    // Configuration
    int m_connectionTimeout{10000};  // 10 seconds
    int m_keepAlive{60};             // 60 seconds
    bool m_autoReconnect{true};
    int m_reconnectInterval{5000};   // 5 seconds
    
    // Threading
    std::thread m_keepAliveThread;
    std::thread m_reconnectThread;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    
    // Error handling
    mutable std::string m_lastError;
    
    // Internal methods
    bool connectSocket();
    void closeSocket();
    bool sendConnectPacket();
    bool sendPublishPacket(const std::string& topic, 
                          const std::string& payload, 
                          int qos, 
                          bool retain);
    bool sendPingRequest();
    bool receiveConnAck();
    void keepAliveLoop();
    void reconnectLoop();
    void setError(const std::string& error) const;
    
    // MQTT packet helpers
    std::vector<uint8_t> encodeString(const std::string& str);
    std::vector<uint8_t> encodeRemainingLength(uint32_t length);
    uint16_t generatePacketId();
    
    // Utility methods
    std::string generateClientId();
    bool sendData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> receiveData(size_t expectedSize);
    
    // Packet ID counter
    std::atomic<uint16_t> m_packetIdCounter{1};
};

/**
 * @brief MQTT Quality of Service levels
 */
enum class MQTTQoS {
    AT_MOST_ONCE = 0,   // Fire and forget
    AT_LEAST_ONCE = 1,  // Acknowledged delivery
    EXACTLY_ONCE = 2    // Assured delivery
};

/**
 * @brief MQTT connection result codes
 */
enum class MQTTConnectResult {
    ACCEPTED = 0,
    UNACCEPTABLE_PROTOCOL_VERSION = 1,
    IDENTIFIER_REJECTED = 2,
    SERVER_UNAVAILABLE = 3,
    BAD_USERNAME_OR_PASSWORD = 4,
    NOT_AUTHORIZED = 5
};
