#include "ONVIFDiscovery.h"
#include "../core/TaskManager.h"
#include "../core/VideoPipeline.h"
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
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "../core/Logger.h"
using namespace AISecurityVision;
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

std::string ONVIFDiscovery::extractXMLAttribute(const std::string& xml, const std::string& tag, const std::string& attribute) {
    // Find the tag
    std::string openTag = "<" + tag;
    size_t tagPos = xml.find(openTag);
    if (tagPos == std::string::npos) {
        return "";
    }

    // Find the end of the tag
    size_t tagEnd = xml.find(">", tagPos);
    if (tagEnd == std::string::npos) {
        return "";
    }

    // Extract the tag content
    std::string tagContent = xml.substr(tagPos, tagEnd - tagPos + 1);

    // Find the attribute
    std::string attrPattern = attribute + "=\"";
    size_t attrPos = tagContent.find(attrPattern);
    if (attrPos == std::string::npos) {
        return "";
    }

    attrPos += attrPattern.length();
    size_t attrEnd = tagContent.find("\"", attrPos);
    if (attrEnd == std::string::npos) {
        return "";
    }

    return tagContent.substr(attrPos, attrEnd - attrPos);
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
    LOG_INFO() << "[ONVIFDiscovery] " << message;
}

void ONVIFDiscovery::logError(const std::string& message) {
    LOG_ERROR() << "[ONVIFDiscovery] ERROR: " << message;
}

bool ONVIFDiscovery::sendSOAPRequest(const std::string& url, const std::string& soapAction,
                                    const std::string& soapBody, std::string& response,
                                    const std::string& username, const std::string& password) {
    // Create SOAP envelope
    std::string envelope = createSOAPEnvelope(soapBody, username, password);

    // Parse URL to extract host, port, and path
    std::string host, path;
    int port = 80;

    if (!parseURL(url, host, port, path)) {
        logError("Failed to parse URL: " + url);
        return false;
    }

    // Create HTTP request
    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << ":" << port << "\r\n";
    request << "Content-Type: application/soap+xml; charset=utf-8\r\n";
    request << "Content-Length: " << envelope.length() << "\r\n";
    request << "SOAPAction: \"" << soapAction << "\"\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << envelope;

    // Send HTTP request
    return sendHTTPRequest(host, port, request.str(), response);
}

std::string ONVIFDiscovery::createSOAPEnvelope(const std::string& body, const std::string& username, const std::string& password) {
    std::ostringstream envelope;
    envelope << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    envelope << "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\">";

    // Add header with security if credentials provided
    if (!username.empty()) {
        envelope << "<soap:Header>";
        envelope << generateWSSecurity(username, password);
        envelope << "</soap:Header>";
    }

    envelope << "<soap:Body>";
    envelope << body;
    envelope << "</soap:Body>";
    envelope << "</soap:Envelope>";

    return envelope.str();
}

std::string ONVIFDiscovery::generateWSSecurity(const std::string& username, const std::string& password) {
    // Enhanced WS-Security implementation with nonce and timestamp
    std::ostringstream security;

    // Generate nonce (random bytes encoded in base64)
    std::string nonce = generateNonce();

    // Generate timestamp (ISO 8601 format)
    std::string created = generateTimestamp();

    security << "<wsse:Security xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
             << "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">";
    security << "<wsse:UsernameToken wsu:Id=\"UsernameToken-1\">";
    security << "<wsse:Username>" << username << "</wsse:Username>";

    // Use password digest for better security
    std::string passwordDigest = generatePasswordDigest(nonce, created, password);
    security << "<wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">";
    security << passwordDigest << "</wsse:Password>";

    security << "<wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">";
    security << nonce << "</wsse:Nonce>";
    security << "<wsu:Created>" << created << "</wsu:Created>";
    security << "</wsse:UsernameToken>";
    security << "</wsse:Security>";

    return security.str();
}

std::string ONVIFDiscovery::generateNonce() {
    // Generate 16 random bytes and encode as base64
    const size_t nonceSize = 16;
    unsigned char nonce[nonceSize];

    // Use current time and random seed for nonce generation
    srand(time(nullptr) + rand());
    for (size_t i = 0; i < nonceSize; ++i) {
        nonce[i] = rand() % 256;
    }

    return base64Encode(nonce, nonceSize);
}

std::string ONVIFDiscovery::generateTimestamp() {
    // Generate ISO 8601 timestamp (UTC)
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";

    return oss.str();
}

std::string ONVIFDiscovery::generatePasswordDigest(const std::string& nonce, const std::string& created, const std::string& password) {
    // Password digest = Base64(SHA1(nonce + created + password))
    std::string combined = base64Decode(nonce) + created + password;

    // Simple SHA1 implementation (for production, use a proper crypto library)
    unsigned char hash[20];
    sha1Hash(combined, hash);

    return base64Encode(hash, 20);
}

