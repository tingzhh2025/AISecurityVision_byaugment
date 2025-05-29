#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <map>
#include <chrono>
#include <future>
#include <opencv2/opencv.hpp>

// Forward declarations
struct FrameResult;
struct BehaviorEvent;

/**
 * @brief Alarm delivery method configuration
 */
enum class AlarmMethod {
    HTTP_POST,
    WEBSOCKET,
    MQTT
};

/**
 * @brief HTTP alarm configuration
 */
struct HttpAlarmConfig {
    std::string url;
    std::string method = "POST";
    std::map<std::string, std::string> headers;
    int timeout_ms = 5000;
    bool enabled = true;

    HttpAlarmConfig() = default;
    HttpAlarmConfig(const std::string& endpoint) : url(endpoint) {
        headers["Content-Type"] = "application/json";
        headers["User-Agent"] = "AISecurityVision/1.0";
    }
};

/**
 * @brief WebSocket alarm configuration
 */
struct WebSocketAlarmConfig {
    std::string endpoint = "/ws/alarms";
    int port = 8081;
    bool enabled = true;
    int max_connections = 100;
    int ping_interval_ms = 30000;  // 30 seconds

    WebSocketAlarmConfig() = default;
    WebSocketAlarmConfig(int ws_port) : port(ws_port) {}
};

/**
 * @brief MQTT alarm configuration
 */
struct MQTTAlarmConfig {
    std::string broker = "localhost";
    int port = 1883;
    std::string topic = "aibox/alarms";
    std::string client_id = "";  // Auto-generated if empty
    std::string username = "";
    std::string password = "";
    int qos = 1;  // Quality of Service (0, 1, or 2)
    bool retain = false;
    int keep_alive_seconds = 60;
    int connection_timeout_ms = 10000;
    bool auto_reconnect = true;
    bool enabled = true;

    MQTTAlarmConfig() = default;
    MQTTAlarmConfig(const std::string& broker_host) : broker(broker_host) {}
};

/**
 * @brief Alarm configuration structure
 */
struct AlarmConfig {
    std::string id;
    AlarmMethod method;
    HttpAlarmConfig httpConfig;
    WebSocketAlarmConfig webSocketConfig;
    MQTTAlarmConfig mqttConfig;
    bool enabled = true;
    int priority = 1;  // 1-5 scale

    AlarmConfig() : method(AlarmMethod::HTTP_POST) {}
};

/**
 * @brief Alarm delivery payload with priority support
 */
struct AlarmPayload {
    std::string event_type;
    std::string camera_id;
    std::string rule_id;
    std::string object_id;
    std::string reid_id;           // Task 77: Global ReID track ID for cross-camera persistence
    int local_track_id = -1;       // Task 77: Local track ID as integer
    int global_track_id = -1;      // Task 77: Global track ID as integer
    double confidence;
    std::string timestamp;
    std::string metadata;
    cv::Rect bounding_box;
    bool test_mode = false;
    int priority = 1;  // 1-5 scale (5 = highest priority)
    std::string alarm_id;  // Unique alarm identifier

    // Convert to JSON string
    std::string toJson() const;

    // Priority comparison for priority queue (higher priority first)
    bool operator<(const AlarmPayload& other) const {
        return priority < other.priority;  // Reverse for max-heap
    }
};

/**
 * @brief Delivery result for individual alarm channels
 */
struct DeliveryResult {
    std::string config_id;
    AlarmMethod method;
    bool success;
    std::chrono::milliseconds delivery_time;
    std::string error_message;

    DeliveryResult(const std::string& id, AlarmMethod m, bool s,
                  std::chrono::milliseconds time, const std::string& error = "")
        : config_id(id), method(m), success(s), delivery_time(time), error_message(error) {}
};

/**
 * @brief Alarm routing result containing all delivery attempts
 */
struct AlarmRoutingResult {
    std::string alarm_id;
    std::vector<DeliveryResult> delivery_results;
    std::chrono::milliseconds total_time;
    size_t successful_deliveries;
    size_t failed_deliveries;

    AlarmRoutingResult(const std::string& id)
        : alarm_id(id), total_time(0), successful_deliveries(0), failed_deliveries(0) {}
};

