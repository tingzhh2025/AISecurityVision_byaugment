#include "AlarmTrigger.h"
#include "WebSocketServer.h"
#include "../core/VideoPipeline.h"
#include "../ai/BehaviorAnalyzer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <curl/curl.h>

#ifdef HAVE_MQTT
#ifdef USE_SIMPLE_MQTT
#include "../../third_party/mqtt/simple_mqtt.h"
#else
#include <mqtt/async_client.h>
#endif
#endif

// Callback function for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string AlarmPayload::toJson() const {
    std::ostringstream json;
    json << "{"
         << "\"event_type\":\"" << event_type << "\","
         << "\"camera_id\":\"" << camera_id << "\","
         << "\"rule_id\":\"" << rule_id << "\","
         << "\"object_id\":\"" << object_id << "\","
         << "\"confidence\":" << std::fixed << std::setprecision(3) << confidence << ","
         << "\"timestamp\":\"" << timestamp << "\","
         << "\"metadata\":\"" << metadata << "\","
         << "\"bounding_box\":{"
         << "\"x\":" << bounding_box.x << ","
         << "\"y\":" << bounding_box.y << ","
         << "\"width\":" << bounding_box.width << ","
         << "\"height\":" << bounding_box.height
         << "},"
         << "\"test_mode\":" << (test_mode ? "true" : "false")
         << "}";
    return json.str();
}

AlarmTrigger::AlarmTrigger() {
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

#ifdef HAVE_WEBSOCKETPP
    // Initialize WebSocket server
    m_webSocketServer = std::make_unique<WebSocketServer>();
#endif
}

AlarmTrigger::~AlarmTrigger() {
    shutdown();
    curl_global_cleanup();
}

bool AlarmTrigger::initialize() {
    std::lock_guard<std::mutex> lock(m_configMutex);

    if (m_running.load()) {
        std::cout << "[AlarmTrigger] Already initialized" << std::endl;
        return true;
    }

    m_running.store(true);
    m_processingThread = std::thread(&AlarmTrigger::processAlarmQueue, this);

    std::cout << "[AlarmTrigger] Initialized with HTTP POST delivery support" << std::endl;
    return true;
}

void AlarmTrigger::shutdown() {
    if (m_running.load()) {
        m_running.store(false);
        m_queueCondition.notify_all();

        if (m_processingThread.joinable()) {
            m_processingThread.join();
        }

#ifdef HAVE_WEBSOCKETPP
        // Stop WebSocket server
        stopWebSocketServer();
#endif

#ifdef HAVE_MQTT
        // Disconnect MQTT client
        disconnectMQTTClient();
#endif

        std::cout << "[AlarmTrigger] Shutdown complete" << std::endl;
    }
}

void AlarmTrigger::triggerAlarm(const FrameResult& result) {
    if (!m_running.load()) {
        return;
    }

    // Process each behavior event in the result
    for (const auto& event : result.events) {
        AlarmPayload payload = createAlarmPayload(result, event);

        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Check queue size limit
        if (m_alarmQueue.size() >= MAX_QUEUE_SIZE) {
            std::cerr << "[AlarmTrigger] Alarm queue full, dropping oldest alarm" << std::endl;
            m_alarmQueue.pop();
        }

        m_alarmQueue.push(payload);
        m_queueCondition.notify_one();

        std::cout << "[AlarmTrigger] Queued alarm: " << event.eventType
                  << " for camera: " << payload.camera_id << std::endl;
    }
}

void AlarmTrigger::triggerTestAlarm(const std::string& eventType, const std::string& cameraId) {
    if (!m_running.load()) {
        return;
    }

    AlarmPayload payload;
    payload.event_type = eventType;
    payload.camera_id = cameraId;
    payload.rule_id = "test_rule";
    payload.object_id = "test_object";
    payload.confidence = 0.95;
    payload.timestamp = getCurrentTimestamp();
    payload.metadata = "Test alarm generated via API";
    payload.bounding_box = cv::Rect(100, 100, 200, 200);
    payload.test_mode = true;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_alarmQueue.push(payload);
    m_queueCondition.notify_one();

    std::cout << "[AlarmTrigger] Queued test alarm: " << eventType
              << " for camera: " << cameraId << std::endl;
}