std::string ONVIFDiscovery::base64Encode(const unsigned char* data, size_t length) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;

    for (size_t i = 0; i < length; i += 3) {
        unsigned int val = (data[i] << 16);
        if (i + 1 < length) val |= (data[i + 1] << 8);
        if (i + 2 < length) val |= data[i + 2];

        result += chars[(val >> 18) & 0x3F];
        result += chars[(val >> 12) & 0x3F];
        result += (i + 1 < length) ? chars[(val >> 6) & 0x3F] : '=';
        result += (i + 2 < length) ? chars[val & 0x3F] : '=';
    }

    return result;
}

std::string ONVIFDiscovery::base64Decode(const std::string& encoded) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;

    for (size_t i = 0; i < encoded.length(); i += 4) {
        unsigned int val = 0;
        for (int j = 0; j < 4; ++j) {
            if (i + j < encoded.length() && encoded[i + j] != '=') {
                val = (val << 6) | chars.find(encoded[i + j]);
            } else {
                val <<= 6;
            }
        }

        result += static_cast<char>((val >> 16) & 0xFF);
        if (i + 2 < encoded.length() && encoded[i + 2] != '=') {
            result += static_cast<char>((val >> 8) & 0xFF);
        }
        if (i + 3 < encoded.length() && encoded[i + 3] != '=') {
            result += static_cast<char>(val & 0xFF);
        }
    }

    return result;
}

