#include "simple_mqtt.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

SimpleMQTTClient::SimpleMQTTClient(const std::string& broker, int port)
    : m_broker(broker), m_port(port), m_socket(-1) {
    std::cout << "[SimpleMQTTClient] Created MQTT client for " << broker << ":" << port << std::endl;
}

SimpleMQTTClient::~SimpleMQTTClient() {
    disconnect();
}

bool SimpleMQTTClient::connect(const std::string& clientId, 
                              const std::string& username, 
                              const std::string& password) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_connected.load()) {
        return true;
    }
    
    m_clientId = clientId.empty() ? generateClientId() : clientId;
    m_username = username;
    m_password = password;
    
    std::cout << "[SimpleMQTTClient] Connecting to " << m_broker << ":" << m_port 
              << " as " << m_clientId << std::endl;
    
    if (!connectSocket()) {
        return false;
    }
    
    if (!sendConnectPacket()) {
        closeSocket();
        return false;
    }
    
    if (!receiveConnAck()) {
        closeSocket();
        return false;
    }
    
    m_connected.store(true);
    m_running.store(true);
    
    // Start keep-alive thread
    m_keepAliveThread = std::thread(&SimpleMQTTClient::keepAliveLoop, this);
    
    // Start reconnect thread if auto-reconnect is enabled
    if (m_autoReconnect) {
        m_reconnectThread = std::thread(&SimpleMQTTClient::reconnectLoop, this);
    }
    
    std::cout << "[SimpleMQTTClient] Connected successfully" << std::endl;
    return true;
}

void SimpleMQTTClient::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected.load()) {
        return;
    }
    
    std::cout << "[SimpleMQTTClient] Disconnecting..." << std::endl;
    
    m_running.store(false);
    m_connected.store(false);
    
    closeSocket();
    
    // Wait for threads to finish
    if (m_keepAliveThread.joinable()) {
        m_keepAliveThread.join();
    }
    
    if (m_reconnectThread.joinable()) {
        m_reconnectThread.join();
    }
    
    std::cout << "[SimpleMQTTClient] Disconnected" << std::endl;
}

bool SimpleMQTTClient::publish(const std::string& topic, 
                              const std::string& payload, 
                              int qos, 
                              bool retain) {
    if (!m_connected.load()) {
        setError("Not connected to broker");
        return false;
    }
    
    if (qos < 0 || qos > 2) {
        setError("Invalid QoS level");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    bool result = sendPublishPacket(topic, payload, qos, retain);
    
    if (result) {
        std::cout << "[SimpleMQTTClient] Published to " << topic 
                  << " (QoS " << qos << ", " << payload.length() << " bytes)" << std::endl;
    } else {
        std::cerr << "[SimpleMQTTClient] Failed to publish to " << topic << std::endl;
    }
    
    return result;
}

bool SimpleMQTTClient::isConnected() const {
    return m_connected.load();
}

std::string SimpleMQTTClient::getLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void SimpleMQTTClient::setConnectionTimeout(int timeoutMs) {
    m_connectionTimeout = timeoutMs;
}

void SimpleMQTTClient::setKeepAlive(int keepAliveSeconds) {
    m_keepAlive = keepAliveSeconds;
}

void SimpleMQTTClient::setAutoReconnect(bool enable, int intervalMs) {
    m_autoReconnect = enable;
    m_reconnectInterval = intervalMs;
}

bool SimpleMQTTClient::connectSocket() {
    // Resolve hostname
    struct hostent* host = gethostbyname(m_broker.c_str());
    if (!host) {
        setError("Failed to resolve hostname: " + m_broker);
        return false;
    }
    
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        setError("Failed to create socket");
        return false;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = m_connectionTimeout / 1000;
    timeout.tv_usec = (m_connectionTimeout % 1000) * 1000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect to broker
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    memcpy(&serverAddr.sin_addr, host->h_addr_list[0], host->h_length);
    
    if (::connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        setError("Failed to connect to broker: " + std::string(strerror(errno)));
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    return true;
}

void SimpleMQTTClient::closeSocket() {
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
}

bool SimpleMQTTClient::sendConnectPacket() {
    std::vector<uint8_t> packet;
    
    // Fixed header
    packet.push_back(0x10);  // CONNECT packet type
    
    // Variable header
    std::vector<uint8_t> variableHeader;
    
    // Protocol name "MQTT"
    auto protocolName = encodeString("MQTT");
    variableHeader.insert(variableHeader.end(), protocolName.begin(), protocolName.end());
    
    // Protocol level (4 for MQTT 3.1.1)
    variableHeader.push_back(0x04);
    
    // Connect flags
    uint8_t connectFlags = 0x02;  // Clean session
    if (!m_username.empty()) {
        connectFlags |= 0x80;  // Username flag
        if (!m_password.empty()) {
            connectFlags |= 0x40;  // Password flag
        }
    }
    variableHeader.push_back(connectFlags);
    
    // Keep alive (MSB, LSB)
    variableHeader.push_back((m_keepAlive >> 8) & 0xFF);
    variableHeader.push_back(m_keepAlive & 0xFF);
    
    // Payload
    std::vector<uint8_t> payload;
    
    // Client ID
    auto clientIdBytes = encodeString(m_clientId);
    payload.insert(payload.end(), clientIdBytes.begin(), clientIdBytes.end());
    
    // Username and password
    if (!m_username.empty()) {
        auto usernameBytes = encodeString(m_username);
        payload.insert(payload.end(), usernameBytes.begin(), usernameBytes.end());
        
        if (!m_password.empty()) {
            auto passwordBytes = encodeString(m_password);
            payload.insert(payload.end(), passwordBytes.begin(), passwordBytes.end());
        }
    }
    
    // Remaining length
    uint32_t remainingLength = variableHeader.size() + payload.size();
    auto remainingLengthBytes = encodeRemainingLength(remainingLength);
    packet.insert(packet.end(), remainingLengthBytes.begin(), remainingLengthBytes.end());
    
    // Add variable header and payload
    packet.insert(packet.end(), variableHeader.begin(), variableHeader.end());
    packet.insert(packet.end(), payload.begin(), payload.end());
    
    return sendData(packet);
}

bool SimpleMQTTClient::sendPublishPacket(const std::string& topic, 
                                        const std::string& payload, 
                                        int qos, 
                                        bool retain) {
    std::vector<uint8_t> packet;
    
    // Fixed header
    uint8_t fixedHeader = 0x30;  // PUBLISH packet type
    if (retain) fixedHeader |= 0x01;
    fixedHeader |= (qos & 0x03) << 1;
    packet.push_back(fixedHeader);
    
    // Variable header
    std::vector<uint8_t> variableHeader;
    
    // Topic name
    auto topicBytes = encodeString(topic);
    variableHeader.insert(variableHeader.end(), topicBytes.begin(), topicBytes.end());
    
    // Packet identifier (for QoS > 0)
    if (qos > 0) {
        uint16_t packetId = generatePacketId();
        variableHeader.push_back((packetId >> 8) & 0xFF);
        variableHeader.push_back(packetId & 0xFF);
    }
    
    // Payload
    std::vector<uint8_t> payloadBytes(payload.begin(), payload.end());
    
    // Remaining length
    uint32_t remainingLength = variableHeader.size() + payloadBytes.size();
    auto remainingLengthBytes = encodeRemainingLength(remainingLength);
    packet.insert(packet.end(), remainingLengthBytes.begin(), remainingLengthBytes.end());
    
    // Add variable header and payload
    packet.insert(packet.end(), variableHeader.begin(), variableHeader.end());
    packet.insert(packet.end(), payloadBytes.begin(), payloadBytes.end());
    
    return sendData(packet);
}

bool SimpleMQTTClient::sendPingRequest() {
    std::vector<uint8_t> packet = {0xC0, 0x00};  // PINGREQ
    return sendData(packet);
}

bool SimpleMQTTClient::receiveConnAck() {
    auto data = receiveData(4);  // CONNACK is 4 bytes
    if (data.size() != 4) {
        setError("Failed to receive CONNACK");
        return false;
    }
    
    if (data[0] != 0x20 || data[1] != 0x02) {
        setError("Invalid CONNACK packet");
        return false;
    }
    
    uint8_t returnCode = data[3];
    if (returnCode != 0) {
        setError("Connection refused, return code: " + std::to_string(returnCode));
        return false;
    }
    
    return true;
}

void SimpleMQTTClient::keepAliveLoop() {
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(m_keepAlive / 2));
        
        if (!m_running.load()) break;
        
        if (m_connected.load()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!sendPingRequest()) {
                std::cerr << "[SimpleMQTTClient] Failed to send ping, connection lost" << std::endl;
                m_connected.store(false);
                m_shouldReconnect.store(true);
            }
        }
    }
}

