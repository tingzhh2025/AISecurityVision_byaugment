#include "JWTManager.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>

JWTManager::JWTManager(const std::string& secretKey) {
    if (secretKey.empty()) {
        m_secretKey = generateSecretKey();
        LOG_INFO() << "[JWTManager] Generated new secret key for JWT signing";
    } else {
        m_secretKey = secretKey;
        LOG_INFO() << "[JWTManager] Using provided secret key for JWT signing";
    }
}

JWTManager::JWTToken JWTManager::generateToken(const UserClaims& claims) {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Create header and payload
        std::string header = createHeader();
        std::string payload = createPayload(claims);

        // Encode header and payload
        std::string encodedHeader = base64UrlEncode(header);
        std::string encodedPayload = base64UrlEncode(payload);

        // Create signature
        std::string signature = createSignature(encodedHeader, encodedPayload);

        // Combine into final token
        std::string token = encodedHeader + "." + encodedPayload + "." + signature;

        LOG_INFO() << "[JWTManager] Generated JWT token for user: " << claims.username;

        return JWTToken(token, claims.user_id, claims.username, claims.role, claims.expires_at);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[JWTManager] Failed to generate token: " << e.what();
        return JWTToken();
    }
}

JWTManager::JWTToken JWTManager::validateToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Check if token is revoked
        if (isTokenRevoked(token)) {
            LOG_WARN() << "[JWTManager] Token validation failed: token is revoked";
            return JWTToken();
        }

        // Parse token components
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            LOG_WARN() << "[JWTManager] Token validation failed: invalid token format";
            return JWTToken();
        }

        // Verify signature
        if (!verifySignature(header, payload, signature)) {
            LOG_WARN() << "[JWTManager] Token validation failed: invalid signature";
            return JWTToken();
        }

        // Decode and parse payload
        std::string decodedPayload = base64UrlDecode(payload);
        nlohmann::json payloadJson = nlohmann::json::parse(decodedPayload);

        // Extract claims
        std::string userId = payloadJson.value("sub", "");
        std::string username = payloadJson.value("username", "");
        std::string role = payloadJson.value("role", "user");
        int64_t exp = payloadJson.value("exp", 0);

        // Check expiration
        auto expirationTime = unixTimestampToTimePoint(exp);
        auto now = std::chrono::system_clock::now();

        if (now >= expirationTime) {
            LOG_WARN() << "[JWTManager] Token validation failed: token expired";
            return JWTToken();
        }

        LOG_DEBUG() << "[JWTManager] Token validated successfully for user: " << username;

        return JWTToken(token, userId, username, role, expirationTime);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[JWTManager] Token validation error: " << e.what();
        return JWTToken();
    }
}

JWTManager::JWTToken JWTManager::refreshToken(const std::string& token, int expirationHours) {
    // Validate current token
    JWTToken currentToken = validateToken(token);
    if (!currentToken.valid) {
        LOG_WARN() << "[JWTManager] Cannot refresh invalid token";
        return JWTToken();
    }

    // Create new claims with extended expiration
    UserClaims newClaims(currentToken.user_id, currentToken.username, currentToken.role, expirationHours);

    // Revoke old token
    revokeToken(token);

    // Generate new token
    return generateToken(newClaims);
}

bool JWTManager::revokeToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Parse token to get expiration time
        std::string header, payload, signature;
        if (!parseToken(token, header, payload, signature)) {
            return false;
        }

        std::string decodedPayload = base64UrlDecode(payload);
        nlohmann::json payloadJson = nlohmann::json::parse(decodedPayload);
        int64_t exp = payloadJson.value("exp", 0);

        // Add to revoked tokens with expiration time
        m_revokedTokens[token] = unixTimestampToTimePoint(exp);

        LOG_INFO() << "[JWTManager] Token revoked successfully";
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[JWTManager] Failed to revoke token: " << e.what();
        return false;
    }
}

bool JWTManager::isTokenRevoked(const std::string& token) {
    return m_revokedTokens.find(token) != m_revokedTokens.end();
}

