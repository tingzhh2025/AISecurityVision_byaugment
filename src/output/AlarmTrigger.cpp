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
         << "\"alarm_id\":\"" << alarm_id << "\","
         << "\"event_type\":\"" << event_type << "\","
         << "\"camera_id\":\"" << camera_id << "\","
         << "\"rule_id\":\"" << rule_id << "\","
         << "\"object_id\":\"" << object_id << "\","
         << "\"confidence\":" << std::fixed << std::setprecision(3) << confidence << ","
         << "\"priority\":" << priority << ","
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

        // Determine priority based on event type and confidence
        payload.priority = calculateAlarmPriority(event.eventType, payload.confidence);
        payload.alarm_id = generateAlarmId();

        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Check queue size limit
        if (m_alarmQueue.size() >= MAX_QUEUE_SIZE) {
            std::cerr << "[AlarmTrigger] Alarm queue full, dropping lowest priority alarm" << std::endl;
            m_alarmQueue.pop();
        }

        m_alarmQueue.push(payload);
        m_queueCondition.notify_one();

        std::cout << "[AlarmTrigger] Queued alarm: " << event.eventType
                  << " for camera: " << payload.camera_id
                  << " (Priority: " << payload.priority
                  << ", ID: " << payload.alarm_id << ")" << std::endl;
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
    payload.priority = 3;  // Medium priority for test alarms
    payload.alarm_id = generateAlarmId();

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_alarmQueue.push(payload);
    m_queueCondition.notify_one();

    std::cout << "[AlarmTrigger] Queued test alarm: " << eventType
              << " for camera: " << cameraId
              << " (Priority: " << payload.priority
              << ", ID: " << payload.alarm_id << ")" << std::endl;
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

        // Process all pending alarms (highest priority first)
        while (!m_alarmQueue.empty()) {
            AlarmPayload payload = m_alarmQueue.top();
            m_alarmQueue.pop();
            lock.unlock();

            // Deliver alarm to all configured destinations with routing
            AlarmRoutingResult result = deliverAlarm(payload);

            // Store routing result in history
            {
                std::lock_guard<std::mutex> historyLock(m_routingHistoryMutex);
                m_routingHistory.push_back(result);

                // Limit history size
                if (m_routingHistory.size() > MAX_ROUTING_HISTORY) {
                    m_routingHistory.erase(m_routingHistory.begin());
                }
            }

            lock.lock();
        }
    }

    std::cout << "[AlarmTrigger] Alarm processing thread stopped" << std::endl;
}

