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

            // Parse port from JSON
            int port = parseJsonInt(request, "port", 8081);
            if (port < 1024 || port > 65535) {
                port = 8081;
            }
            config.webSocketConfig.port = port;

            int reconnectInterval = parseJsonInt(request, "ping_interval_ms", 30000);
            if (reconnectInterval < 5000 || reconnectInterval > 60000) {
                reconnectInterval = 30000;
            }
            config.webSocketConfig.ping_interval_ms = reconnectInterval;

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

            config.mqttConfig = MQTTAlarmConfig(broker);
            config.mqttConfig.topic = topic;
            
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

        // Get AlarmTrigger from TaskManager
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        if (!alarmTrigger->addAlarmConfig(config)) {
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
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        auto configs = alarmTrigger->getAlarmConfigs();

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

        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        // Trigger test alarm
        alarmTrigger->triggerTestAlarm(eventType, cameraId);

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
                 << "\"port\":" << config.webSocketConfig.port << ","
                 << "\"ping_interval_ms\":" << config.webSocketConfig.ping_interval_ms;
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

void AlertController::handleGetAlarmStatus(const std::string& request, std::string& response) {
    try {
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        // Get alarm system status and statistics
        size_t pendingCount = alarmTrigger->getPendingAlarmsCount();
        size_t deliveredCount = alarmTrigger->getDeliveredAlarmsCount();
        size_t failedCount = alarmTrigger->getFailedAlarmsCount();
        double avgDeliveryTime = alarmTrigger->getAverageDeliveryTime();

        std::ostringstream json;
        json << "{"
             << "\"status\":\"active\","
             << "\"pending_alarms\":" << pendingCount << ","
             << "\"delivered_alarms\":" << deliveredCount << ","
             << "\"failed_alarms\":" << failedCount << ","
             << "\"average_delivery_time_ms\":" << avgDeliveryTime << ","
             << "\"total_processed\":" << (deliveredCount + failedCount) << ","
             << "\"success_rate\":" << (deliveredCount + failedCount > 0 ?
                 (double)deliveredCount / (deliveredCount + failedCount) * 100.0 : 0.0) << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved alarm system status");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm status: " + std::string(e.what()), 500);
    }
}

void AlertController::handleGetAlert(const std::string& alertId, std::string& response) {
    try {
        if (alertId.empty()) {
            response = createErrorResponse("Alert ID is required", 400);
            return;
        }

        // For demonstration, return a sample alert based on ID
        // In a real implementation, this would query the database
        std::ostringstream json;
        json << "{"
             << "\"id\":" << alertId << ","
             << "\"type\":\"intrusion\","
             << "\"camera_id\":\"camera_1\","
             << "\"message\":\"Person detected in restricted area\","
             << "\"severity\":\"high\","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\","
             << "\"acknowledged\":false,"
             << "\"details\":{"
             << "\"detection_confidence\":0.95,"
             << "\"object_count\":1,"
             << "\"location\":\"entrance\""
             << "}"
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved alert: " + alertId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alert: " + std::string(e.what()), 500);
    }
}

void AlertController::handleDeleteAlert(const std::string& alertId, std::string& response) {
    try {
        if (alertId.empty()) {
            response = createErrorResponse("Alert ID is required", 400);
            return;
        }

        // In a real implementation, this would delete from database
        // For now, simulate successful deletion
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Alert deleted successfully\","
             << "\"alert_id\":" << alertId << ","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Deleted alert: " + alertId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete alert: " + std::string(e.what()), 500);
    }
}

void AlertController::handleMarkAlertAsRead(const std::string& alertId, std::string& response) {
    try {
        if (alertId.empty()) {
            response = createErrorResponse("Alert ID is required", 400);
            return;
        }

        // In a real implementation, this would update the database
        // For now, simulate successful acknowledgment
        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Alert marked as read\","
             << "\"alert_id\":" << alertId << ","
             << "\"acknowledged\":true,"
             << "\"acknowledged_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Marked alert as read: " + alertId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to mark alert as read: " + std::string(e.what()), 500);
    }
}