void JWTManager::cleanupExpiredTokens() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto it = m_revokedTokens.begin();

    while (it != m_revokedTokens.end()) {
        if (now >= it->second) {
            it = m_revokedTokens.erase(it);
        } else {
            ++it;
        }
    }

    LOG_DEBUG() << "[JWTManager] Cleaned up expired revoked tokens";
}

std::string JWTManager::getSecretKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_secretKey;
}

void JWTManager::setSecretKey(const std::string& newSecretKey) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_secretKey = newSecretKey;
    
    // Clear revoked tokens as they're now invalid anyway
    m_revokedTokens.clear();
    
    LOG_INFO() << "[JWTManager] Secret key updated, all existing tokens invalidated";
}

int JWTManager::getTokenExpirationSeconds(const std::string& token) {
    try {
        JWTToken validatedToken = validateToken(token);
        if (!validatedToken.valid) {
            return 0;
        }

        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(validatedToken.expires_at - now);
        
        return std::max(0, static_cast<int>(duration.count()));

    } catch (const std::exception& e) {
        LOG_ERROR() << "[JWTManager] Failed to get token expiration: " << e.what();
        return 0;
    }
}

std::string JWTManager::generateSecretKey() {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string secretKey;
    secretKey.reserve(64);

    for (int i = 0; i < 64; ++i) {
        secretKey += chars[dis(gen)];
    }

    return secretKey;
}

std::string JWTManager::base64UrlEncode(const std::string& data) {
    // Simple base64 URL-safe encoding implementation
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string encoded;

    int val = 0, valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    // Remove padding for URL-safe encoding
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }

    return encoded;
}

std::string JWTManager::base64UrlDecode(const std::string& encoded) {
    // Simple base64 URL-safe decoding implementation
    std::string data = encoded;

    // Add padding if needed
    while (data.length() % 4) {
        data += '=';
    }

    // Replace URL-safe characters
    std::replace(data.begin(), data.end(), '-', '+');
    std::replace(data.begin(), data.end(), '_', '/');

    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string decoded;

    int val = 0, valb = -8;
    for (char c : data) {
        if (c == '=') break;

        size_t pos = chars.find(c);
        if (pos == std::string::npos) continue;

        val = (val << 6) + pos;
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return decoded;
}

std::string JWTManager::createHeader() {
    nlohmann::json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    return header.dump();
}

std::string JWTManager::createPayload(const UserClaims& claims) {
    nlohmann::json payload;
    payload["sub"] = claims.user_id;
    payload["username"] = claims.username;
    payload["role"] = claims.role;
    payload["iat"] = timePointToUnixTimestamp(claims.issued_at);
    payload["exp"] = timePointToUnixTimestamp(claims.expires_at);
    payload["iss"] = "AISecurityVision";
    return payload.dump();
}

std::string JWTManager::createSignature(const std::string& header, const std::string& payload) {
    std::string data = header + "." + payload;

    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hashLen;

    HMAC(EVP_sha256(), m_secretKey.c_str(), m_secretKey.length(),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         hash, &hashLen);

    std::string signature(reinterpret_cast<char*>(hash), hashLen);
    return base64UrlEncode(signature);
}

bool JWTManager::verifySignature(const std::string& header, const std::string& payload, const std::string& signature) {
    std::string expectedSignature = createSignature(header, payload);
    return expectedSignature == signature;
}

bool JWTManager::parseToken(const std::string& token, std::string& header, std::string& payload, std::string& signature) {
    size_t firstDot = token.find('.');
    size_t secondDot = token.find('.', firstDot + 1);

    if (firstDot == std::string::npos || secondDot == std::string::npos) {
        return false;
    }

    header = token.substr(0, firstDot);
    payload = token.substr(firstDot + 1, secondDot - firstDot - 1);
    signature = token.substr(secondDot + 1);

    return true;
}

int64_t JWTManager::timePointToUnixTimestamp(const std::chrono::system_clock::time_point& timePoint) {
    return std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
}

std::chrono::system_clock::time_point JWTManager::unixTimestampToTimePoint(int64_t timestamp) {
    return std::chrono::system_clock::from_time_t(timestamp);
}