// Configuration management methods
bool AlarmTrigger::addAlarmConfig(const AlarmConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    // Check if config with same ID already exists
    for (const auto& existing : m_alarmConfigs) {
        if (existing.id == config.id) {
            std::cerr << "[AlarmTrigger] Config with ID " << config.id << " already exists" << std::endl;
            return false;
        }
    }

    m_alarmConfigs.push_back(config);
    std::cout << "[AlarmTrigger] Added alarm config: " << config.id
              << " (method: " << (config.method == AlarmMethod::HTTP_POST ? "HTTP_POST" : "OTHER") << ")" << std::endl;
    return true;
}

bool AlarmTrigger::removeAlarmConfig(const std::string& configId) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    auto it = std::remove_if(m_alarmConfigs.begin(), m_alarmConfigs.end(),
                            [&configId](const AlarmConfig& config) {
                                return config.id == configId;
                            });

    if (it != m_alarmConfigs.end()) {
        m_alarmConfigs.erase(it, m_alarmConfigs.end());
        std::cout << "[AlarmTrigger] Removed alarm config: " << configId << std::endl;
        return true;
    }

    std::cerr << "[AlarmTrigger] Config not found: " << configId << std::endl;
    return false;
}

bool AlarmTrigger::updateAlarmConfig(const AlarmConfig& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    for (auto& existing : m_alarmConfigs) {
        if (existing.id == config.id) {
            existing = config;
            std::cout << "[AlarmTrigger] Updated alarm config: " << config.id << std::endl;
            return true;
        }
    }

    std::cerr << "[AlarmTrigger] Config not found for update: " << config.id << std::endl;
    return false;
}

std::vector<AlarmConfig> AlarmTrigger::getAlarmConfigs() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_alarmConfigs;
}

// Status and statistics methods
size_t AlarmTrigger::getPendingAlarmsCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_alarmQueue.size();
}

size_t AlarmTrigger::getDeliveredAlarmsCount() const {
    return m_deliveredCount.load();
}

size_t AlarmTrigger::getFailedAlarmsCount() const {
    return m_failedCount.load();
}

// Private alarm processing methods
void AlarmTrigger::processAlarmQueue() {
    std::cout << "[AlarmTrigger] Alarm processing thread started" << std::endl;

    while (m_running.load()) {
        std::unique_lock<std::mutex> lock(m_queueMutex);

        // Wait for alarms or shutdown signal
        m_queueCondition.wait(lock, [this] {
            return !m_alarmQueue.empty() || !m_running.load();
        });

        if (!m_running.load()) {
            break;
        }

        // Process all pending alarms
        while (!m_alarmQueue.empty()) {
            AlarmPayload payload = m_alarmQueue.front();
            m_alarmQueue.pop();
            lock.unlock();

            // Deliver alarm to all configured destinations
            deliverAlarm(payload);

            lock.lock();
        }
    }

    std::cout << "[AlarmTrigger] Alarm processing thread stopped" << std::endl;
}

void AlarmTrigger::deliverAlarm(const AlarmPayload& payload) {
    std::lock_guard<std::mutex> lock(m_configMutex);

    bool delivered = false;

    for (const auto& config : m_alarmConfigs) {
        if (!config.enabled) {
            continue;
        }

        try {
            switch (config.method) {
                case AlarmMethod::HTTP_POST:
                    deliverHttpAlarm(payload, config.httpConfig);
                    delivered = true;
                    break;

                case AlarmMethod::WEBSOCKET:
                    deliverWebSocketAlarm(payload, config.webSocketConfig);
                    delivered = true;
                    break;

                case AlarmMethod::MQTT:
                    deliverMQTTAlarm(payload, config.mqttConfig);
                    delivered = true;
                    break;
            }
        } catch (const std::exception& e) {
            std::cerr << "[AlarmTrigger] Failed to deliver alarm via config "
                      << config.id << ": " << e.what() << std::endl;
            m_failedCount.fetch_add(1);
        }
    }

    if (delivered) {
        m_deliveredCount.fetch_add(1);
        std::cout << "[AlarmTrigger] Successfully delivered alarm: " << payload.event_type << std::endl;
    } else {
        std::cerr << "[AlarmTrigger] No enabled alarm configurations found" << std::endl;
        m_failedCount.fetch_add(1);
    }
}

