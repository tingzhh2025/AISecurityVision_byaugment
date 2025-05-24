#include "ONVIFDiscovery.h"
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>
#include <regex>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

// ONVIFDevice implementation
bool ONVIFDevice::isValid() const {
    return !uuid.empty() && !ipAddress.empty() && !serviceUrl.empty();
}

std::string ONVIFDevice::toString() const {
    std::ostringstream oss;
    oss << "ONVIFDevice{uuid=" << uuid
        << ", name=" << name
        << ", manufacturer=" << manufacturer
        << ", model=" << model
        << ", ip=" << ipAddress
        << ", port=" << port
        << ", serviceUrl=" << serviceUrl
        << ", streamUri=" << streamUri
        << ", requiresAuth=" << (requiresAuth ? "true" : "false")
        << "}";
    return oss.str();
}

// ONVIFDiscovery implementation
ONVIFDiscovery::ONVIFDiscovery()
    : m_initialized(false)
    , m_discoverySocket(-1)
    , m_timeoutMs(DEFAULT_TIMEOUT_MS)
    , m_maxDevices(DEFAULT_MAX_DEVICES)
    , m_lastDiscovery(std::chrono::steady_clock::time_point::min())
{
    logDebug("ONVIFDiscovery created");
}

ONVIFDiscovery::~ONVIFDiscovery() {
    cleanup();
}

bool ONVIFDiscovery::initialize() {
    std::lock_guard<std::mutex> lock(m_errorMutex);

    if (m_initialized) {
        return true;
    }

    logDebug("Initializing ONVIF Discovery...");

    // Initialize network
    if (!initializeNetwork()) {
        setLastError("Failed to initialize network for ONVIF discovery");
        return false;
    }

    m_initialized = true;
    logDebug("ONVIF Discovery initialized successfully");
    return true;
}

void ONVIFDiscovery::cleanup() {
    if (m_discoverySocket >= 0) {
        close(m_discoverySocket);
        m_discoverySocket = -1;
    }

    m_initialized = false;
    logDebug("ONVIF Discovery cleaned up");
}

std::vector<ONVIFDevice> ONVIFDiscovery::discoverDevices(int timeoutMs) {
    std::vector<ONVIFDevice> devices;

    if (!m_initialized) {
        setLastError("ONVIF Discovery not initialized");
        return devices;
    }

    logDebug("Starting ONVIF device discovery...");

    // Send WS-Discovery probe
    if (!sendProbeMessage()) {
        setLastError("Failed to send WS-Discovery probe message");
        return devices;
    }

    // Receive probe matches
    if (!receiveProbeMatches(devices, timeoutMs)) {
        setLastError("Failed to receive probe match responses");
        return devices;
    }

    // Get detailed information for each device
    for (auto& device : devices) {
        getDeviceInformation(device);
        getMediaProfiles(device);
        getStreamUri(device);
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_cachedDevices = devices;
        m_lastDiscovery = std::chrono::steady_clock::now();
    }

    logDebug("ONVIF device discovery completed. Found " + std::to_string(devices.size()) + " devices");
    return devices;
}

