#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "../../auth/AuthService.h"
#include "../../database/DatabaseManager.h"
#include "../../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief Authentication Controller for user management API endpoints (NEW - Phase 2)
 * 
 * This controller handles all authentication-related HTTP requests including
 * user login, logout, token validation, and user management operations.
 */
class AuthController {
public:
    /**
     * @brief Constructor
     */
    AuthController();

    /**
     * @brief Destructor
     */
    ~AuthController() = default;

    /**
     * @brief Initialize controller with dependencies
     * @param dbManager Database manager instance
     * @param jwtSecretKey JWT secret key (optional)
     * @return True if initialization successful
     */
    bool initialize(std::shared_ptr<DatabaseManager> dbManager, const std::string& jwtSecretKey = "");

    /**
     * @brief Handle user login request
     * @param requestBody JSON request body containing username and password
     * @param response Output response string
     */
    void handleLogin(const std::string& requestBody, std::string& response);

    /**
     * @brief Handle user logout request
     * @param requestBody JSON request body containing token
     * @param response Output response string
     */
    void handleLogout(const std::string& requestBody, std::string& response);

    /**
     * @brief Handle get current user request
     * @param authHeader Authorization header containing Bearer token
     * @param response Output response string
     */
    void handleGetCurrentUser(const std::string& authHeader, std::string& response);

    /**
     * @brief Handle token validation request
     * @param requestBody JSON request body containing token
     * @param response Output response string
     */
    void handleValidateToken(const std::string& requestBody, std::string& response);

    /**
     * @brief Handle token refresh request
     * @param requestBody JSON request body containing token
     * @param response Output response string
     */
    void handleRefreshToken(const std::string& requestBody, std::string& response);

    /**
     * @brief Handle user registration request (admin only)
     * @param requestBody JSON request body containing user data
     * @param authHeader Authorization header containing admin token
     * @param response Output response string
     */
    void handleRegisterUser(const std::string& requestBody, const std::string& authHeader, std::string& response);

    /**
     * @brief Handle change password request
     * @param requestBody JSON request body containing current and new passwords
     * @param authHeader Authorization header containing user token
     * @param response Output response string
     */
    void handleChangePassword(const std::string& requestBody, const std::string& authHeader, std::string& response);

    /**
     * @brief Handle get all users request (admin only)
     * @param authHeader Authorization header containing admin token
     * @param response Output response string
     */
    void handleGetAllUsers(const std::string& authHeader, std::string& response);

    /**
     * @brief Handle update user role request (admin only)
     * @param requestBody JSON request body containing user ID and new role
     * @param authHeader Authorization header containing admin token
     * @param response Output response string
     */
    void handleUpdateUserRole(const std::string& requestBody, const std::string& authHeader, std::string& response);

    /**
     * @brief Handle enable/disable user request (admin only)
     * @param requestBody JSON request body containing user ID and enabled status
     * @param authHeader Authorization header containing admin token
     * @param response Output response string
     */
    void handleSetUserEnabled(const std::string& requestBody, const std::string& authHeader, std::string& response);

    /**
     * @brief Middleware function to validate JWT token from Authorization header
     * @param authHeader Authorization header value
     * @return AuthResult with validation result
     */
    AuthService::AuthResult validateAuthHeader(const std::string& authHeader);

    /**
     * @brief Extract Bearer token from Authorization header
     * @param authHeader Authorization header value
     * @return Token string (empty if not found)
     */
    std::string extractBearerToken(const std::string& authHeader);

private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<AuthService> m_authService;
    bool m_initialized = false;

    /**
     * @brief Create HTTP response with proper headers
     * @param statusCode HTTP status code
     * @param message Response message
     * @param data Optional data object
     * @return JSON response string
     */
    std::string createResponse(int statusCode, const std::string& message, const nlohmann::json& data = nlohmann::json::object());

    /**
     * @brief Create error response
     * @param statusCode HTTP status code
     * @param message Error message
     * @return JSON error response string
     */
    std::string createErrorResponse(int statusCode, const std::string& message);

    /**
     * @brief Create success response
     * @param message Success message
     * @param data Optional data object
     * @return JSON success response string
     */
    std::string createSuccessResponse(const std::string& message, const nlohmann::json& data = nlohmann::json::object());

    /**
     * @brief Validate JSON request body
     * @param requestBody JSON string to validate
     * @param requiredFields List of required field names
     * @return Parsed JSON object (empty if validation failed)
     */
    nlohmann::json validateRequestBody(const std::string& requestBody, const std::vector<std::string>& requiredFields);

    /**
     * @brief Convert UserRecord to JSON object (without sensitive data)
     * @param user User record to convert
     * @return JSON object
     */
    nlohmann::json userToJson(const UserRecord& user);

    /**
     * @brief Convert AuthResult to JSON object
     * @param authResult Authentication result to convert
     * @return JSON object
     */
    nlohmann::json authResultToJson(const AuthService::AuthResult& authResult);
};

} // namespace AISecurityVision