void AlarmTrigger::deliverHttpAlarm(const AlarmPayload& payload, const HttpAlarmConfig& config) {
    if (!config.enabled || config.url.empty()) {
        std::cerr << "[AlarmTrigger] HTTP config disabled or invalid URL" << std::endl;
        return;
    }

    std::string jsonPayload = payload.toJson();

    bool success = sendHttpPost(config.url, jsonPayload, config.headers, config.timeout_ms);

    if (success) {
        std::cout << "[AlarmTrigger] HTTP alarm delivered to: " << config.url << std::endl;
    } else {
        std::cerr << "[AlarmTrigger] Failed to deliver HTTP alarm to: " << config.url << std::endl;
        throw std::runtime_error("HTTP delivery failed");
    }
}

// HTTP client functionality
bool AlarmTrigger::sendHttpPost(const std::string& url, const std::string& jsonPayload,
                               const std::map<std::string, std::string>& headers, int timeout_ms) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[AlarmTrigger] Failed to initialize CURL" << std::endl;
        return false;
    }

    std::string response;
    struct curl_slist* headerList = nullptr;

    try {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Set POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonPayload.length());

        // Set headers
        for (const auto& header : headers) {
            std::string headerStr = header.first + ": " + header.second;
            headerList = curl_slist_append(headerList, headerStr.c_str());
        }
        if (headerList) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        }

        // Set timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

        // Set response callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Disable SSL verification for testing (should be configurable in production)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "[AlarmTrigger] CURL error: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        // Check HTTP response code
        long responseCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode >= 200 && responseCode < 300) {
            std::cout << "[AlarmTrigger] HTTP POST successful (code: " << responseCode << ")" << std::endl;
            return true;
        } else {
            std::cerr << "[AlarmTrigger] HTTP POST failed with code: " << responseCode << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "[AlarmTrigger] Exception in HTTP POST: " << e.what() << std::endl;
        return false;
    }

    // Cleanup
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    return false;
}

// Utility methods
std::string AlarmTrigger::generateAlarmId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::ostringstream id;
    id << "alarm_" << timestamp;
    return id.str();
}

std::string AlarmTrigger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream timestamp;
    timestamp << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    timestamp << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";

    return timestamp.str();
}

AlarmPayload AlarmTrigger::createAlarmPayload(const FrameResult& result, const BehaviorEvent& event) const {
    AlarmPayload payload;

    payload.event_type = event.eventType;
    payload.camera_id = "camera_" + std::to_string(std::hash<std::string>{}(result.frame.data ? "active" : "inactive"));
    payload.rule_id = event.ruleId;
    payload.object_id = event.objectId;
    payload.confidence = event.confidence;
    payload.timestamp = event.timestamp.empty() ? getCurrentTimestamp() : event.timestamp;
    payload.metadata = event.metadata;
    payload.bounding_box = event.boundingBox;
    payload.test_mode = false;

    return payload;
}

// WebSocket delivery implementation
void AlarmTrigger::deliverWebSocketAlarm(const AlarmPayload& payload, const WebSocketAlarmConfig& config) {
#ifdef HAVE_WEBSOCKETPP
    if (!config.enabled) {
        std::cerr << "[AlarmTrigger] WebSocket config disabled" << std::endl;
        return;
    }

    if (!m_webSocketServer || !m_webSocketServer->isRunning()) {
        std::cerr << "[AlarmTrigger] WebSocket server not running" << std::endl;
        throw std::runtime_error("WebSocket server not available");
    }

    std::string jsonPayload = payload.toJson();
    m_webSocketServer->broadcast(jsonPayload);

    std::cout << "[AlarmTrigger] WebSocket alarm broadcasted to "
              << m_webSocketServer->getConnectionCount() << " clients" << std::endl;
#else
    std::cerr << "[AlarmTrigger] WebSocket support not compiled" << std::endl;
    throw std::runtime_error("WebSocket support not available");
#endif
}

// WebSocket server management
void AlarmTrigger::startWebSocketServer(int port) {
#ifdef HAVE_WEBSOCKETPP
    if (m_webSocketRunning.load()) {
        std::cout << "[AlarmTrigger] WebSocket server already running" << std::endl;
        return;
    }

    if (!m_webSocketServer) {
        m_webSocketServer = std::make_unique<WebSocketServer>();
    }

    if (m_webSocketServer->start(port)) {
        m_webSocketRunning.store(true);
        std::cout << "[AlarmTrigger] WebSocket server started on port " << port << std::endl;
    } else {
        std::cerr << "[AlarmTrigger] Failed to start WebSocket server" << std::endl;
    }
#else
    std::cerr << "[AlarmTrigger] WebSocket support not compiled" << std::endl;
#endif
}