bool ONVIFDiscovery::initializeNetwork() {
    // Create UDP socket for multicast
    m_discoverySocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_discoverySocket < 0) {
        logError("Failed to create discovery socket");
        return false;
    }

    // Set socket options for multicast
    int reuse = 1;
    if (setsockopt(m_discoverySocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        logError("Failed to set SO_REUSEADDR");
        close(m_discoverySocket);
        m_discoverySocket = -1;
        return false;
    }

    // Set multicast TTL
    int ttl = 1;
    if (setsockopt(m_discoverySocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        logError("Failed to set multicast TTL");
        close(m_discoverySocket);
        m_discoverySocket = -1;
        return false;
    }

    // Bind to local address
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = 0; // Let system choose port

    if (bind(m_discoverySocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        logError("Failed to bind discovery socket");
        close(m_discoverySocket);
        m_discoverySocket = -1;
        return false;
    }

    return true;
}

bool ONVIFDiscovery::sendProbeMessage() {
    // Generate UUID for message ID
    std::string messageId = generateUUID();

    // Format probe message
    char probeMessage[2048];
    snprintf(probeMessage, sizeof(probeMessage), WS_DISCOVERY_PROBE_MESSAGE, messageId.c_str());

    // Set up multicast address
    struct sockaddr_in multicastAddr;
    memset(&multicastAddr, 0, sizeof(multicastAddr));
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(MULTICAST_PORT);
    inet_pton(AF_INET, MULTICAST_ADDRESS, &multicastAddr.sin_addr);

    // Send probe message
    ssize_t sent = sendto(m_discoverySocket, probeMessage, strlen(probeMessage), 0,
                         (struct sockaddr*)&multicastAddr, sizeof(multicastAddr));

    if (sent < 0) {
        logError("Failed to send WS-Discovery probe message");
        return false;
    }

    logDebug("WS-Discovery probe message sent (" + std::to_string(sent) + " bytes)");
    return true;
}

bool ONVIFDiscovery::receiveProbeMatches(std::vector<ONVIFDevice>& devices, int timeoutMs) {
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    if (setsockopt(m_discoverySocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        logError("Failed to set socket timeout");
        return false;
    }

    char buffer[8192];
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < endTime && devices.size() < m_maxDevices) {
        ssize_t received = recvfrom(m_discoverySocket, buffer, sizeof(buffer) - 1, 0,
                                   (struct sockaddr*)&senderAddr, &senderAddrLen);

        if (received > 0) {
            buffer[received] = '\0';
            std::string response(buffer);

            // Parse probe match response
            ONVIFDevice device;
            if (parseProbeMatch(response, device)) {
                device.ipAddress = inet_ntoa(senderAddr.sin_addr);
                device.discoveredAt = std::chrono::steady_clock::now();
                devices.push_back(device);

                logDebug("Discovered ONVIF device: " + device.toString());
            }
        } else if (received < 0) {
            // Check if it's a timeout (expected) or real error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Timeout reached
            } else {
                logError("Error receiving probe match response");
                return false;
            }
        }
    }

    return true;
}

bool ONVIFDiscovery::parseProbeMatch(const std::string& response, ONVIFDevice& device) {
    // Simple XML parsing for probe match response
    // In a production system, you would use a proper XML parser

    // Extract UUID
    device.uuid = extractXMLValue(response, "wsa:EndpointReference/wsa:Address");
    if (device.uuid.empty()) {
        return false;
    }

    // Extract service URLs
    std::vector<std::string> xaddrs = extractXMLValues(response, "wsd:XAddrs");
    if (!xaddrs.empty()) {
        device.serviceUrl = xaddrs[0]; // Use first service URL

        // Extract port from URL if present
        std::regex portRegex(R"(:(\d+)/)");
        std::smatch match;
        if (std::regex_search(device.serviceUrl, match, portRegex)) {
            device.port = std::stoi(match[1].str());
        }
    }

    // Extract device types
    std::string types = extractXMLValue(response, "wsd:Types");
    if (types.find("NetworkVideoTransmitter") == std::string::npos) {
        return false; // Not an ONVIF camera
    }

    return device.isValid();
}

std::string ONVIFDiscovery::extractXMLValue(const std::string& xml, const std::string& tag) {
    // Simple XML value extraction
    // This is a basic implementation - production code should use proper XML parsing

    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t startPos = xml.find(openTag);
    if (startPos == std::string::npos) {
        return "";
    }

    startPos += openTag.length();
    size_t endPos = xml.find(closeTag, startPos);
    if (endPos == std::string::npos) {
        return "";
    }

    return xml.substr(startPos, endPos - startPos);
}

std::vector<std::string> ONVIFDiscovery::extractXMLValues(const std::string& xml, const std::string& tag) {
    std::vector<std::string> values;
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";

    size_t pos = 0;
    while ((pos = xml.find(openTag, pos)) != std::string::npos) {
        pos += openTag.length();
        size_t endPos = xml.find(closeTag, pos);
        if (endPos != std::string::npos) {
            values.push_back(xml.substr(pos, endPos - pos));
            pos = endPos + closeTag.length();
        } else {
            break;
        }
    }

    return values;
}

std::string ONVIFDiscovery::generateUUID() {
    // Simple UUID generation for message IDs
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            oss << "-";
        }
        oss << dis(gen);
    }

    return oss.str();
}

void ONVIFDiscovery::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = error;
}

std::string ONVIFDiscovery::getLastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void ONVIFDiscovery::logDebug(const std::string& message) {
    std::cout << "[ONVIFDiscovery] " << message << std::endl;
}

void ONVIFDiscovery::logError(const std::string& message) {
    std::cerr << "[ONVIFDiscovery] ERROR: " << message << std::endl;
}

// Stub implementations for methods that require full SOAP/ONVIF implementation
bool ONVIFDiscovery::getDeviceInformation(ONVIFDevice& device) {
    // TODO: Implement SOAP GetDeviceInformation request
    device.name = "ONVIF Camera";
    device.manufacturer = "Unknown";
    device.model = "Unknown";
    device.firmwareVersion = "Unknown";
    device.serialNumber = "Unknown";
    return true;
}