AlarmRoutingResult AlarmTrigger::deliverAlarm(const AlarmPayload& payload) {
    auto startTime = std::chrono::high_resolution_clock::now();
    AlarmRoutingResult routingResult(payload.alarm_id);

    std::vector<AlarmConfig> configs;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        configs = m_alarmConfigs;  // Copy configs to avoid holding lock during delivery
    }

    // Filter enabled configs
    std::vector<AlarmConfig> enabledConfigs;
    for (const auto& config : configs) {
        if (config.enabled) {
            enabledConfigs.push_back(config);
        }
    }

    if (enabledConfigs.empty()) {
        std::cerr << "[AlarmTrigger] No enabled alarm configurations found" << std::endl;
        m_failedCount.fetch_add(1);
        return routingResult;
    }

    std::cout << "[AlarmTrigger] Delivering alarm " << payload.alarm_id
              << " to " << enabledConfigs.size() << " channels simultaneously" << std::endl;

    // Prepare futures for parallel delivery
    std::vector<std::future<DeliveryResult>> futures;
    std::vector<std::promise<DeliveryResult>> promises(enabledConfigs.size());

    // Launch parallel delivery tasks
    for (size_t i = 0; i < enabledConfigs.size(); ++i) {
        futures.push_back(promises[i].get_future());

        std::thread deliveryThread(&AlarmTrigger::deliverToChannelAsync, this,
                                 payload, enabledConfigs[i], std::ref(promises[i]));
        deliveryThread.detach();
    }

    // Collect results from all delivery attempts
    for (auto& future : futures) {
        try {
            // Wait for delivery with timeout (max 10 seconds per channel)
            if (future.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
                DeliveryResult result = future.get();
                routingResult.delivery_results.push_back(result);

                if (result.success) {
                    routingResult.successful_deliveries++;
                    m_deliveredCount.fetch_add(1);
                } else {
                    routingResult.failed_deliveries++;
                    m_failedCount.fetch_add(1);
                }
            } else {
                // Timeout occurred
                routingResult.delivery_results.emplace_back("timeout", AlarmMethod::HTTP_POST,
                                                          false, std::chrono::milliseconds(10000),
                                                          "Delivery timeout");
                routingResult.failed_deliveries++;
                m_failedCount.fetch_add(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "[AlarmTrigger] Exception during delivery: " << e.what() << std::endl;
            routingResult.delivery_results.emplace_back("exception", AlarmMethod::HTTP_POST,
                                                      false, std::chrono::milliseconds(0),
                                                      e.what());
            routingResult.failed_deliveries++;
            m_failedCount.fetch_add(1);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    routingResult.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "[AlarmTrigger] Alarm " << payload.alarm_id << " routing complete: "
              << routingResult.successful_deliveries << " successful, "
              << routingResult.failed_deliveries << " failed, "
              << routingResult.total_time.count() << "ms total" << std::endl;

    return routingResult;
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

// New routing system helper methods
void AlarmTrigger::deliverToChannelAsync(const AlarmPayload& payload, const AlarmConfig& config,
                                        std::promise<DeliveryResult>& promise) {
    try {
        DeliveryResult result("unknown", config.method, false, std::chrono::milliseconds(0));

        switch (config.method) {
            case AlarmMethod::HTTP_POST:
                result = deliverHttpAlarm(payload, config);
                break;
            case AlarmMethod::WEBSOCKET:
                result = deliverWebSocketAlarm(payload, config);
                break;
            case AlarmMethod::MQTT:
                result = deliverMQTTAlarm(payload, config);
                break;
        }

        promise.set_value(result);
    } catch (const std::exception& e) {
        DeliveryResult errorResult(config.id, config.method, false,
                                 std::chrono::milliseconds(0), e.what());
        promise.set_value(errorResult);
    }
}

DeliveryResult AlarmTrigger::deliverHttpAlarm(const AlarmPayload& payload, const AlarmConfig& config) {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!config.httpConfig.enabled || config.httpConfig.url.empty()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return DeliveryResult(config.id, AlarmMethod::HTTP_POST, false, duration,
                            "HTTP config disabled or invalid URL");
    }

    std::string jsonPayload = payload.toJson();
    bool success = sendHttpPost(config.httpConfig.url, jsonPayload,
                               config.httpConfig.headers, config.httpConfig.timeout_ms);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (success) {
        std::cout << "[AlarmTrigger] HTTP alarm delivered to: " << config.httpConfig.url
                  << " (" << duration.count() << "ms)" << std::endl;
        return DeliveryResult(config.id, AlarmMethod::HTTP_POST, true, duration);
    } else {
        std::cerr << "[AlarmTrigger] Failed to deliver HTTP alarm to: " << config.httpConfig.url << std::endl;
        return DeliveryResult(config.id, AlarmMethod::HTTP_POST, false, duration, "HTTP delivery failed");
    }
}

DeliveryResult AlarmTrigger::deliverWebSocketAlarm(const AlarmPayload& payload, const AlarmConfig& config) {
    auto startTime = std::chrono::high_resolution_clock::now();

#ifdef HAVE_WEBSOCKETPP
    if (!config.webSocketConfig.enabled) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return DeliveryResult(config.id, AlarmMethod::WEBSOCKET, false, duration,
                            "WebSocket config disabled");
    }

    if (!m_webSocketServer || !m_webSocketServer->isRunning()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return DeliveryResult(config.id, AlarmMethod::WEBSOCKET, false, duration,
                            "WebSocket server not running");
    }

    std::string jsonPayload = payload.toJson();
    m_webSocketServer->broadcast(jsonPayload);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "[AlarmTrigger] WebSocket alarm broadcasted to "
              << m_webSocketServer->getConnectionCount() << " clients ("
              << duration.count() << "ms)" << std::endl;

    return DeliveryResult(config.id, AlarmMethod::WEBSOCKET, true, duration);
#else
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    return DeliveryResult(config.id, AlarmMethod::WEBSOCKET, false, duration,
                        "WebSocket support not compiled");
#endif
}

DeliveryResult AlarmTrigger::deliverMQTTAlarm(const AlarmPayload& payload, const AlarmConfig& config) {
    auto startTime = std::chrono::high_resolution_clock::now();

#ifdef HAVE_MQTT
    if (!config.mqttConfig.enabled) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return DeliveryResult(config.id, AlarmMethod::MQTT, false, duration,
                            "MQTT config disabled");
    }

    // Connect to MQTT broker if not connected or config changed
    if (!m_mqttConnected.load() ||
        m_currentMQTTConfig.broker != config.mqttConfig.broker ||
        m_currentMQTTConfig.port != config.mqttConfig.port) {

        if (!connectMQTTClient(config.mqttConfig)) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            return DeliveryResult(config.id, AlarmMethod::MQTT, false, duration,
                                "Failed to connect to MQTT broker");
        }
    }

    std::string jsonPayload = payload.toJson();

    if (!publishMQTTMessage(config.mqttConfig.topic, jsonPayload,
                           config.mqttConfig.qos, config.mqttConfig.retain)) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        return DeliveryResult(config.id, AlarmMethod::MQTT, false, duration,
                            "Failed to publish MQTT message");
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "[AlarmTrigger] MQTT alarm published to " << config.mqttConfig.broker
              << " topic: " << config.mqttConfig.topic << " (QoS " << config.mqttConfig.qos
              << ", " << duration.count() << "ms)" << std::endl;

    return DeliveryResult(config.id, AlarmMethod::MQTT, true, duration);
#else
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    return DeliveryResult(config.id, AlarmMethod::MQTT, false, duration,
                        "MQTT support not compiled");
#endif
}