void ONVIFDiscovery::sha1Hash(const std::string& input, unsigned char* hash) {
    // Simple SHA1 implementation (for production, use OpenSSL or similar)
    // This is a basic implementation for demonstration
    const unsigned char* data = reinterpret_cast<const unsigned char*>(input.c_str());
    size_t length = input.length();

    // Initialize hash values
    unsigned int h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    // Pre-processing: adding padding bits
    std::vector<unsigned char> padded(data, data + length);
    padded.push_back(0x80);

    while ((padded.size() % 64) != 56) {
        padded.push_back(0x00);
    }

    // Append length as 64-bit big-endian
    unsigned long long bitLength = length * 8;
    for (int i = 7; i >= 0; --i) {
        padded.push_back((bitLength >> (i * 8)) & 0xFF);
    }

    // Process message in 512-bit chunks
    for (size_t chunk = 0; chunk < padded.size(); chunk += 64) {
        unsigned int w[80];

        // Break chunk into sixteen 32-bit big-endian words
        for (int i = 0; i < 16; ++i) {
            w[i] = (padded[chunk + i * 4] << 24) |
                   (padded[chunk + i * 4 + 1] << 16) |
                   (padded[chunk + i * 4 + 2] << 8) |
                   (padded[chunk + i * 4 + 3]);
        }

        // Extend the sixteen 32-bit words into eighty 32-bit words
        for (int i = 16; i < 80; ++i) {
            unsigned int temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        // Initialize hash value for this chunk
        unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

        // Main loop
        for (int i = 0; i < 80; ++i) {
            unsigned int f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            unsigned int temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        // Add this chunk's hash to result
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    // Produce the final hash value as a 160-bit number (20 bytes)
    for (int i = 0; i < 5; ++i) {
        hash[i * 4] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

bool ONVIFDiscovery::parseURL(const std::string& url, std::string& host, int& port, std::string& path) {
    // Simple URL parsing for http://host:port/path format
    std::string urlCopy = url;

    // Remove protocol if present
    if (urlCopy.find("http://") == 0) {
        urlCopy = urlCopy.substr(7);
    } else if (urlCopy.find("https://") == 0) {
        urlCopy = urlCopy.substr(8);
        port = 443; // Default HTTPS port
    }

    // Find path separator
    size_t pathPos = urlCopy.find('/');
    std::string hostPort;

    if (pathPos != std::string::npos) {
        hostPort = urlCopy.substr(0, pathPos);
        path = urlCopy.substr(pathPos);
    } else {
        hostPort = urlCopy;
        path = "/";
    }

    // Parse host and port
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = hostPort.substr(0, colonPos);
        try {
            port = std::stoi(hostPort.substr(colonPos + 1));
        } catch (const std::exception&) {
            return false;
        }
    } else {
        host = hostPort;
        // Keep default port (80 or 443)
    }

    return !host.empty();
}

bool ONVIFDiscovery::sendHTTPRequest(const std::string& host, int port, const std::string& request, std::string& response) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Failed to create socket");
        return false;
    }

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 10; // 10 second timeout
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Connect to server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        // Try to resolve hostname
        struct hostent* hostEntry = gethostbyname(host.c_str());
        if (!hostEntry) {
            close(sock);
            logError("Failed to resolve hostname: " + host);
            return false;
        }
        memcpy(&serverAddr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        logError("Failed to connect to " + host + ":" + std::to_string(port));
        return false;
    }

    // Send request
    ssize_t sent = send(sock, request.c_str(), request.length(), 0);
    if (sent < 0) {
        close(sock);
        logError("Failed to send HTTP request");
        return false;
    }

    // Receive response
    response.clear();
    char buffer[4096];
    ssize_t received;

    while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[received] = '\0';
        response += buffer;
    }

    close(sock);

    if (response.empty()) {
        logError("No response received");
        return false;
    }

    return true;
}

bool ONVIFDiscovery::testAuthentication(const ONVIFDevice& device, const std::string& username, const std::string& password) {
    if (device.serviceUrl.empty()) {
        logError("Device service URL is empty");
        return false;
    }

    logDebug("Testing authentication for device: " + device.ipAddress + " with username: " + username);

    // Create a simple GetDeviceInformation SOAP request to test authentication
    std::string soapBody =
        "<tds:GetDeviceInformation xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"/>";

    std::string response;
    bool success = sendSOAPRequest(device.serviceUrl,
                                  "http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation",
                                  soapBody, response, username, password);

    if (!success) {
        logError("SOAP request failed during authentication test");
        return false;
    }

    // Check for authentication-related errors in the response
    if (response.find("401") != std::string::npos ||
        response.find("Unauthorized") != std::string::npos ||
        response.find("Authentication") != std::string::npos ||
        response.find("NotAuthorized") != std::string::npos ||
        response.find("FailedAuthentication") != std::string::npos) {
        logError("Authentication failed for device " + device.ipAddress);
        return false;
    }

    // Check for successful SOAP response
    if (response.find("GetDeviceInformationResponse") != std::string::npos ||
        response.find("tds:GetDeviceInformationResponse") != std::string::npos) {
        logDebug("Authentication successful for device " + device.ipAddress);
        return true;
    }

    // If we get here, the response was unexpected
    logError("Unexpected response during authentication test for device " + device.ipAddress);
    return false;
}

// Enhanced implementations for ONVIF SOAP communication
bool ONVIFDiscovery::getDeviceInformation(ONVIFDevice& device) {
    if (device.serviceUrl.empty()) {
        logError("Device service URL is empty");
        return false;
    }

    // Create SOAP request for GetDeviceInformation
    std::string soapBody = R"(
        <tds:GetDeviceInformation xmlns:tds="http://www.onvif.org/ver10/device/wsdl"/>
    )";

    std::string response;
    if (!sendSOAPRequest(device.serviceUrl, "http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation",
                        soapBody, response, device.username, device.password)) {
        // Fallback to default values if SOAP request fails
        device.name = "ONVIF Camera (" + device.ipAddress + ")";
        device.manufacturer = "Unknown";
        device.model = "Unknown";
        device.firmwareVersion = "Unknown";
        device.serialNumber = "Unknown";
        logDebug("Failed to get device information via SOAP, using defaults");
        return true; // Don't fail completely
    }

    // Parse SOAP response
    device.manufacturer = extractXMLValue(response, "tds:Manufacturer");
    device.model = extractXMLValue(response, "tds:Model");
    device.firmwareVersion = extractXMLValue(response, "tds:FirmwareVersion");
    device.serialNumber = extractXMLValue(response, "tds:SerialNumber");

    // Generate a friendly name
    if (!device.manufacturer.empty() && !device.model.empty()) {
        device.name = device.manufacturer + " " + device.model;
    } else {
        device.name = "ONVIF Camera (" + device.ipAddress + ")";
    }

    logDebug("Retrieved device information: " + device.name);
    return true;
}

bool ONVIFDiscovery::getMediaProfiles(ONVIFDevice& device) {
    if (device.serviceUrl.empty()) {
        logError("Device service URL is empty");
        return false;
    }

    // Construct media service URL (typically /onvif/Media)
    std::string mediaUrl = device.serviceUrl;
    if (mediaUrl.find("/onvif/device_service") != std::string::npos) {
        mediaUrl = std::regex_replace(mediaUrl, std::regex("/onvif/device_service"), "/onvif/Media");
    } else if (mediaUrl.back() != '/') {
        mediaUrl += "/onvif/Media";
    } else {
        mediaUrl += "onvif/Media";
    }

    // Create SOAP request for GetProfiles
    std::string soapBody = R"(
        <trt:GetProfiles xmlns:trt="http://www.onvif.org/ver10/media/wsdl"/>
    )";

    std::string response;
    if (!sendSOAPRequest(mediaUrl, "http://www.onvif.org/ver10/media/wsdl/GetProfiles",
                        soapBody, response, device.username, device.password)) {
        // Fallback to default profile
        device.profileToken = "Profile_1";
        logDebug("Failed to get media profiles via SOAP, using default");
        return true; // Don't fail completely
    }

    // Parse SOAP response to extract profile token
    std::string profileToken = extractXMLValue(response, "trt:Profiles");
    if (profileToken.empty()) {
        // Try alternative parsing
        profileToken = extractXMLAttribute(response, "trt:Profiles", "token");
    }

    if (!profileToken.empty()) {
        device.profileToken = profileToken;
        logDebug("Retrieved media profile token: " + profileToken);
    } else {
        device.profileToken = "Profile_1";
        logDebug("No profile token found, using default");
    }

    return true;
}

