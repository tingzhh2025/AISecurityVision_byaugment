#include "AuthService.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/rand.h>

AuthService::AuthService(std::shared_ptr<DatabaseManager> dbManager, const std::string& jwtSecretKey)
    : m_dbManager(dbManager), m_jwtManager(std::make_unique<JWTManager>(jwtSecretKey)) {
    
    if (!m_dbManager) {
        throw std::runtime_error("DatabaseManager cannot be null");
    }

    LOG_INFO() << "[AuthService] Authentication service initialized";
    
    // Initialize default admin user if no users exist
    initializeDefaultAdmin();
}

AuthService::AuthResult AuthService::login(const std::string& username, const std::string& password, int expirationHours) {
    try {
        // Validate input
        if (username.empty() || password.empty()) {
            LOG_WARN() << "[AuthService] Login failed: empty username or password";
            return AuthResult(false, "Username and password are required");
        }

        // Get user from database
        UserRecord user = m_dbManager->getUserByUsername(username);
        if (user.user_id.empty()) {
            LOG_WARN() << "[AuthService] Login failed: user not found: " << username;
            return AuthResult(false, "Invalid username or password");
        }

        // Check if user is enabled
        if (!user.enabled) {
            LOG_WARN() << "[AuthService] Login failed: user disabled: " << username;
            return AuthResult(false, "User account is disabled");
        }

        // Verify password
        if (!verifyPassword(password, user.password_hash)) {
            LOG_WARN() << "[AuthService] Login failed: invalid password for user: " << username;
            return AuthResult(false, "Invalid username or password");
        }

        // Create JWT claims
        JWTManager::UserClaims claims(user.user_id, user.username, user.role, expirationHours);

        // Generate JWT token
        JWTManager::JWTToken jwtToken = m_jwtManager->generateToken(claims);
        if (!jwtToken.valid) {
            LOG_ERROR() << "[AuthService] Login failed: could not generate JWT token";
            return AuthResult(false, "Authentication failed");
        }

        // Create session record
        SessionRecord session(generateSessionId(), user.user_id, expirationHours);
        if (!m_dbManager->insertSession(session)) {
            LOG_WARN() << "[AuthService] Could not create session record for user: " << username;
            // Continue anyway, token is still valid
        }

        // Update last login time
        m_dbManager->updateUserLastLogin(user.user_id);

        LOG_INFO() << "[AuthService] User logged in successfully: " << username;

        return AuthResult(true, "Login successful", jwtToken.token, user.user_id, 
                         user.username, user.role, m_jwtManager->getTokenExpirationSeconds(jwtToken.token));

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Login error: " << e.what();
        return AuthResult(false, "Authentication failed");
    }
}

bool AuthService::logout(const std::string& token) {
    try {
        // Validate token first
        JWTManager::JWTToken jwtToken = m_jwtManager->validateToken(token);
        if (!jwtToken.valid) {
            LOG_WARN() << "[AuthService] Logout failed: invalid token";
            return false;
        }

        // Revoke JWT token
        bool tokenRevoked = m_jwtManager->revokeToken(token);

        // Delete user sessions from database
        bool sessionsDeleted = m_dbManager->deleteUserSessions(jwtToken.user_id);

        LOG_INFO() << "[AuthService] User logged out: " << jwtToken.username;

        return tokenRevoked && sessionsDeleted;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Logout error: " << e.what();
        return false;
    }
}

AuthService::AuthResult AuthService::validateToken(const std::string& token) {
    try {
        // Validate JWT token
        JWTManager::JWTToken jwtToken = m_jwtManager->validateToken(token);
        if (!jwtToken.valid) {
            return AuthResult(false, "Invalid or expired token");
        }

        // Get user from database to ensure they still exist and are enabled
        UserRecord user = m_dbManager->getUserById(jwtToken.user_id);
        if (user.user_id.empty() || !user.enabled) {
            // Revoke token if user no longer exists or is disabled
            m_jwtManager->revokeToken(token);
            return AuthResult(false, "User account not found or disabled");
        }

        int expirationSeconds = m_jwtManager->getTokenExpirationSeconds(token);

        return AuthResult(true, "Token valid", token, user.user_id, 
                         user.username, user.role, expirationSeconds);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Token validation error: " << e.what();
        return AuthResult(false, "Token validation failed");
    }
}