void AlarmTrigger::stopWebSocketServer() {
#ifdef HAVE_WEBSOCKETPP
    if (!m_webSocketRunning.load()) {
        return;
    }

    if (m_webSocketServer) {
        m_webSocketServer->stop();
        m_webSocketRunning.store(false);
        std::cout << "[AlarmTrigger] WebSocket server stopped" << std::endl;
    }
#endif
}

void AlarmTrigger::broadcastToWebSocketClients(const std::string& message) {
#ifdef HAVE_WEBSOCKETPP
    if (m_webSocketServer && m_webSocketServer->isRunning()) {
        m_webSocketServer->broadcast(message);
    }
#endif
}

// MQTT delivery implementation
void AlarmTrigger::deliverMQTTAlarm(const AlarmPayload& payload, const MQTTAlarmConfig& config) {
#ifdef HAVE_MQTT
    if (!config.enabled) {
        std::cerr << "[AlarmTrigger] MQTT config disabled" << std::endl;
        return;
    }

    // Connect to MQTT broker if not connected or config changed
    if (!m_mqttConnected.load() ||
        m_currentMQTTConfig.broker != config.broker ||
        m_currentMQTTConfig.port != config.port) {

        if (!connectMQTTClient(config)) {
            throw std::runtime_error("Failed to connect to MQTT broker");
        }
    }

    std::string jsonPayload = payload.toJson();

    if (!publishMQTTMessage(config.topic, jsonPayload, config.qos, config.retain)) {
        throw std::runtime_error("Failed to publish MQTT message");
    }

    std::cout << "[AlarmTrigger] MQTT alarm published to " << config.broker
              << " topic: " << config.topic << " (QoS " << config.qos << ")" << std::endl;
#else
    std::cerr << "[AlarmTrigger] MQTT support not compiled" << std::endl;
    throw std::runtime_error("MQTT support not available");
#endif
}

// MQTT client management
bool AlarmTrigger::connectMQTTClient(const MQTTAlarmConfig& config) {
#ifdef HAVE_MQTT
    try {
        // Disconnect existing client if any
        disconnectMQTTClient();

#ifdef USE_SIMPLE_MQTT
        m_mqttClient = std::make_unique<SimpleMQTTClient>(config.broker, config.port);

        m_mqttClient->setConnectionTimeout(config.connection_timeout_ms);
        m_mqttClient->setKeepAlive(config.keep_alive_seconds);
        m_mqttClient->setAutoReconnect(config.auto_reconnect);

        if (m_mqttClient->connect(config.client_id, config.username, config.password)) {
            m_mqttConnected.store(true);
            m_currentMQTTConfig = config;
            std::cout << "[AlarmTrigger] Connected to MQTT broker: " << config.broker
                      << ":" << config.port << std::endl;
            return true;
        } else {
            std::cerr << "[AlarmTrigger] Failed to connect to MQTT broker: "
                      << m_mqttClient->getLastError() << std::endl;
            return false;
        }
#else
        // TODO: Implement Paho MQTT C++ client connection
        std::cerr << "[AlarmTrigger] Paho MQTT C++ client not yet implemented" << std::endl;
        return false;
#endif

    } catch (const std::exception& e) {
        std::cerr << "[AlarmTrigger] MQTT connection error: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[AlarmTrigger] MQTT support not compiled" << std::endl;
    return false;
#endif
}

void AlarmTrigger::disconnectMQTTClient() {
#ifdef HAVE_MQTT
    if (m_mqttClient) {
        m_mqttClient->disconnect();
        m_mqttClient.reset();
        m_mqttConnected.store(false);
        std::cout << "[AlarmTrigger] Disconnected from MQTT broker" << std::endl;
    }
#endif
}

bool AlarmTrigger::publishMQTTMessage(const std::string& topic, const std::string& payload, int qos, bool retain) {
#ifdef HAVE_MQTT
    if (!m_mqttClient || !m_mqttConnected.load()) {
        std::cerr << "[AlarmTrigger] MQTT client not connected" << std::endl;
        return false;
    }

    try {
        return m_mqttClient->publish(topic, payload, qos, retain);
    } catch (const std::exception& e) {
        std::cerr << "[AlarmTrigger] MQTT publish error: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[AlarmTrigger] MQTT support not compiled" << std::endl;
    return false;
#endif
}