bool ONVIFDiscovery::getMediaProfiles(ONVIFDevice& device) {
    // TODO: Implement SOAP GetProfiles request
    device.profileToken = "Profile_1";
    return true;
}

bool ONVIFDiscovery::getStreamUri(ONVIFDevice& device) {
    // TODO: Implement SOAP GetStreamUri request
    // For now, construct a typical RTSP URL
    device.streamUri = "rtsp://" + device.ipAddress + ":554/stream1";
    return true;
}

bool ONVIFDiscovery::isInitialized() const {
    return m_initialized;
}

size_t ONVIFDiscovery::getDiscoveredDeviceCount() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    return m_cachedDevices.size();
}

std::vector<ONVIFDevice> ONVIFDiscovery::getCachedDevices() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    return m_cachedDevices;
}

void ONVIFDiscovery::clearCache() {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_cachedDevices.clear();
}

void ONVIFDiscovery::setDiscoveryTimeout(int timeoutMs) {
    m_timeoutMs = timeoutMs;
}

void ONVIFDiscovery::setMaxDevices(size_t maxDevices) {
    m_maxDevices = maxDevices;
}

void ONVIFDiscovery::setNetworkInterface(const std::string& interfaceName) {
    m_networkInterface = interfaceName;
}

void ONVIFDiscovery::setDefaultCredentials(const std::string& username, const std::string& password) {
    m_defaultUsername = username;
    m_defaultPassword = password;
}

// ONVIFManager implementation
ONVIFManager::ONVIFManager()
    : m_discovery(std::make_unique<ONVIFDiscovery>())
    , m_autoAddDevices(false)
    , m_initialized(false)
{
}

ONVIFManager::~ONVIFManager() {
    shutdown();
}

bool ONVIFManager::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!m_discovery->initialize()) {
        m_lastError = "Failed to initialize ONVIF discovery: " + m_discovery->getLastError();
        return false;
    }

    m_initialized = true;
    return true;
}

void ONVIFManager::shutdown() {
    if (m_discovery) {
        m_discovery->cleanup();
    }
    m_initialized = false;
}

std::vector<ONVIFDevice> ONVIFManager::scanNetwork(int timeoutMs) {
    if (!m_initialized) {
        m_lastError = "ONVIFManager not initialized";
        return {};
    }

    auto devices = m_discovery->discoverDevices(timeoutMs);

    // Auto-add devices if enabled
    if (m_autoAddDevices) {
        for (const auto& device : devices) {
            addDiscoveredDevice(device);
        }
    }

    return devices;
}

bool ONVIFManager::addDiscoveredDevice(const ONVIFDevice& device) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    // Check if device already exists
    std::string deviceId = generateDeviceId(device);
    for (const auto& existingDevice : m_knownDevices) {
        if (generateDeviceId(existingDevice) == deviceId) {
            return false; // Device already exists
        }
    }

    m_knownDevices.push_back(device);
    return true;
}

bool ONVIFManager::removeDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    auto it = std::remove_if(m_knownDevices.begin(), m_knownDevices.end(),
        [&deviceId, this](const ONVIFDevice& device) {
            return generateDeviceId(device) == deviceId;
        });

    if (it != m_knownDevices.end()) {
        m_knownDevices.erase(it, m_knownDevices.end());
        return true;
    }

    return false;
}

std::vector<ONVIFDevice> ONVIFManager::getKnownDevices() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    return m_knownDevices;
}

ONVIFDevice* ONVIFManager::findDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_devicesMutex);

    for (auto& device : m_knownDevices) {
        if (generateDeviceId(device) == deviceId) {
            return &device;
        }
    }

    return nullptr;
}

bool ONVIFManager::updateDeviceCredentials(const std::string& deviceId, const std::string& username, const std::string& password) {
    ONVIFDevice* device = findDevice(deviceId);
    if (!device) {
        m_lastError = "Device not found: " + deviceId;
        return false;
    }

    device->username = username;
    device->password = password;
    device->requiresAuth = !username.empty();

    return true;
}

void ONVIFManager::setAutoAddDevices(bool enabled) {
    m_autoAddDevices = enabled;
}

bool ONVIFManager::isAutoAddEnabled() const {
    return m_autoAddDevices;
}

bool ONVIFManager::isInitialized() const {
    return m_initialized;
}

std::string ONVIFManager::getLastError() const {
    return m_lastError;
}

std::string ONVIFManager::generateDeviceId(const ONVIFDevice& device) {
    // Generate a unique device ID based on UUID or IP address
    if (!device.uuid.empty()) {
        return device.uuid;
    } else {
        return "onvif_" + device.ipAddress + "_" + std::to_string(device.port);
    }
}