AuthService::AuthResult AuthService::refreshToken(const std::string& token, int expirationHours) {
    try {
        // Validate current token
        AuthResult currentAuth = validateToken(token);
        if (!currentAuth.success) {
            return AuthResult(false, "Cannot refresh invalid token");
        }

        // Generate new token
        JWTManager::JWTToken newToken = m_jwtManager->refreshToken(token, expirationHours);
        if (!newToken.valid) {
            return AuthResult(false, "Failed to refresh token");
        }

        LOG_INFO() << "[AuthService] Token refreshed for user: " << currentAuth.username;

        return AuthResult(true, "Token refreshed", newToken.token, newToken.user_id,
                         newToken.username, newToken.role, 
                         m_jwtManager->getTokenExpirationSeconds(newToken.token));

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Token refresh error: " << e.what();
        return AuthResult(false, "Token refresh failed");
    }
}

AuthService::AuthResult AuthService::registerUser(const UserRegistration& registration) {
    try {
        // Validate input
        if (!isValidUsername(registration.username)) {
            return AuthResult(false, "Invalid username format");
        }

        if (!isValidPassword(registration.password)) {
            return AuthResult(false, "Password does not meet requirements");
        }

        // Check if username already exists
        UserRecord existingUser = m_dbManager->getUserByUsername(registration.username);
        if (!existingUser.user_id.empty()) {
            LOG_WARN() << "[AuthService] Registration failed: username already exists: " << registration.username;
            return AuthResult(false, "Username already exists");
        }

        // Create new user record
        UserRecord newUser;
        newUser.user_id = generateUserId();
        newUser.username = registration.username;
        newUser.password_hash = hashPassword(registration.password);
        newUser.role = registration.role;
        newUser.enabled = registration.enabled;

        // Insert user into database
        if (!m_dbManager->insertUser(newUser)) {
            LOG_ERROR() << "[AuthService] Failed to insert new user: " << registration.username;
            return AuthResult(false, "Failed to create user account");
        }

        LOG_INFO() << "[AuthService] User registered successfully: " << registration.username;

        return AuthResult(true, "User registered successfully");

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] User registration error: " << e.what();
        return AuthResult(false, "Registration failed");
    }
}

bool AuthService::changePassword(const std::string& userId, const std::string& currentPassword, const std::string& newPassword) {
    try {
        // Get user from database
        UserRecord user = m_dbManager->getUserById(userId);
        if (user.user_id.empty()) {
            LOG_WARN() << "[AuthService] Change password failed: user not found: " << userId;
            return false;
        }

        // Verify current password
        if (!verifyPassword(currentPassword, user.password_hash)) {
            LOG_WARN() << "[AuthService] Change password failed: invalid current password for user: " << userId;
            return false;
        }

        // Validate new password
        if (!isValidPassword(newPassword)) {
            LOG_WARN() << "[AuthService] Change password failed: new password does not meet requirements";
            return false;
        }

        // Update password
        user.password_hash = hashPassword(newPassword);
        if (!m_dbManager->updateUser(user)) {
            LOG_ERROR() << "[AuthService] Failed to update password for user: " << userId;
            return false;
        }

        // Revoke all existing sessions for this user
        m_dbManager->deleteUserSessions(userId);

        LOG_INFO() << "[AuthService] Password changed successfully for user: " << user.username;
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Change password error: " << e.what();
        return false;
    }
}

bool AuthService::updateUserRole(const std::string& adminToken, const std::string& userId, const std::string& newRole) {
    try {
        // Verify admin privileges
        if (!isAdmin(adminToken)) {
            LOG_WARN() << "[AuthService] Update user role failed: insufficient privileges";
            return false;
        }

        // Get target user
        UserRecord user = m_dbManager->getUserById(userId);
        if (user.user_id.empty()) {
            LOG_WARN() << "[AuthService] Update user role failed: user not found: " << userId;
            return false;
        }

        // Update role
        user.role = newRole;
        if (!m_dbManager->updateUser(user)) {
            LOG_ERROR() << "[AuthService] Failed to update role for user: " << userId;
            return false;
        }

        LOG_INFO() << "[AuthService] User role updated: " << user.username << " -> " << newRole;
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Update user role error: " << e.what();
        return false;
    }
}

bool AuthService::setUserEnabled(const std::string& adminToken, const std::string& userId, bool enabled) {
    try {
        // Verify admin privileges
        if (!isAdmin(adminToken)) {
            LOG_WARN() << "[AuthService] Set user enabled failed: insufficient privileges";
            return false;
        }

        // Get target user
        UserRecord user = m_dbManager->getUserById(userId);
        if (user.user_id.empty()) {
            LOG_WARN() << "[AuthService] Set user enabled failed: user not found: " << userId;
            return false;
        }

        // Update enabled status
        user.enabled = enabled;
        if (!m_dbManager->updateUser(user)) {
            LOG_ERROR() << "[AuthService] Failed to update enabled status for user: " << userId;
            return false;
        }

        // If disabling user, revoke all their sessions
        if (!enabled) {
            m_dbManager->deleteUserSessions(userId);
        }

        LOG_INFO() << "[AuthService] User enabled status updated: " << user.username << " -> " << (enabled ? "enabled" : "disabled");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Set user enabled error: " << e.what();
        return false;
    }
}