void SimpleMQTTClient::reconnectLoop() {
    while (m_running.load()) {
        if (m_shouldReconnect.load() && !m_connected.load()) {
            std::cout << "[SimpleMQTTClient] Attempting to reconnect..." << std::endl;
            
            if (connectSocket() && sendConnectPacket() && receiveConnAck()) {
                m_connected.store(true);
                m_shouldReconnect.store(false);
                std::cout << "[SimpleMQTTClient] Reconnected successfully" << std::endl;
            } else {
                closeSocket();
                std::cerr << "[SimpleMQTTClient] Reconnection failed" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(m_reconnectInterval));
    }
}

void SimpleMQTTClient::setError(const std::string& error) const {
    m_lastError = error;
    std::cerr << "[SimpleMQTTClient] Error: " << error << std::endl;
}

std::vector<uint8_t> SimpleMQTTClient::encodeString(const std::string& str) {
    std::vector<uint8_t> result;
    uint16_t length = str.length();
    result.push_back((length >> 8) & 0xFF);
    result.push_back(length & 0xFF);
    result.insert(result.end(), str.begin(), str.end());
    return result;
}

std::vector<uint8_t> SimpleMQTTClient::encodeRemainingLength(uint32_t length) {
    std::vector<uint8_t> result;
    do {
        uint8_t byte = length % 128;
        length /= 128;
        if (length > 0) {
            byte |= 0x80;
        }
        result.push_back(byte);
    } while (length > 0);
    return result;
}

uint16_t SimpleMQTTClient::generatePacketId() {
    return m_packetIdCounter.fetch_add(1);
}

std::string SimpleMQTTClient::generateClientId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return "aibox_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

bool SimpleMQTTClient::sendData(const std::vector<uint8_t>& data) {
    if (m_socket < 0) {
        setError("Socket not connected");
        return false;
    }
    
    ssize_t sent = send(m_socket, data.data(), data.size(), 0);
    if (sent != static_cast<ssize_t>(data.size())) {
        setError("Failed to send data: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

std::vector<uint8_t> SimpleMQTTClient::receiveData(size_t expectedSize) {
    std::vector<uint8_t> result;
    
    if (m_socket < 0) {
        setError("Socket not connected");
        return result;
    }
    
    result.resize(expectedSize);
    ssize_t received = recv(m_socket, result.data(), expectedSize, 0);
    
    if (received <= 0) {
        setError("Failed to receive data: " + std::string(strerror(errno)));
        result.clear();
        return result;
    }
    
    result.resize(received);
    return result;
}
