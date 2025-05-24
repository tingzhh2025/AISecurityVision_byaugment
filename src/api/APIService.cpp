#include "APIService.h"
#include "../core/TaskManager.h"
#include <iostream>
#include <sstream>
#include <chrono>

APIService::APIService(int port) : m_port(port), m_httpServer(nullptr) {
    std::cout << "[APIService] Initializing API service on port " << port << std::endl;
}

APIService::~APIService() {
    stop();
}

bool APIService::start() {
    if (m_running.load()) {
        std::cout << "[APIService] Service already running" << std::endl;
        return true;
    }
    
    try {
        m_running.store(true);
        m_serverThread = std::thread(&APIService::serverThread, this);
        
        // Give the server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "[APIService] API service started on port " << m_port << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[APIService] Failed to start: " << e.what() << std::endl;
        m_running.store(false);
        return false;
    }
}

void APIService::stop() {
    if (!m_running.load()) {
        return;
    }
    
    std::cout << "[APIService] Stopping API service..." << std::endl;
    
    m_running.store(false);
    
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    std::cout << "[APIService] API service stopped" << std::endl;
}

bool APIService::isRunning() const {
    return m_running.load();
}

void APIService::setPort(int port) {
    m_port = port;
}

int APIService::getPort() const {
    return m_port;
}

void APIService::serverThread() {
    std::cout << "[APIService] Server thread started" << std::endl;
    
    // TODO: Implement actual HTTP server using httplib
    // For now, this is a placeholder that simulates a running server
    
    while (m_running.load()) {
        // Simulate server processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // In a real implementation, this would:
        // 1. Listen for HTTP requests
        // 2. Parse request paths and methods
        // 3. Route to appropriate handlers
        // 4. Send responses back to clients
    }
    
    std::cout << "[APIService] Server thread stopped" << std::endl;
}

void APIService::setupRoutes() {
    // TODO: Setup HTTP routes
    // GET /api/system/status
    // POST /api/source/add
    // DELETE /api/source/{id}
    // GET /api/source/list
    // POST /api/source/discover
    // GET /api/faces
    // POST /api/faces/add
    // DELETE /api/faces/{id}
    // POST /api/rules
    // GET /api/rules
    // PUT /api/rules/{id}
    // DELETE /api/rules/{id}
}

void APIService::handleGetStatus(const std::string& request, std::string& response) {
    TaskManager& taskManager = TaskManager::getInstance();
    
    std::ostringstream json;
    json << "{"
         << "\"status\":\"running\","
         << "\"active_pipelines\":" << taskManager.getActivePipelineCount() << ","
         << "\"cpu_usage\":" << taskManager.getCpuUsage() << ","
         << "\"gpu_memory\":\"" << taskManager.getGpuMemoryUsage() << "\","
         << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
         << "}";
    
    response = createJsonResponse(json.str());
}

void APIService::handlePostVideoSource(const std::string& request, std::string& response) {
    // TODO: Parse JSON request to extract video source parameters
    // For now, return a placeholder response
    
    response = createJsonResponse("{\"message\":\"Video source endpoint not implemented yet\"}");
}

void APIService::handleDeleteVideoSource(const std::string& request, std::string& response) {
    // TODO: Extract source ID from URL path and remove from TaskManager
    
    response = createJsonResponse("{\"message\":\"Delete video source endpoint not implemented yet\"}");
}

void APIService::handleGetVideoSources(const std::string& request, std::string& response) {
    TaskManager& taskManager = TaskManager::getInstance();
    auto activePipelines = taskManager.getActivePipelines();
    
    std::ostringstream json;
    json << "{\"sources\":[";
    
    for (size_t i = 0; i < activePipelines.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":\"" << activePipelines[i] << "\",\"status\":\"active\"}";
    }
    
    json << "]}";
    
    response = createJsonResponse(json.str());
}

std::string APIService::createJsonResponse(const std::string& data, int statusCode) {
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << data.length() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "\r\n"
             << data;
    return response.str();
}

std::string APIService::createErrorResponse(const std::string& error, int statusCode) {
    std::ostringstream json;
    json << "{\"error\":\"" << error << "\"}";
    return createJsonResponse(json.str(), statusCode);
}
