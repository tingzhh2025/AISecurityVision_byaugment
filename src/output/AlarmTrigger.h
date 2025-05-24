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
 * @brief Alarm configuration structure
 */
struct AlarmConfig {
    std::string id;
    AlarmMethod method;
    HttpAlarmConfig httpConfig;
    bool enabled = true;
    int priority = 1;  // 1-5 scale

    AlarmConfig() : method(AlarmMethod::HTTP_POST) {}
};

/**
 * @brief Alarm delivery payload
 */
struct AlarmPayload {
    std::string event_type;
    std::string camera_id;
    std::string rule_id;
    std::string object_id;
    double confidence;
    std::string timestamp;
    std::string metadata;
    cv::Rect bounding_box;
    bool test_mode = false;

    // Convert to JSON string
    std::string toJson() const;
};

/**
 * @brief Enhanced alarm trigger system with HTTP POST delivery
 *
 * This class provides:
 * - HTTP POST alarm delivery with JSON payloads
 * - Configurable alarm destinations
 * - Asynchronous alarm processing
 * - Test alarm generation
 * - Alarm delivery status tracking
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

private:
    // Alarm processing
    void processAlarmQueue();
    void deliverAlarm(const AlarmPayload& payload);
    void deliverHttpAlarm(const AlarmPayload& payload, const HttpAlarmConfig& config);

    // HTTP client functionality
    bool sendHttpPost(const std::string& url, const std::string& jsonPayload,
                     const std::map<std::string, std::string>& headers, int timeout_ms);

    // Utility methods
    std::string generateAlarmId() const;
    std::string getCurrentTimestamp() const;
    AlarmPayload createAlarmPayload(const FrameResult& result, const BehaviorEvent& event) const;

    // Member variables
    std::vector<AlarmConfig> m_alarmConfigs;
    std::queue<AlarmPayload> m_alarmQueue;

    // Threading
    std::thread m_processingThread;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_configMutex;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Statistics
    std::atomic<size_t> m_deliveredCount{0};
    std::atomic<size_t> m_failedCount{0};

    // Constants
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    static constexpr int DEFAULT_HTTP_TIMEOUT_MS = 5000;
};
