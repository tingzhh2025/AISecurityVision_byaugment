#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>

/**
 * @brief ONVIF device information structure
 */
struct ONVIFDevice {
    std::string uuid;           // Device UUID
    std::string name;           // Device name/model
    std::string manufacturer;   // Device manufacturer
    std::string model;          // Device model
    std::string firmwareVersion; // Firmware version
    std::string serialNumber;   // Serial number
    std::string ipAddress;      // Device IP address
    int port = 80;             // ONVIF service port
    std::string serviceUrl;     // ONVIF service URL
    std::string profileToken;   // Media profile token
    std::string streamUri;      // RTSP stream URI
    bool requiresAuth = false;  // Whether device requires authentication
    std::string username;       // Authentication username (if required)
    std::string password;       // Authentication password (if required)
    std::chrono::steady_clock::time_point discoveredAt; // Discovery timestamp
    
    // Validation
    bool isValid() const;
    std::string toString() const;
};

/**
 * @brief ONVIF device discovery service using WS-Discovery
 * 
 * This class implements ONVIF device discovery using the WS-Discovery protocol.
 * It can discover ONVIF-compliant cameras and devices on the local network
 * and retrieve their service information.
 * 
 * Features:
 * - Multicast WS-Discovery probe
 * - Device capability detection
 * - Media profile enumeration
 * - RTSP stream URI retrieval
 * - Authentication support
 */
class ONVIFDiscovery {
public:
    ONVIFDiscovery();
    ~ONVIFDiscovery();
    
    // Discovery operations
    bool initialize();
    void cleanup();
    
    // Device discovery
    std::vector<ONVIFDevice> discoverDevices(int timeoutMs = 5000);
    bool probeDevice(const std::string& ipAddress, ONVIFDevice& device);
    
    // Device information retrieval
    bool getDeviceInformation(ONVIFDevice& device);
    bool getMediaProfiles(ONVIFDevice& device);
    bool getStreamUri(ONVIFDevice& device);
    
    // Authentication
    bool testAuthentication(const ONVIFDevice& device, const std::string& username, const std::string& password);
    void setDefaultCredentials(const std::string& username, const std::string& password);
    
    // Configuration
    void setDiscoveryTimeout(int timeoutMs);
    void setMaxDevices(size_t maxDevices);
    void setNetworkInterface(const std::string& interfaceName);
    
    // Status
    bool isInitialized() const;
    size_t getDiscoveredDeviceCount() const;
    std::vector<ONVIFDevice> getCachedDevices() const;
    void clearCache();
    
    // Error handling
    std::string getLastError() const;
    
private:
    // Internal discovery methods
    bool initializeNetwork();
    bool sendProbeMessage();
    bool receiveProbeMatches(std::vector<ONVIFDevice>& devices, int timeoutMs);
    bool parseProbeMatch(const std::string& response, ONVIFDevice& device);
    
    // SOAP/HTTP communication
    bool sendSOAPRequest(const std::string& url, const std::string& soapAction, 
                        const std::string& soapBody, std::string& response,
                        const std::string& username = "", const std::string& password = "");
    std::string createSOAPEnvelope(const std::string& body, const std::string& username = "", 
                                  const std::string& password = "");
    std::string generateWSSecurity(const std::string& username, const std::string& password);
    
    // XML parsing helpers
    std::string extractXMLValue(const std::string& xml, const std::string& tag);
    std::vector<std::string> extractXMLValues(const std::string& xml, const std::string& tag);
    
    // Network utilities
    std::string getLocalIPAddress();
    bool isValidIPAddress(const std::string& ip);
    
    // Member variables
    bool m_initialized;
    int m_discoverySocket;
    std::string m_networkInterface;
    int m_timeoutMs;
    size_t m_maxDevices;
    std::string m_defaultUsername;
    std::string m_defaultPassword;
    
    // Device cache
    mutable std::mutex m_devicesMutex;
    std::vector<ONVIFDevice> m_cachedDevices;
    std::chrono::steady_clock::time_point m_lastDiscovery;
    
    // Error handling
    mutable std::mutex m_errorMutex;
    std::string m_lastError;
    
    // Constants
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;
    static constexpr size_t DEFAULT_MAX_DEVICES = 32;
    static constexpr int MULTICAST_PORT = 3702;
    static constexpr const char* MULTICAST_ADDRESS = "239.255.255.250";
    static constexpr const char* WS_DISCOVERY_PROBE_MESSAGE = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
        "xmlns:wsdp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\" "
        "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
        "<soap:Header>"
        "<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</wsa:Action>"
        "<wsa:MessageID>urn:uuid:%s</wsa:MessageID>"
        "<wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"
        "</soap:Header>"
        "<soap:Body>"
        "<wsd:Probe>"
        "<wsd:Types>dn:NetworkVideoTransmitter</wsd:Types>"
        "</wsd:Probe>"
        "</soap:Body>"
        "</soap:Envelope>";
    
    // Helper methods
    std::string generateUUID();
    void setLastError(const std::string& error);
    void logDebug(const std::string& message);
    void logError(const std::string& message);
};

/**
 * @brief ONVIF Discovery Manager - High-level interface for device discovery
 * 
 * This class provides a simplified interface for ONVIF device discovery
 * and management, integrating with the TaskManager for automatic device
 * configuration.
 */
class ONVIFManager {
public:
    ONVIFManager();
    ~ONVIFManager();
    
    // Manager operations
    bool initialize();
    void shutdown();
    
    // Discovery operations
    std::vector<ONVIFDevice> scanNetwork(int timeoutMs = 5000);
    bool addDiscoveredDevice(const ONVIFDevice& device);
    bool removeDevice(const std::string& deviceId);
    
    // Device management
    std::vector<ONVIFDevice> getKnownDevices() const;
    ONVIFDevice* findDevice(const std::string& deviceId);
    bool updateDeviceCredentials(const std::string& deviceId, const std::string& username, const std::string& password);
    
    // Auto-configuration
    void setAutoAddDevices(bool enabled);
    bool isAutoAddEnabled() const;
    
    // Status
    bool isInitialized() const;
    std::string getLastError() const;
    
private:
    std::unique_ptr<ONVIFDiscovery> m_discovery;
    std::vector<ONVIFDevice> m_knownDevices;
    mutable std::mutex m_devicesMutex;
    bool m_autoAddDevices;
    bool m_initialized;
    std::string m_lastError;
    
    // Helper methods
    bool configureDevice(ONVIFDevice& device);
    std::string generateDeviceId(const ONVIFDevice& device);
};