UserRecord AuthService::getCurrentUser(const std::string& token) {
    try {
        AuthResult auth = validateToken(token);
        if (!auth.success) {
            return UserRecord();
        }

        return m_dbManager->getUserById(auth.user_id);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Get current user error: " << e.what();
        return UserRecord();
    }
}

std::vector<UserRecord> AuthService::getAllUsers(const std::string& adminToken) {
    try {
        // Verify admin privileges
        if (!isAdmin(adminToken)) {
            LOG_WARN() << "[AuthService] Get all users failed: insufficient privileges";
            return std::vector<UserRecord>();
        }

        return m_dbManager->getAllUsers();

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Get all users error: " << e.what();
        return std::vector<UserRecord>();
    }
}

void AuthService::cleanupExpiredSessions() {
    try {
        // Clean up expired sessions in database
        m_dbManager->deleteExpiredSessions();

        // Clean up expired tokens in JWT manager
        m_jwtManager->cleanupExpiredTokens();

        LOG_DEBUG() << "[AuthService] Cleaned up expired sessions and tokens";

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Cleanup expired sessions error: " << e.what();
    }
}

bool AuthService::initializeDefaultAdmin(const std::string& adminUsername, const std::string& adminPassword) {
    try {
        // Check if any users exist
        std::vector<UserRecord> users = m_dbManager->getAllUsers();
        if (!users.empty()) {
            LOG_INFO() << "[AuthService] Users already exist, skipping default admin creation";
            return true;
        }

        // Create default admin user
        UserRegistration adminReg(adminUsername, adminPassword, "admin");
        AuthResult result = registerUser(adminReg);

        if (result.success) {
            LOG_INFO() << "[AuthService] Default admin user created: " << adminUsername;
        } else {
            LOG_ERROR() << "[AuthService] Failed to create default admin user: " << result.message;
        }

        return result.success;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Initialize default admin error: " << e.what();
        return false;
    }
}

std::string AuthService::hashPassword(const std::string& password) {
    // Simple SHA-256 based password hashing with salt
    // In production, use bcrypt or similar

    // Generate random salt
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        throw std::runtime_error("Failed to generate salt");
    }

    // Convert salt to hex string
    std::stringstream saltHex;
    for (int i = 0; i < 16; ++i) {
        saltHex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(salt[i]);
    }

    // Combine password and salt
    std::string saltedPassword = password + saltHex.str();

    // Hash with SHA-256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(saltedPassword.c_str()), saltedPassword.length(), hash);

    // Convert hash to hex string
    std::stringstream hashHex;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashHex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    // Return salt + hash
    return saltHex.str() + hashHex.str();
}

bool AuthService::verifyPassword(const std::string& password, const std::string& hash) {
    if (hash.length() < 32) {
        return false;
    }

    // Extract salt (first 32 characters)
    std::string salt = hash.substr(0, 32);
    std::string storedHash = hash.substr(32);

    // Combine password and salt
    std::string saltedPassword = password + salt;

    // Hash with SHA-256
    unsigned char computedHash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(saltedPassword.c_str()), saltedPassword.length(), computedHash);

    // Convert to hex string
    std::stringstream hashHex;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashHex << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(computedHash[i]);
    }

    return hashHex.str() == storedHash;
}

std::string AuthService::generateUserId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "user_";
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

std::string AuthService::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "sess_";
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

bool AuthService::isAdmin(const std::string& token) {
    try {
        AuthResult auth = validateToken(token);
        return auth.success && auth.role == "admin";
    } catch (const std::exception& e) {
        LOG_ERROR() << "[AuthService] Is admin check error: " << e.what();
        return false;
    }
}

bool AuthService::isValidUsername(const std::string& username) {
    // Username must be 3-50 characters, alphanumeric and underscore only
    if (username.length() < 3 || username.length() > 50) {
        return false;
    }

    for (char c : username) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }

    return true;
}

bool AuthService::isValidPassword(const std::string& password) {
    // Password must be at least 6 characters
    return password.length() >= 6;
}
