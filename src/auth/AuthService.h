#pragma once

#include <string>
#include <memory>
#include <chrono>
#include "JWTManager.h"
#include "../database/DatabaseManager.h"
#include "../core/Logger.h"

/**
 * @brief Authentication Service for user management (NEW - Phase 2)
 * 
 * This class provides high-level authentication operations including
 * user login, logout, registration, and session management.
 */
class AuthService {
public:
    /**
     * @brief Authentication result structure
     */
    struct AuthResult {
        bool success = false;
        std::string message;
        std::string token;
        std::string user_id;
        std::string username;
        std::string role;
        int expires_in_seconds = 0;

        AuthResult() = default;
        AuthResult(bool s, const std::string& msg) : success(s), message(msg) {}
        AuthResult(bool s, const std::string& msg, const std::string& t, 
                   const std::string& uid, const std::string& uname, 
                   const std::string& r, int exp)
            : success(s), message(msg), token(t), user_id(uid), 
              username(uname), role(r), expires_in_seconds(exp) {}
    };

    /**
     * @brief User registration data structure
     */
    struct UserRegistration {
        std::string username;
        std::string password;
        std::string role = "user";
        bool enabled = true;

        UserRegistration() = default;
        UserRegistration(const std::string& uname, const std::string& pwd, const std::string& r = "user")
            : username(uname), password(pwd), role(r) {}
    };

    /**
     * @brief Constructor
     * @param dbManager Database manager instance
     * @param jwtSecretKey JWT secret key (optional, auto-generated if empty)
     */
    explicit AuthService(std::shared_ptr<DatabaseManager> dbManager, const std::string& jwtSecretKey = "");

    /**
     * @brief Destructor
     */
    ~AuthService() = default;

    /**
     * @brief Authenticate user with username and password
     * @param username User's username
     * @param password User's password (plain text)
     * @param expirationHours Token expiration time in hours (default: 24)
     * @return Authentication result with token if successful
     */
    AuthResult login(const std::string& username, const std::string& password, int expirationHours = 24);

    /**
     * @brief Logout user by revoking token
     * @param token JWT token to revoke
     * @return True if logout successful
     */
    bool logout(const std::string& token);

    /**
     * @brief Validate JWT token and get user information
     * @param token JWT token to validate
     * @return Authentication result with user info if valid
     */
    AuthResult validateToken(const std::string& token);

    /**
     * @brief Refresh JWT token
     * @param token Current JWT token
     * @param expirationHours New expiration time in hours (default: 24)
     * @return New authentication result with refreshed token
     */
    AuthResult refreshToken(const std::string& token, int expirationHours = 24);

    /**
     * @brief Register a new user
     * @param registration User registration data
     * @return Authentication result indicating success/failure
     */
    AuthResult registerUser(const UserRegistration& registration);

    /**
     * @brief Change user password
     * @param userId User ID
     * @param currentPassword Current password
     * @param newPassword New password
     * @return True if password changed successfully
     */
    bool changePassword(const std::string& userId, const std::string& currentPassword, const std::string& newPassword);

    /**
     * @brief Update user role (admin only)
     * @param adminToken Admin's JWT token
     * @param userId Target user ID
     * @param newRole New role to assign
     * @return True if role updated successfully
     */
    bool updateUserRole(const std::string& adminToken, const std::string& userId, const std::string& newRole);

    /**
     * @brief Enable/disable user account (admin only)
     * @param adminToken Admin's JWT token
     * @param userId Target user ID
     * @param enabled Enable/disable flag
     * @return True if user status updated successfully
     */
    bool setUserEnabled(const std::string& adminToken, const std::string& userId, bool enabled);

    /**
     * @brief Get user information by token
     * @param token JWT token
     * @return User record if token is valid
     */
    UserRecord getCurrentUser(const std::string& token);

    /**
     * @brief Get all users (admin only)
     * @param adminToken Admin's JWT token
     * @return Vector of user records
     */
    std::vector<UserRecord> getAllUsers(const std::string& adminToken);

    /**
     * @brief Clean up expired sessions and tokens
     */
    void cleanupExpiredSessions();

    /**
     * @brief Initialize default admin user if no users exist
     * @param adminUsername Default admin username
     * @param adminPassword Default admin password
     * @return True if admin user created or already exists
     */
    bool initializeDefaultAdmin(const std::string& adminUsername = "admin", const std::string& adminPassword = "admin123");

private:
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<JWTManager> m_jwtManager;

    /**
     * @brief Hash password using bcrypt-like algorithm
     * @param password Plain text password
     * @return Hashed password
     */
    std::string hashPassword(const std::string& password);

    /**
     * @brief Verify password against hash
     * @param password Plain text password
     * @param hash Stored password hash
     * @return True if password matches
     */
    bool verifyPassword(const std::string& password, const std::string& hash);

    /**
     * @brief Generate unique user ID
     * @return Unique user ID string
     */
    std::string generateUserId();

    /**
     * @brief Generate unique session ID
     * @return Unique session ID string
     */
    std::string generateSessionId();

    /**
     * @brief Check if user has admin privileges
     * @param token JWT token
     * @return True if user is admin
     */
    bool isAdmin(const std::string& token);

    /**
     * @brief Validate username format
     * @param username Username to validate
     * @return True if username is valid
     */
    bool isValidUsername(const std::string& username);

    /**
     * @brief Validate password strength
     * @param password Password to validate
     * @return True if password meets requirements
     */
    bool isValidPassword(const std::string& password);
};
