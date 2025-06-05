#pragma once

#include <string>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include "../core/Logger.h"

/**
 * @brief JWT Token Manager for user authentication (NEW - Phase 2)
 * 
 * This class provides JWT token generation, validation, and management
 * for the AI Security Vision System authentication system.
 */
class JWTManager {
public:
    /**
     * @brief JWT Token structure
     */
    struct JWTToken {
        std::string token;
        std::string user_id;
        std::string username;
        std::string role;
        std::chrono::system_clock::time_point issued_at;
        std::chrono::system_clock::time_point expires_at;
        bool valid = false;

        JWTToken() = default;
        JWTToken(const std::string& t, const std::string& uid, const std::string& uname, 
                 const std::string& r, std::chrono::system_clock::time_point exp)
            : token(t), user_id(uid), username(uname), role(r), 
              issued_at(std::chrono::system_clock::now()), expires_at(exp), valid(true) {}
    };

    /**
     * @brief User claims structure for JWT payload
     */
    struct UserClaims {
        std::string user_id;
        std::string username;
        std::string role;
        std::chrono::system_clock::time_point issued_at;
        std::chrono::system_clock::time_point expires_at;

        UserClaims() = default;
        UserClaims(const std::string& uid, const std::string& uname, const std::string& r, int expirationHours = 24)
            : user_id(uid), username(uname), role(r), issued_at(std::chrono::system_clock::now()) {
            expires_at = issued_at + std::chrono::hours(expirationHours);
        }
    };

    /**
     * @brief Constructor
     * @param secretKey Secret key for JWT signing (default: auto-generated)
     */
    explicit JWTManager(const std::string& secretKey = "");

    /**
     * @brief Destructor
     */
    ~JWTManager() = default;

    /**
     * @brief Generate a JWT token for user
     * @param claims User claims to encode in token
     * @return Generated JWT token
     */
    JWTToken generateToken(const UserClaims& claims);

    /**
     * @brief Validate and decode a JWT token
     * @param token JWT token string to validate
     * @return Decoded token with user claims (valid=false if invalid)
     */
    JWTToken validateToken(const std::string& token);

    /**
     * @brief Refresh a JWT token (extend expiration)
     * @param token Current JWT token
     * @param expirationHours New expiration time in hours
     * @return New JWT token with extended expiration
     */
    JWTToken refreshToken(const std::string& token, int expirationHours = 24);

    /**
     * @brief Revoke a JWT token (add to blacklist)
     * @param token JWT token to revoke
     * @return True if successfully revoked
     */
    bool revokeToken(const std::string& token);

    /**
     * @brief Check if a token is revoked
     * @param token JWT token to check
     * @return True if token is revoked
     */
    bool isTokenRevoked(const std::string& token);

    /**
     * @brief Clean up expired tokens from blacklist
     */
    void cleanupExpiredTokens();

    /**
     * @brief Get current secret key
     * @return Current JWT secret key
     */
    std::string getSecretKey() const;

    /**
     * @brief Set new secret key (invalidates all existing tokens)
     * @param newSecretKey New secret key
     */
    void setSecretKey(const std::string& newSecretKey);

    /**
     * @brief Get token expiration time in seconds
     * @param token JWT token
     * @return Seconds until expiration (0 if expired or invalid)
     */
    int getTokenExpirationSeconds(const std::string& token);

private:
    std::string m_secretKey;
    std::map<std::string, std::chrono::system_clock::time_point> m_revokedTokens; // Token blacklist
    mutable std::mutex m_mutex;

    /**
     * @brief Generate a random secret key
     * @return Random secret key string
     */
    std::string generateSecretKey();

    /**
     * @brief Base64 URL-safe encode
     * @param data Data to encode
     * @return Base64 URL-safe encoded string
     */
    std::string base64UrlEncode(const std::string& data);

    /**
     * @brief Base64 URL-safe decode
     * @param encoded Encoded string to decode
     * @return Decoded data
     */
    std::string base64UrlDecode(const std::string& encoded);

    /**
     * @brief Create JWT header
     * @return JWT header JSON string
     */
    std::string createHeader();

    /**
     * @brief Create JWT payload from user claims
     * @param claims User claims
     * @return JWT payload JSON string
     */
    std::string createPayload(const UserClaims& claims);

    /**
     * @brief Create JWT signature
     * @param header JWT header (base64 encoded)
     * @param payload JWT payload (base64 encoded)
     * @return JWT signature (base64 encoded)
     */
    std::string createSignature(const std::string& header, const std::string& payload);

    /**
     * @brief Verify JWT signature
     * @param header JWT header (base64 encoded)
     * @param payload JWT payload (base64 encoded)
     * @param signature JWT signature (base64 encoded)
     * @return True if signature is valid
     */
    bool verifySignature(const std::string& header, const std::string& payload, const std::string& signature);

    /**
     * @brief Parse JWT token into components
     * @param token JWT token string
     * @param header Output header component
     * @param payload Output payload component
     * @param signature Output signature component
     * @return True if parsing successful
     */
    bool parseToken(const std::string& token, std::string& header, std::string& payload, std::string& signature);

    /**
     * @brief Convert time point to Unix timestamp
     * @param timePoint Time point to convert
     * @return Unix timestamp
     */
    int64_t timePointToUnixTimestamp(const std::chrono::system_clock::time_point& timePoint);

    /**
     * @brief Convert Unix timestamp to time point
     * @param timestamp Unix timestamp
     * @return Time point
     */
    std::chrono::system_clock::time_point unixTimestampToTimePoint(int64_t timestamp);
};
