#include "BaseController.h"
#include "../../core/TaskManager.h"
#include "../../onvif/ONVIFDiscovery.h"
#include "../../network/NetworkManager.h"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <regex>
#include <nlohmann/json.hpp>

using namespace AISecurityVision;

void BaseController::initialize(TaskManager* taskManager, 
                               ONVIFManager* onvifManager, 
                               AISecurityVision::NetworkManager* networkManager) {
    m_taskManager = taskManager;
    m_onvifManager = onvifManager;
    m_networkManager = networkManager;
}

std::string BaseController::createJsonResponse(const std::string& data, int statusCode) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " ";
    
    switch (statusCode) {
        case 200: response << "OK"; break;
        case 201: response << "Created"; break;
        case 400: response << "Bad Request"; break;
        case 404: response << "Not Found"; break;
        case 500: response << "Internal Server Error"; break;
        default: response << "Unknown"; break;
    }
    
    response << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "Content-Length: " << data.length() << "\r\n";
    response << "\r\n";
    response << data;
    
    return response.str();
}

std::string BaseController::createErrorResponse(const std::string& error, int statusCode) {
    std::ostringstream json;
    json << "{"
         << "\"error\":\"" << error << "\","
         << "\"status\":" << statusCode << ","
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
         << "}";
    
    return createJsonResponse(json.str(), statusCode);
}

std::string BaseController::createSuccessResponse(const std::string& message) {
    std::ostringstream json;
    json << "{"
         << "\"message\":\"" << message << "\","
         << "\"status\":200,"
         << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
         << "}";
    
    return createJsonResponse(json.str(), 200);
}

std::string BaseController::parseJsonField(const std::string& json, const std::string& field) {
    try {
        auto j = nlohmann::json::parse(json);
        if (j.contains(field) && j[field].is_string()) {
            return j[field].get<std::string>();
        }
    } catch (const std::exception& e) {
        logError("Failed to parse JSON field '" + field + "': " + e.what());
    }
    return "";
}

int BaseController::parseJsonInt(const std::string& json, const std::string& field, int defaultValue) {
    try {
        auto j = nlohmann::json::parse(json);
        if (j.contains(field) && j[field].is_number_integer()) {
            return j[field].get<int>();
        }
    } catch (const std::exception& e) {
        logError("Failed to parse JSON int field '" + field + "': " + e.what());
    }
    return defaultValue;
}

float BaseController::parseJsonFloat(const std::string& json, const std::string& field, float defaultValue) {
    try {
        auto j = nlohmann::json::parse(json);
        if (j.contains(field) && j[field].is_number()) {
            return j[field].get<float>();
        }
    } catch (const std::exception& e) {
        logError("Failed to parse JSON float field '" + field + "': " + e.what());
    }
    return defaultValue;
}

bool BaseController::parseJsonBool(const std::string& json, const std::string& field, bool defaultValue) {
    try {
        auto j = nlohmann::json::parse(json);
        if (j.contains(field) && j[field].is_boolean()) {
            return j[field].get<bool>();
        }
    } catch (const std::exception& e) {
        logError("Failed to parse JSON bool field '" + field + "': " + e.what());
    }
    return defaultValue;
}

std::string BaseController::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return oss.str();
}

void BaseController::setCorsHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

std::string BaseController::stripHttpHeaders(const std::string& response) {
    size_t contentStart = response.find("\r\n\r\n");
    if (contentStart != std::string::npos) {
        return response.substr(contentStart + 4);
    }
    return response;
}

bool BaseController::isValidCameraId(const std::string& cameraId) {
    if (cameraId.empty() || cameraId.length() > 64) {
        return false;
    }
    
    // Check for valid characters (alphanumeric, underscore, hyphen)
    std::regex validPattern("^[a-zA-Z0-9_-]+$");
    return std::regex_match(cameraId, validPattern);
}

bool BaseController::isValidJson(const std::string& json) {
    try {
        auto parsed = nlohmann::json::parse(json);
        (void)parsed; // Suppress unused variable warning
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void BaseController::logInfo(const std::string& message, const std::string& context) {
    std::string fullContext = "[" + getControllerName() + "]";
    if (!context.empty()) {
        fullContext += " [" + context + "]";
    }
    LOG_INFO() << fullContext << " " << message;
}

void BaseController::logWarn(const std::string& message, const std::string& context) {
    std::string fullContext = "[" + getControllerName() + "]";
    if (!context.empty()) {
        fullContext += " [" + context + "]";
    }
    LOG_WARN() << fullContext << " " << message;
}

void BaseController::logError(const std::string& message, const std::string& context) {
    std::string fullContext = "[" + getControllerName() + "]";
    if (!context.empty()) {
        fullContext += " [" + context + "]";
    }
    LOG_ERROR() << fullContext << " " << message;
}

void BaseController::logDebug(const std::string& message, const std::string& context) {
    std::string fullContext = "[" + getControllerName() + "]";
    if (!context.empty()) {
        fullContext += " [" + context + "]";
    }
    LOG_DEBUG() << fullContext << " " << message;
}