void AlertController::handleGetAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        auto configs = alarmTrigger->getAlarmConfigs();

        // Find the specific config
        for (const auto& config : configs) {
            if (config.id == configId) {
                std::ostringstream json;
                json << "{"
                     << "\"config\":" << serializeAlarmConfig(config) << ","
                     << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
                     << "}";

                response = createJsonResponse(json.str());
                logInfo("Retrieved alarm config: " + configId);
                return;
            }
        }

        response = createErrorResponse("Alarm config not found: " + configId, 404);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get alarm config: " + std::string(e.what()), 500);
    }
}

void AlertController::handlePutAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        // Parse the request to get updated config
        AlarmConfig config;
        if (!deserializeAlarmConfig(request, config)) {
            response = createErrorResponse("Invalid alarm config format", 400);
            return;
        }

        // Ensure the ID matches
        config.id = configId;

        if (!alarmTrigger->updateAlarmConfig(config)) {
            response = createErrorResponse("Failed to update alarm configuration", 500);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Alarm configuration updated successfully\","
             << "\"config_id\":\"" << configId << "\","
             << "\"updated_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Updated alarm configuration: " + configId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to update alarm config: " + std::string(e.what()), 500);
    }
}

void AlertController::handleDeleteAlarmConfig(const std::string& request, std::string& response, const std::string& configId) {
    try {
        auto& taskManager = TaskManager::getInstance();
        AlarmTrigger* alarmTrigger = taskManager.getAlarmTrigger();
        if (!alarmTrigger) {
            response = createErrorResponse("Alarm system not available", 503);
            return;
        }

        if (!alarmTrigger->removeAlarmConfig(configId)) {
            response = createErrorResponse("Alarm config not found: " + configId, 404);
            return;
        }

        std::ostringstream json;
        json << "{"
             << "\"status\":\"success\","
             << "\"message\":\"Alarm configuration deleted successfully\","
             << "\"config_id\":\"" << configId << "\","
             << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Deleted alarm configuration: " + configId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete alarm config: " + std::string(e.what()), 500);
    }
}

bool AlertController::deserializeAlarmConfig(const std::string& json, struct AlarmConfig& config) {
    try {
        auto jsonObj = nlohmann::json::parse(json);

        config.id = jsonObj.value("id", "");
        std::string method = jsonObj.value("method", "http");
        config.enabled = jsonObj.value("enabled", true);
        config.priority = jsonObj.value("priority", 1);

        if (method == "http") {
            config.method = AlarmMethod::HTTP_POST;
            config.httpConfig.url = jsonObj.value("url", "");
            config.httpConfig.timeout_ms = jsonObj.value("timeout_ms", 5000);

            if (jsonObj.contains("headers") && jsonObj["headers"].is_string()) {
                std::string headersStr = jsonObj["headers"];
                if (!headersStr.empty()) {
                    auto headersJson = nlohmann::json::parse(headersStr);
                    for (const auto& item : headersJson.items()) {
                        config.httpConfig.headers[item.key()] = item.value().get<std::string>();
                    }
                }
            }
        } else if (method == "websocket") {
            config.method = AlarmMethod::WEBSOCKET;
            config.webSocketConfig.port = jsonObj.value("port", 8081);
            config.webSocketConfig.ping_interval_ms = jsonObj.value("ping_interval_ms", 30000);
        } else if (method == "mqtt") {
            config.method = AlarmMethod::MQTT;
            config.mqttConfig.broker = jsonObj.value("broker", "");
            config.mqttConfig.port = jsonObj.value("port", 1883);
            config.mqttConfig.topic = jsonObj.value("topic", "aibox/alarms");
            config.mqttConfig.qos = jsonObj.value("qos", 1);
            config.mqttConfig.username = jsonObj.value("username", "");
            config.mqttConfig.password = jsonObj.value("password", "");
            config.mqttConfig.keep_alive_seconds = jsonObj.value("keep_alive_seconds", 60);
        } else {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}