/**
 * @brief Enhanced alarm trigger system with multi-channel routing
 *
 * This class provides:
 * - Multi-channel alarm delivery (HTTP, WebSocket, MQTT)
 * - Priority-based alarm processing with priority queue
 * - Simultaneous delivery to multiple channels
 * - Parallel delivery with performance monitoring
 * - Configurable alarm destinations with priority levels
 * - Comprehensive delivery statistics and routing results
 */
class AlarmTrigger {
public:
    AlarmTrigger();
    ~AlarmTrigger();

    // Initialization
    bool initialize();
    void shutdown();

    // Main alarm triggering
    void triggerAlarm(const FrameResult& result);
    void triggerTestAlarm(const std::string& eventType, const std::string& cameraId);

    // Configuration management
    bool addAlarmConfig(const AlarmConfig& config);
    bool removeAlarmConfig(const std::string& configId);
    bool updateAlarmConfig(const AlarmConfig& config);
    std::vector<AlarmConfig> getAlarmConfigs() const;

    // Status and statistics
    size_t getPendingAlarmsCount() const;
    size_t getDeliveredAlarmsCount() const;
    size_t getFailedAlarmsCount() const;

    // Routing system methods
    AlarmRoutingResult getLastRoutingResult() const;
    std::vector<AlarmRoutingResult> getRecentRoutingResults(size_t count = 10) const;
    void clearRoutingHistory();

    // Performance monitoring
    double getAverageDeliveryTime() const;
    std::map<AlarmMethod, double> getDeliveryTimesByMethod() const;
    std::map<AlarmMethod, double> getSuccessRatesByMethod() const;

    // WebSocket server functionality
    void startWebSocketServer(int port);
    void stopWebSocketServer();
    void broadcastToWebSocketClients(const std::string& message);

private:
    // Alarm processing
    void processAlarmQueue();
    AlarmRoutingResult deliverAlarm(const AlarmPayload& payload);

    // Individual delivery methods with timing
    DeliveryResult deliverHttpAlarm(const AlarmPayload& payload, const AlarmConfig& config);
    DeliveryResult deliverWebSocketAlarm(const AlarmPayload& payload, const AlarmConfig& config);
    DeliveryResult deliverMQTTAlarm(const AlarmPayload& payload, const AlarmConfig& config);

    // Parallel delivery support
    void deliverToChannelAsync(const AlarmPayload& payload, const AlarmConfig& config,
                              std::promise<DeliveryResult>& promise);

    // HTTP client functionality
    bool sendHttpPost(const std::string& url, const std::string& jsonPayload,
                     const std::map<std::string, std::string>& headers, int timeout_ms);

    // MQTT client functionality
    bool connectMQTTClient(const MQTTAlarmConfig& config);
    void disconnectMQTTClient();
    bool publishMQTTMessage(const std::string& topic, const std::string& payload, int qos, bool retain);

    // Utility methods
    std::string generateAlarmId() const;
    std::string getCurrentTimestamp() const;
    AlarmPayload createAlarmPayload(const FrameResult& result, const BehaviorEvent& event) const;
    int calculateAlarmPriority(const std::string& eventType, double confidence) const;

    // Member variables
    std::vector<AlarmConfig> m_alarmConfigs;
    std::priority_queue<AlarmPayload> m_alarmQueue;  // Priority queue for alarm routing

    // Routing history and statistics
    std::vector<AlarmRoutingResult> m_routingHistory;
    mutable std::mutex m_routingHistoryMutex;
    static constexpr size_t MAX_ROUTING_HISTORY = 100;

    // Threading
    std::thread m_processingThread;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_configMutex;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Statistics
    std::atomic<size_t> m_deliveredCount{0};
    std::atomic<size_t> m_failedCount{0};

    // WebSocket server
    std::unique_ptr<class WebSocketServer> m_webSocketServer;
    std::thread m_webSocketThread;
    std::atomic<bool> m_webSocketRunning{false};

    // MQTT client
    std::unique_ptr<class SimpleMQTTClient> m_mqttClient;
    std::atomic<bool> m_mqttConnected{false};
    MQTTAlarmConfig m_currentMQTTConfig;

    // Constants
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    static constexpr int DEFAULT_HTTP_TIMEOUT_MS = 5000;
    static constexpr int DEFAULT_WEBSOCKET_PORT = 8081;
    static constexpr int DEFAULT_MQTT_PORT = 1883;
};
