#include "AuthController.h"
#include <nlohmann/json.hpp>
#include <regex>

namespace AISecurityVision {

AuthController::AuthController() {
    LOG_INFO() << "[AuthController] Authentication controller created";
}

bool AuthController::initialize(std::shared_ptr<DatabaseManager> dbManager, const std::string& jwtSecretKey) {
    try {
        if (!dbManager) {
            LOG_ERROR() << "[AuthController] DatabaseManager cannot be null";
            return false;
        }

        m_dbManager = dbManager;
        m_authService = std::make_unique<AuthService>(dbManager, jwtSecretKey);

        m_initialized = true;
        LOG_INFO() << "[AuthController] Authentication controller initialized successfully";
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Initialization failed: " << e.what();
        return false;
    }
}

void AuthController::handleLogin(const std::string& requestBody, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"username", "password"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Username and password are required");
            return;
        }

        std::string username = requestJson["username"];
        std::string password = requestJson["password"];
        int expirationHours = requestJson.value("expiration_hours", 24);

        // Attempt login
        AuthService::AuthResult authResult = m_authService->login(username, password, expirationHours);

        if (authResult.success) {
            nlohmann::json responseData = authResultToJson(authResult);
            response = createSuccessResponse("Login successful", responseData);
            LOG_INFO() << "[AuthController] User logged in: " << username;
        } else {
            response = createErrorResponse(401, authResult.message);
            LOG_WARN() << "[AuthController] Login failed for user: " << username;
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Login error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleLogout(const std::string& requestBody, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"token"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Token is required");
            return;
        }

        std::string token = requestJson["token"];

        // Attempt logout
        bool success = m_authService->logout(token);

        if (success) {
            response = createSuccessResponse("Logout successful");
            LOG_INFO() << "[AuthController] User logged out successfully";
        } else {
            response = createErrorResponse(400, "Logout failed");
            LOG_WARN() << "[AuthController] Logout failed";
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Logout error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleGetCurrentUser(const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate authorization header
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success) {
            response = createErrorResponse(401, authResult.message);
            return;
        }

        // Get user details
        UserRecord user = m_authService->getCurrentUser(authResult.token);
        if (user.user_id.empty()) {
            response = createErrorResponse(404, "User not found");
            return;
        }

        nlohmann::json userData = userToJson(user);
        response = createSuccessResponse("User information retrieved", userData);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Get current user error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleValidateToken(const std::string& requestBody, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"token"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Token is required");
            return;
        }

        std::string token = requestJson["token"];

        // Validate token
        AuthService::AuthResult authResult = m_authService->validateToken(token);

        if (authResult.success) {
            nlohmann::json responseData = authResultToJson(authResult);
            response = createSuccessResponse("Token is valid", responseData);
        } else {
            response = createErrorResponse(401, authResult.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Token validation error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleRefreshToken(const std::string& requestBody, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"token"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Token is required");
            return;
        }

        std::string token = requestJson["token"];
        int expirationHours = requestJson.value("expiration_hours", 24);

        // Refresh token
        AuthService::AuthResult authResult = m_authService->refreshToken(token, expirationHours);

        if (authResult.success) {
            nlohmann::json responseData = authResultToJson(authResult);
            response = createSuccessResponse("Token refreshed successfully", responseData);
            LOG_INFO() << "[AuthController] Token refreshed for user: " << authResult.username;
        } else {
            response = createErrorResponse(401, authResult.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Token refresh error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

AuthService::AuthResult AuthController::validateAuthHeader(const std::string& authHeader) {
    std::string token = extractBearerToken(authHeader);
    if (token.empty()) {
        return AuthService::AuthResult(false, "Missing or invalid Authorization header");
    }

    return m_authService->validateToken(token);
}

std::string AuthController::extractBearerToken(const std::string& authHeader) {
    // Expected format: "Bearer <token>"
    std::regex bearerRegex(R"(^Bearer\s+(.+)$)", std::regex_constants::icase);
    std::smatch matches;

    if (std::regex_match(authHeader, matches, bearerRegex) && matches.size() == 2) {
        return matches[1].str();
    }

    return "";
}

void AuthController::handleRegisterUser(const std::string& requestBody, const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate admin authorization
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success || authResult.role != "admin") {
            response = createErrorResponse(403, "Admin privileges required");
            return;
        }

        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"username", "password"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Username and password are required");
            return;
        }

        AuthService::UserRegistration registration;
        registration.username = requestJson["username"];
        registration.password = requestJson["password"];
        registration.role = requestJson.value("role", "user");
        registration.enabled = requestJson.value("enabled", true);

        // Register user
        AuthService::AuthResult regResult = m_authService->registerUser(registration);

        if (regResult.success) {
            response = createSuccessResponse(regResult.message);
            LOG_INFO() << "[AuthController] User registered by admin: " << registration.username;
        } else {
            response = createErrorResponse(400, regResult.message);
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] User registration error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleChangePassword(const std::string& requestBody, const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate authorization
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success) {
            response = createErrorResponse(401, authResult.message);
            return;
        }

        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"current_password", "new_password"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. Current and new passwords are required");
            return;
        }

        std::string currentPassword = requestJson["current_password"];
        std::string newPassword = requestJson["new_password"];

        // Change password
        bool success = m_authService->changePassword(authResult.user_id, currentPassword, newPassword);

        if (success) {
            response = createSuccessResponse("Password changed successfully");
            LOG_INFO() << "[AuthController] Password changed for user: " << authResult.username;
        } else {
            response = createErrorResponse(400, "Failed to change password");
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Change password error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleGetAllUsers(const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate admin authorization
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success || authResult.role != "admin") {
            response = createErrorResponse(403, "Admin privileges required");
            return;
        }

        // Get all users
        std::vector<UserRecord> users = m_authService->getAllUsers(authResult.token);

        nlohmann::json usersArray = nlohmann::json::array();
        for (const auto& user : users) {
            usersArray.push_back(userToJson(user));
        }

        response = createSuccessResponse("Users retrieved successfully", usersArray);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Get all users error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleUpdateUserRole(const std::string& requestBody, const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate admin authorization
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success || authResult.role != "admin") {
            response = createErrorResponse(403, "Admin privileges required");
            return;
        }

        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"user_id", "role"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. User ID and role are required");
            return;
        }

        std::string userId = requestJson["user_id"];
        std::string newRole = requestJson["role"];

        // Update user role
        bool success = m_authService->updateUserRole(authResult.token, userId, newRole);

        if (success) {
            response = createSuccessResponse("User role updated successfully");
            LOG_INFO() << "[AuthController] User role updated: " << userId << " -> " << newRole;
        } else {
            response = createErrorResponse(400, "Failed to update user role");
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Update user role error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

void AuthController::handleSetUserEnabled(const std::string& requestBody, const std::string& authHeader, std::string& response) {
    if (!m_initialized) {
        response = createErrorResponse(500, "Authentication service not initialized");
        return;
    }

    try {
        // Validate admin authorization
        AuthService::AuthResult authResult = validateAuthHeader(authHeader);
        if (!authResult.success || authResult.role != "admin") {
            response = createErrorResponse(403, "Admin privileges required");
            return;
        }

        // Validate request body
        nlohmann::json requestJson = validateRequestBody(requestBody, {"user_id", "enabled"});
        if (requestJson.empty()) {
            response = createErrorResponse(400, "Invalid request body. User ID and enabled status are required");
            return;
        }

        std::string userId = requestJson["user_id"];
        bool enabled = requestJson["enabled"];

        // Update user enabled status
        bool success = m_authService->setUserEnabled(authResult.token, userId, enabled);

        if (success) {
            std::string status = enabled ? "enabled" : "disabled";
            response = createSuccessResponse("User " + status + " successfully");
            LOG_INFO() << "[AuthController] User " << status << ": " << userId;
        } else {
            response = createErrorResponse(400, "Failed to update user status");
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthController] Set user enabled error: " << e.what();
        response = createErrorResponse(500, "Internal server error");
    }
}

std::string AuthController::createResponse(int statusCode, const std::string& message, const nlohmann::json& data) {
    nlohmann::json response;
    response["status"] = statusCode;
    response["message"] = message;

    if (!data.empty()) {
        response["data"] = data;
    }

    return "HTTP/1.1 " + std::to_string(statusCode) + " " +
           (statusCode == 200 ? "OK" : (statusCode == 401 ? "Unauthorized" :
            (statusCode == 403 ? "Forbidden" : (statusCode == 404 ? "Not Found" : "Error")))) +
           "\r\nContent-Type: application/json\r\n\r\n" + response.dump();
}

std::string AuthController::createErrorResponse(int statusCode, const std::string& message) {
    return createResponse(statusCode, message);
}

std::string AuthController::createSuccessResponse(const std::string& message, const nlohmann::json& data) {
    return createResponse(200, message, data);
}

nlohmann::json AuthController::validateRequestBody(const std::string& requestBody, const std::vector<std::string>& requiredFields) {
    try {
        nlohmann::json json = nlohmann::json::parse(requestBody);

        // Check required fields
        for (const auto& field : requiredFields) {
            if (!json.contains(field) || json[field].is_null()) {
                LOG_WARN() << "[AuthController] Missing required field: " << field;
                return nlohmann::json();
            }
        }

        return json;

    } catch (const std::exception& e) {
        LOG_WARN() << "[AuthController] Invalid JSON in request body: " << e.what();
        return nlohmann::json();
    }
}

nlohmann::json AuthController::userToJson(const UserRecord& user) {
    nlohmann::json userJson;
    userJson["user_id"] = user.user_id;
    userJson["username"] = user.username;
    userJson["role"] = user.role;
    userJson["enabled"] = user.enabled;
    userJson["created_at"] = user.created_at;
    userJson["last_login"] = user.last_login;
    // Note: password_hash is intentionally excluded for security
    return userJson;
}

nlohmann::json AuthController::authResultToJson(const AuthService::AuthResult& authResult) {
    nlohmann::json resultJson;
    resultJson["success"] = authResult.success;
    resultJson["message"] = authResult.message;

    if (authResult.success) {
        resultJson["token"] = authResult.token;
        resultJson["user_id"] = authResult.user_id;
        resultJson["username"] = authResult.username;
        resultJson["role"] = authResult.role;
        resultJson["expires_in"] = authResult.expires_in_seconds;
    }

    return resultJson;
}

} // namespace AISecurityVision
