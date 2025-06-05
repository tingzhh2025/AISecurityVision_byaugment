#include "SwaggerUI.h"
#include "../third_party/nlohmann/json.hpp"
#include <sstream>
#include <algorithm>

using namespace AISecurityVision;

SwaggerUI::SwaggerUI() 
    : m_authEnabled(true), m_authScheme("bearer"), 
      m_authDescription("JWT认证令牌"),
      m_startTime(std::chrono::system_clock::now()) {
    
    loadBuiltinSchemas();
    loadBuiltinEndpoints();
}

bool SwaggerUI::initialize(const std::string& title, const std::string& version,
                          const std::string& description, const std::string& baseUrl) {
    m_title = title;
    m_version = version;
    m_description = description;
    m_baseUrl = baseUrl;
    
    // 添加默认服务器
    addServer(baseUrl, "开发环境");
    
    LOG_INFO() << "[SwaggerUI] Initialized: " << title << " v" << version;
    return true;
}

void SwaggerUI::addEndpoint(const EndpointInfo& endpoint) {
    m_endpoints.push_back(endpoint);
    LOG_DEBUG() << "[SwaggerUI] Added endpoint: " << endpoint.method << " " << endpoint.path;
}

void SwaggerUI::addSchema(const SchemaDefinition& schema) {
    m_schemas.push_back(schema);
    LOG_DEBUG() << "[SwaggerUI] Added schema: " << schema.name;
}

std::string SwaggerUI::generateOpenAPISpec() const {
    nlohmann::json spec;
    
    // OpenAPI版本和基本信息
    spec["openapi"] = "3.0.3";
    spec["info"]["title"] = m_title;
    spec["info"]["version"] = m_version;
    spec["info"]["description"] = m_description;
    
    // 联系信息
    if (!m_contactName.empty()) {
        spec["info"]["contact"]["name"] = m_contactName;
        if (!m_contactEmail.empty()) {
            spec["info"]["contact"]["email"] = m_contactEmail;
        }
        if (!m_contactUrl.empty()) {
            spec["info"]["contact"]["url"] = m_contactUrl;
        }
    }
    
    // 许可证信息
    if (!m_licenseName.empty()) {
        spec["info"]["license"]["name"] = m_licenseName;
        if (!m_licenseUrl.empty()) {
            spec["info"]["license"]["url"] = m_licenseUrl;
        }
    }
    
    // 服务器信息
    spec["servers"] = nlohmann::json::array();
    for (const auto& [url, description] : m_servers) {
        nlohmann::json server;
        server["url"] = url;
        server["description"] = description;
        spec["servers"].push_back(server);
    }
    
    // 标签
    if (!m_tags.empty()) {
        spec["tags"] = nlohmann::json::array();
        for (const auto& [name, description] : m_tags) {
            nlohmann::json tag;
            tag["name"] = name;
            tag["description"] = description;
            spec["tags"].push_back(tag);
        }
    }
    
    // 路径
    spec["paths"] = nlohmann::json::object();
    for (const auto& endpoint : m_endpoints) {
        std::string method = endpoint.method;
        std::transform(method.begin(), method.end(), method.begin(), ::tolower);
        
        if (spec["paths"][endpoint.path].is_null()) {
            spec["paths"][endpoint.path] = nlohmann::json::object();
        }
        
        nlohmann::json operation;
        operation["summary"] = endpoint.summary;
        operation["description"] = endpoint.description;
        operation["tags"] = nlohmann::json::array();
        operation["tags"].push_back(endpoint.tag);
        
        // 认证要求
        if (endpoint.requiresAuth && m_authEnabled) {
            operation["security"] = nlohmann::json::array();
            nlohmann::json security;
            security["BearerAuth"] = nlohmann::json::array();
            operation["security"].push_back(security);
        }
        
        // 响应
        operation["responses"]["200"] = nlohmann::json::object();
        operation["responses"]["200"]["description"] = "成功响应";
        operation["responses"]["200"]["content"]["application/json"]["schema"]["$ref"] = "#/components/schemas/Success";
        
        if (endpoint.requiresAuth) {
            operation["responses"]["401"] = nlohmann::json::object();
            operation["responses"]["401"]["description"] = "认证失败";
            operation["responses"]["401"]["content"]["application/json"]["schema"]["$ref"] = "#/components/schemas/Error";
        }
        
        spec["paths"][endpoint.path][method] = operation;
    }
    
    // 组件
    spec["components"]["schemas"] = nlohmann::json::object();
    
    // 内置模式
    spec["components"]["schemas"]["Success"] = {
        {"type", "object"},
        {"properties", {
            {"success", {{"type", "boolean"}, {"example", true}}},
            {"message", {{"type", "string"}, {"example", "操作成功"}}},
            {"data", {{"type", "object"}}}
        }},
        {"required", nlohmann::json::array({"success", "message"})}
    };
    
    spec["components"]["schemas"]["Error"] = {
        {"type", "object"},
        {"properties", {
            {"success", {{"type", "boolean"}, {"example", false}}},
            {"message", {{"type", "string"}, {"example", "错误描述"}}},
            {"error_code", {{"type", "integer"}, {"example", 400}}}
        }},
        {"required", nlohmann::json::array({"success", "message"})}
    };
    
    // 自定义模式
    for (const auto& schema : m_schemas) {
        nlohmann::json schemaObj;
        schemaObj["type"] = schema.type;
        schemaObj["description"] = schema.description;
        
        if (!schema.properties.empty()) {
            try {
                schemaObj["properties"] = nlohmann::json::parse(schema.properties);
            } catch (const std::exception& e) {
                LOG_WARN() << "[SwaggerUI] Failed to parse schema properties for " << schema.name << ": " << e.what();
            }
        }
        
        if (!schema.required.empty()) {
            schemaObj["required"] = schema.required;
        }
        
        spec["components"]["schemas"][schema.name] = schemaObj;
    }
    
    // 安全方案
    if (m_authEnabled) {
        spec["components"]["securitySchemes"]["BearerAuth"] = {
            {"type", "http"},
            {"scheme", m_authScheme},
            {"bearerFormat", "JWT"},
            {"description", m_authDescription}
        };
    }
    
    return spec.dump(2);
}