// Priority calculation helper
int AlarmTrigger::calculateAlarmPriority(const std::string& eventType, double confidence) const {
    // Priority calculation based on event type and confidence
    int basePriority = 1;

    if (eventType == "intrusion" || eventType == "unauthorized_access") {
        basePriority = 5;  // Highest priority
    } else if (eventType == "motion_detected" || eventType == "object_detected") {
        basePriority = 3;  // Medium priority
    } else if (eventType == "loitering" || eventType == "abandoned_object") {
        basePriority = 2;  // Low-medium priority
    } else {
        basePriority = 1;  // Default priority
    }

    // Adjust based on confidence (higher confidence = higher priority)
    if (confidence >= 0.9) {
        basePriority = std::min(5, basePriority + 1);
    } else if (confidence < 0.5) {
        basePriority = std::max(1, basePriority - 1);
    }

    return basePriority;
}

// Routing system status methods
AlarmRoutingResult AlarmTrigger::getLastRoutingResult() const {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);

    if (m_routingHistory.empty()) {
        return AlarmRoutingResult("no_results");
    }

    return m_routingHistory.back();
}

std::vector<AlarmRoutingResult> AlarmTrigger::getRecentRoutingResults(size_t count) const {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);

    std::vector<AlarmRoutingResult> results;

    if (m_routingHistory.empty()) {
        return results;
    }

    size_t startIndex = m_routingHistory.size() > count ? m_routingHistory.size() - count : 0;

    for (size_t i = startIndex; i < m_routingHistory.size(); ++i) {
        results.push_back(m_routingHistory[i]);
    }

    return results;
}

void AlarmTrigger::clearRoutingHistory() {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);
    m_routingHistory.clear();
    std::cout << "[AlarmTrigger] Routing history cleared" << std::endl;
}

// Performance monitoring methods
double AlarmTrigger::getAverageDeliveryTime() const {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);

    if (m_routingHistory.empty()) {
        return 0.0;
    }

    double totalTime = 0.0;
    size_t totalDeliveries = 0;

    for (const auto& result : m_routingHistory) {
        for (const auto& delivery : result.delivery_results) {
            totalTime += delivery.delivery_time.count();
            totalDeliveries++;
        }
    }

    return totalDeliveries > 0 ? totalTime / totalDeliveries : 0.0;
}

std::map<AlarmMethod, double> AlarmTrigger::getDeliveryTimesByMethod() const {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);

    std::map<AlarmMethod, double> methodTimes;
    std::map<AlarmMethod, size_t> methodCounts;

    for (const auto& result : m_routingHistory) {
        for (const auto& delivery : result.delivery_results) {
            methodTimes[delivery.method] += delivery.delivery_time.count();
            methodCounts[delivery.method]++;
        }
    }

    // Calculate averages
    for (auto& pair : methodTimes) {
        if (methodCounts[pair.first] > 0) {
            pair.second /= methodCounts[pair.first];
        }
    }

    return methodTimes;
}

std::map<AlarmMethod, double> AlarmTrigger::getSuccessRatesByMethod() const {
    std::lock_guard<std::mutex> lock(m_routingHistoryMutex);

    std::map<AlarmMethod, size_t> methodSuccesses;
    std::map<AlarmMethod, size_t> methodTotals;
    std::map<AlarmMethod, double> successRates;

    for (const auto& result : m_routingHistory) {
        for (const auto& delivery : result.delivery_results) {
            methodTotals[delivery.method]++;
            if (delivery.success) {
                methodSuccesses[delivery.method]++;
            }
        }
    }

    // Calculate success rates
    for (const auto& pair : methodTotals) {
        AlarmMethod method = pair.first;
        size_t total = pair.second;
        size_t successes = methodSuccesses[method];

        successRates[method] = total > 0 ? (double)successes / total * 100.0 : 0.0;
    }

    return successRates;
}