bool ONVIFDiscovery::getStreamUri(ONVIFDevice& device) {
    if (device.serviceUrl.empty() || device.profileToken.empty()) {
        logError("Device service URL or profile token is empty");
        // Fallback to standard RTSP URL
        device.streamUri = "rtsp://" + device.ipAddress + ":554/stream1";
        return true;
    }

    // Construct media service URL
    std::string mediaUrl = device.serviceUrl;
    if (mediaUrl.find("/onvif/device_service") != std::string::npos) {
        mediaUrl = std::regex_replace(mediaUrl, std::regex("/onvif/device_service"), "/onvif/Media");
    } else if (mediaUrl.back() != '/') {
        mediaUrl += "/onvif/Media";
    } else {
        mediaUrl += "onvif/Media";
    }

    // Create SOAP request for GetStreamUri
    std::string soapBody = R"(
        <trt:GetStreamUri xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
            <trt:StreamSetup>
                <tt:Stream xmlns:tt="http://www.onvif.org/ver10/schema">RTP-Unicast</tt:Stream>
                <tt:Transport xmlns:tt="http://www.onvif.org/ver10/schema">
                    <tt:Protocol>RTSP</tt:Protocol>
                </tt:Transport>
            </trt:StreamSetup>
            <trt:ProfileToken>)" + device.profileToken + R"(</trt:ProfileToken>
        </trt:GetStreamUri>
    )";

    std::string response;
    if (!sendSOAPRequest(mediaUrl, "http://www.onvif.org/ver10/media/wsdl/GetStreamUri",
                        soapBody, response, device.username, device.password)) {
        // Fallback to standard RTSP URL
        device.streamUri = "rtsp://" + device.ipAddress + ":554/stream1";
        logDebug("Failed to get stream URI via SOAP, using fallback");
        return true; // Don't fail completely
    }

    // Parse SOAP response to extract stream URI
    std::string streamUri = extractXMLValue(response, "tt:Uri");
    if (streamUri.empty()) {
        // Try alternative parsing
        streamUri = extractXMLValue(response, "trt:Uri");
    }

    if (!streamUri.empty()) {
        device.streamUri = streamUri;
        logDebug("Retrieved stream URI: " + streamUri);
    } else {
        // Fallback to standard RTSP URL
        device.streamUri = "rtsp://" + device.ipAddress + ":554/stream1";
        logDebug("No stream URI found, using fallback");
    }

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

    // Add to known devices
    m_knownDevices.push_back(device);

    // Auto-configure device if enabled
    if (m_autoAddDevices) {
        ONVIFDevice deviceCopy = device; // Make a copy for configuration
        if (configureDevice(deviceCopy)) {
            LOG_INFO() << "[ONVIFManager] Successfully auto-configured device: " << deviceCopy.name;
        } else {
            LOG_ERROR() << "[ONVIFManager] Failed to auto-configure device: " << deviceCopy.name;
        }
    }

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

bool ONVIFManager::configureDevice(ONVIFDevice& device) {
    // Validate device has required information
    if (device.ipAddress.empty() || device.streamUri.empty()) {
        m_lastError = "Device missing required information (IP or stream URI)";
        return false;
    }

    try {
        // Create VideoSource from ONVIF device
        VideoSource videoSource;
        videoSource.id = generateDeviceId(device);
        videoSource.url = device.streamUri;
        videoSource.protocol = "rtsp";
        videoSource.username = device.username;
        videoSource.password = device.password;
        videoSource.enabled = true;

        // Set reasonable defaults for video parameters
        videoSource.width = 1920;   // Default to Full HD
        videoSource.height = 1080;
        videoSource.fps = 25;       // Default frame rate

        // Validate VideoSource
        if (!videoSource.isValid()) {
            m_lastError = "Generated VideoSource is invalid";
            return false;
        }

        // Add to TaskManager
        TaskManager& taskManager = TaskManager::getInstance();
        if (!taskManager.addVideoSource(videoSource)) {
            m_lastError = "Failed to add video source to TaskManager";
            return false;
        }

        LOG_INFO() << "[ONVIFManager] Auto-configured ONVIF device: " << device.name
                  << " (" << device.ipAddress << ") -> " << videoSource.id;

        return true;

    } catch (const std::exception& e) {
        m_lastError = "Exception during device configuration: " + std::string(e.what());
        return false;
    }
}