std::string SwaggerUI::generateSwaggerUIHtml() const {
    std::ostringstream html;
    
    html << R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << m_title << R"( - API Documentation</title>
    <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui.css" />
    <style>
        html { box-sizing: border-box; overflow: -moz-scrollbars-vertical; overflow-y: scroll; }
        *, *:before, *:after { box-sizing: inherit; }
        body { margin:0; background: #fafafa; }
        .swagger-ui .topbar { background-color: #667eea; }
        .swagger-ui .topbar .download-url-wrapper { display: none; }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui-bundle.js"></script>
    <script src="https://unpkg.com/swagger-ui-dist@4.15.5/swagger-ui-standalone-preset.js"></script>
    <script>
        window.onload = function() {
            const ui = SwaggerUIBundle({
                url: '/api/openapi.json',
                dom_id: '#swagger-ui',
                deepLinking: true,
                presets: [
                    SwaggerUIBundle.presets.apis,
                    SwaggerUIStandalonePreset
                ],
                plugins: [
                    SwaggerUIBundle.plugins.DownloadUrl
                ],
                layout: "StandaloneLayout",
                defaultModelsExpandDepth: 1,
                defaultModelExpandDepth: 1,
                docExpansion: "list",
                filter: true,
                showRequestHeaders: true,
                showCommonExtensions: true,
                tryItOutEnabled: true
            });
        };
    </script>
</body>
</html>)";
    
    return html.str();
}

bool SwaggerUI::handleRequest(const std::string& path, const std::string& method, std::string& response) {
    if (method != "GET") {
        return false;
    }
    
    if (path == "/api/docs" || path == "/api/docs/") {
        m_uiRequests++;
        response = generateSwaggerUIHtml();
        return true;
    } else if (path == "/api/openapi.json") {
        m_specRequests++;
        response = generateOpenAPISpec();
        return true;
    }
    
    return false;
}

void SwaggerUI::addServer(const std::string& url, const std::string& description) {
    m_servers.emplace_back(url, description);
}

void SwaggerUI::setContact(const std::string& name, const std::string& email, const std::string& url) {
    m_contactName = name;
    m_contactEmail = email;
    m_contactUrl = url;
}

void SwaggerUI::setLicense(const std::string& name, const std::string& url) {
    m_licenseName = name;
    m_licenseUrl = url;
}

void SwaggerUI::addTag(const std::string& name, const std::string& description) {
    m_tags.emplace_back(name, description);
}

void SwaggerUI::setAuthentication(bool enabled, const std::string& scheme, const std::string& description) {
    m_authEnabled = enabled;
    m_authScheme = scheme;
    m_authDescription = description;
}

std::string SwaggerUI::getApiStats() const {
    nlohmann::json stats;
    
    stats["spec_requests"] = m_specRequests.load();
    stats["ui_requests"] = m_uiRequests.load();
    stats["total_endpoints"] = m_endpoints.size();
    stats["total_schemas"] = m_schemas.size();
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
    stats["uptime_seconds"] = uptime;
    
    return stats.dump();
}

void SwaggerUI::loadBuiltinSchemas() {
    // 添加一些内置的模式定义
    SchemaDefinition systemStatus;
    systemStatus.name = "SystemStatus";
    systemStatus.type = "object";
    systemStatus.description = "系统状态信息";
    systemStatus.properties = R"({
        "status": {"type": "string", "enum": ["running", "stopped", "error"]},
        "uptime": {"type": "integer", "description": "System uptime in seconds"},
        "version": {"type": "string"},
        "cameras_active": {"type": "integer"},
        "ai_backend": {"type": "string", "enum": ["RKNN", "OpenCV", "TensorRT"]}
    })";
    systemStatus.required = {"status", "uptime", "version"};
    addSchema(systemStatus);
    
    SchemaDefinition camera;
    camera.name = "Camera";
    camera.type = "object";
    camera.description = "摄像头信息";
    camera.properties = R"({
        "id": {"type": "string"},
        "name": {"type": "string"},
        "rtsp_url": {"type": "string"},
        "enabled": {"type": "boolean"},
        "detection_enabled": {"type": "boolean"},
        "mjpeg_port": {"type": "integer"},
        "status": {"type": "string", "enum": ["connected", "disconnected", "error"]}
    })";
    camera.required = {"id", "name", "rtsp_url"};
    addSchema(camera);
}

