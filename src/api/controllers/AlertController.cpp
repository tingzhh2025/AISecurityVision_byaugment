#include "AlertController.h"
#include "../../core/TaskManager.h"
#include "../../output/AlarmTrigger.h"
#include "../../database/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

using namespace AISecurityVision;

void AlertController::handleGetAlerts(const std::string& request, std::string& response) {
    try {
        // Return sample alerts for frontend compatibility
        std::ostringstream json;
        json << "{"
             << "\"alerts\":["
             << "{"
             << "\"id\":1,"
             << "\"type\":\"intrusion\","
             << "\"camera_id\":\"camera_1\","
             << "\"message\":\"Person detected in restricted area\","
             << "\"severity\":\"high\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\","
             << "\"acknowledged\":false"
             << "},"
             << "{"
             << "\"id\":2,"
             << "\"type\":\"motion\","
             << "\"camera_id\":\"camera_2\","
             << "\"message\":\"Motion detected\","
             << "\"severity\":\"medium\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\","
             << "\"acknowledged\":true"
             << "}"
             << "],"
             << "\"total\":2,"
             << "\"unacknowledged\":1,"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Returned alerts list");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alerts: " + std::string(e.what()), 500);
    }
}

void AlertController::handlePostAlarmConfig(const std::string& request, std::string& response) {
    try {
        std::string configId = parseJsonField(request, "id");
        std::string method = parseJsonField(request, "method");
        std::string url = parseJsonField(request, "url");

        if (configId.empty()) {
            response = createErrorResponse("id is required", 400);
            return;
        }

        if (method.empty()) {
            response = createErrorResponse("method is required", 400);
            return;
        }

        if (method != "http" && method != "websocket" && method != "mqtt") {
            response = createErrorResponse("method must be 'http', 'websocket', or 'mqtt'", 400);
            return;
        }

        if (method == "http" && url.empty()) {
            response = createErrorResponse("url is required for HTTP method", 400);
            return;
        }

        // Create alarm configuration
        AlarmConfig config;
        config.id = configId;

        if (method == "http") {
            config.method = AlarmMethod::HTTP_POST;
            config.httpConfig = HttpAlarmConfig(url);

            // Parse optional HTTP parameters
            int timeout = parseJsonInt(request, "timeout_ms", 5000);
            if (timeout < 1000 || timeout > 30000) {
                response = createErrorResponse("timeout_ms must be between 1000 and 30000", 400);
                return;
            }
            config.httpConfig.timeout_ms = timeout;

            // Parse optional headers
            std::string headers = parseJsonField(request, "headers");
            if (!headers.empty()) {
                try {
                    auto headersJson = nlohmann::json::parse(headers);
                    for (const auto& item : headersJson.items()) {
                        config.httpConfig.headers[item.key()] = item.value().get<std::string>();
                    }
                } catch (const std::exception& e) {
                    response = createErrorResponse("Invalid headers format: " + std::string(e.what()), 400);
                    return;
                }
            }

        } else if (method == "websocket") {
            config.method = AlarmMethod::WEBSOCKET;
            config.websocketConfig = WebSocketAlarmConfig(url);

            int reconnectInterval = parseJsonInt(request, "reconnect_interval_ms", 5000);
            if (reconnectInterval < 1000 || reconnectInterval > 60000) {
                reconnectInterval = 5000;
            }
            config.websocketConfig.reconnect_interval_ms = reconnectInterval;

        } else if (method == "mqtt") {
            config.method = AlarmMethod::MQTT;
            
            std::string broker = parseJsonField(request, "broker");
            std::string topic = parseJsonField(request, "topic");
            
            if (broker.empty()) {
                response = createErrorResponse("broker is required for MQTT method", 400);
                return;
            }
            
            if (topic.empty()) {
                response = createErrorResponse("topic is required for MQTT method", 400);
                return;
            }

            config.mqttConfig = MqttAlarmConfig(broker, topic);
            
            int port = parseJsonInt(request, "port", 1883);
            if (port < 1 || port > 65535) {
                port = 1883;
            }
            config.mqttConfig.port = port;

            config.mqttConfig.username = parseJsonField(request, "username");
            config.mqttConfig.password = parseJsonField(request, "password");
            
            int qos = parseJsonInt(request, "qos", 0);
            if (qos < 0 || qos > 2) {
                qos = 0;
            }
            config.mqttConfig.qos = qos;

            int keepAlive = parseJsonInt(request, "keep_alive_seconds", 60);
            if (keepAlive < 10 || keepAlive > 300) {
                keepAlive = 60;
            }
            config.mqttConfig.keep_alive_seconds = keepAlive;
        }

        config.enabled = true;
        config.priority = parseJsonInt(request, "priority", 1);
        if (config.priority < 1 || config.priority > 5) {
            config.priority = 1;
        }

        // Get AlarmTrigger from TaskManager (assuming it's accessible)
        // For now, we'll create a static AlarmTrigger instance
        // In a real implementation, this should be managed by TaskManager
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        if (!alarmTrigger.addAlarmConfig(config)) {
            response = createErrorResponse("Failed to add alarm configuration", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Alarm configuration added successfully\","
             << "\"config_id\":\"" << configId << "\","
             << "\"method\":\"" << method << "\","
             << "\"created_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str(), 201);
        logInfo("Added alarm configuration: " + configId + " (" + method + ")");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to add alarm config: " + std::string(e.what()), 500);
    }
}

void AlertController::handleGetAlarmConfigs(const std::string& request, std::string& response) {
    try {
        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        auto configs = alarmTrigger.getAllConfigs();

        std::ostringstream json;
        json << "{"
             << "\"configs\":[";

        for (size_t i = 0; i < configs.size(); ++i) {
            if (i > 0) json << ",";
            json << serializeAlarmConfig(configs[i]);
        }

        json << "],"
             << "\"total\":" << configs.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved " + std::to_string(configs.size()) + " alarm configurations");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm configs: " + std::string(e.what()), 500);
    }
}

void AlertController::handlePostTestAlarm(const std::string& request, std::string& response) {
    try {
        std::string eventType = parseJsonField(request, "event_type");
        std::string cameraId = parseJsonField(request, "camera_id");

        if (eventType.empty()) {
            response = createErrorResponse("event_type is required", 400);
            return;
        }

        if (cameraId.empty()) {
            cameraId = "test_camera";
        }

        static AlarmTrigger alarmTrigger;
        static bool initialized = false;
        if (!initialized) {
            alarmTrigger.initialize();
            initialized = true;
        }

        // Trigger test alarm
        alarmTrigger.triggerTestAlarm(eventType, cameraId);

        std::ostringstream json;
        json << "{"
             << "\"status\":\"test_alarm_triggered\","
             << "\"event_type\":\"" << eventType << "\","
             << "\"camera_id\":\"" << cameraId << "\","
             << "\"test_mode\":true,"
             << "\"triggered_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());

        logInfo("Test alarm triggered: " + eventType + " for camera: " + cameraId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to trigger test alarm: " + std::string(e.what()), 500);
    }
}

std::string AlertController::serializeAlarmConfig(const struct AlarmConfig& config) {
    std::ostringstream json;
    json << "{"
         << "\"id\":\"" << config.id << "\","
         << "\"enabled\":" << (config.enabled ? "true" : "false") << ","
         << "\"priority\":" << config.priority << ",";

    switch (config.method) {
        case AlarmMethod::HTTP_POST:
            json << "\"method\":\"http\","
                 << "\"url\":\"" << config.httpConfig.url << "\","
                 << "\"timeout_ms\":" << config.httpConfig.timeout_ms;
            break;
        case AlarmMethod::WEBSOCKET:
            json << "\"method\":\"websocket\","
                 << "\"url\":\"" << config.websocketConfig.url << "\","
                 << "\"reconnect_interval_ms\":" << config.websocketConfig.reconnect_interval_ms;
            break;
        case AlarmMethod::MQTT:
            json << "\"method\":\"mqtt\","
                 << "\"broker\":\"" << config.mqttConfig.broker << "\","
                 << "\"topic\":\"" << config.mqttConfig.topic << "\","
                 << "\"port\":" << config.mqttConfig.port << ","
                 << "\"qos\":" << config.mqttConfig.qos;
            break;
    }

    json << "}";
    return json.str();
}
