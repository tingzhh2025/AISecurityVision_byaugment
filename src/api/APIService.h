#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>

/**
 * @brief HTTP API Service for system control and monitoring
 * 
 * Provides RESTful endpoints for:
 * - Video source management
 * - System status and monitoring
 * - AI model configuration
 * - Behavior rule management
 * - Face/plate database operations
 */
class APIService {
public:
    explicit APIService(int port = 8080);
    ~APIService();
    
    // Service control
    bool start();
    void stop();
    bool isRunning() const;
    
    // Configuration
    void setPort(int port);
    int getPort() const;

private:
    // HTTP server implementation
    void serverThread();
    void setupRoutes();
    
    // Route handlers
    void handleGetStatus(const std::string& request, std::string& response);
    void handlePostVideoSource(const std::string& request, std::string& response);
    void handleDeleteVideoSource(const std::string& request, std::string& response);
    void handleGetVideoSources(const std::string& request, std::string& response);
    
    // Utility methods
    std::string createJsonResponse(const std::string& data, int statusCode = 200);
    std::string createErrorResponse(const std::string& error, int statusCode = 400);
    
    // Member variables
    int m_port;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    
    // HTTP server implementation (placeholder)
    void* m_httpServer; // Will be replaced with actual HTTP library
};