void SwaggerUI::loadBuiltinEndpoints() {
    // 添加一些内置的端点定义
    EndpointInfo systemStatus;
    systemStatus.method = "GET";
    systemStatus.path = "/api/system/status";
    systemStatus.summary = "获取系统状态";
    systemStatus.description = "获取系统运行状态、版本信息和基本统计";
    systemStatus.tag = "System Management";
    systemStatus.requiresAuth = false;
    addEndpoint(systemStatus);
    
    EndpointInfo login;
    login.method = "POST";
    login.path = "/api/auth/login";
    login.summary = "用户登录";
    login.description = "使用用户名和密码进行登录，返回JWT令牌";
    login.tag = "Authentication";
    login.requiresAuth = false;
    addEndpoint(login);
    
    EndpointInfo cameras;
    cameras.method = "GET";
    cameras.path = "/api/cameras";
    cameras.summary = "获取摄像头列表";
    cameras.description = "获取所有已配置的摄像头信息";
    cameras.tag = "Camera Management";
    cameras.requiresAuth = true;
    addEndpoint(cameras);
}

std::string SwaggerUI::escapeJson(const std::string& input) const {
    std::string result;
    result.reserve(input.length() * 1.2);
    
    for (char c : input) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

std::string SwaggerUI::getMethodColor(const std::string& method) const {
    if (method == "GET") return "#61affe";
    if (method == "POST") return "#49cc90";
    if (method == "PUT") return "#fca130";
    if (method == "DELETE") return "#f93e3e";
    if (method == "PATCH") return "#50e3c2";
    return "#9012fe";
}
