#include "RateLimiter.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <unordered_set>

using namespace AISecurityVision;

RateLimiter::RateLimiter(const RateLimitConfig& defaultConfig)
    : m_defaultConfig(defaultConfig), m_startTime(std::chrono::steady_clock::now()) {
    LOG_INFO() << "[RateLimiter] Initialized with default config: " 
               << m_defaultConfig.maxRequests << " requests/" 
               << m_defaultConfig.windowSeconds << "s";
}

RateLimiter::RateLimitResult RateLimiter::checkRequest(const std::string& clientId, const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_totalRequests++;
    
    // 检查黑名单
    if (isBlacklisted(clientId)) {
        m_blockedRequests++;
        LOG_WARN() << "[RateLimiter] Request blocked - client in blacklist: " << clientId;
        return RateLimitResult(false, 0, 0, "Client is blacklisted");
    }
    
    // 检查白名单
    if (isWhitelisted(clientId)) {
        LOG_DEBUG() << "[RateLimiter] Request allowed - client in whitelist: " << clientId;
        return RateLimitResult(true, 999, 0, "Whitelisted client");
    }
    
    // 获取配置
    RateLimitConfig config = getConfigForEndpoint(endpoint);
    
    // 获取或创建客户端记录
    auto& record = m_clientRecords[clientId];
    
    // 检查是否允许请求
    bool allowed = updateClientRecord(record, config);
    
    if (!allowed) {
        m_blockedRequests++;
        LOG_WARN() << "[RateLimiter] Request blocked - rate limit exceeded: " << clientId 
                   << " (endpoint: " << endpoint << ")";
    }
    
    int remaining = calculateRemaining(record, config);
    int resetTime = calculateResetTime(record, config);
    
    std::string message = allowed ? "Request allowed" : "Rate limit exceeded";
    
    return RateLimitResult(allowed, remaining, resetTime, message);
}

void RateLimiter::setEndpointConfig(const std::string& endpoint, const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_endpointConfigs[endpoint] = config;
    LOG_INFO() << "[RateLimiter] Set endpoint config for " << endpoint 
               << ": " << config.maxRequests << " requests/" << config.windowSeconds << "s";
}

void RateLimiter::setDefaultConfig(const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultConfig = config;
    LOG_INFO() << "[RateLimiter] Updated default config: " 
               << config.maxRequests << " requests/" << config.windowSeconds << "s";
}

std::string RateLimiter::getClientStats(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    nlohmann::json stats;
    stats["client_id"] = clientId;
    stats["whitelisted"] = isWhitelisted(clientId);
    stats["blacklisted"] = isBlacklisted(clientId);
    
    auto it = m_clientRecords.find(clientId);
    if (it != m_clientRecords.end()) {
        const auto& record = it->second;
        stats["request_count"] = record.requestCount;
        
        auto now = std::chrono::steady_clock::now();
        auto windowAge = std::chrono::duration_cast<std::chrono::seconds>(now - record.windowStart).count();
        auto lastRequestAge = std::chrono::duration_cast<std::chrono::seconds>(now - record.lastRequest).count();
        
        stats["window_age_seconds"] = windowAge;
        stats["last_request_age_seconds"] = lastRequestAge;
    } else {
        stats["request_count"] = 0;
        stats["window_age_seconds"] = 0;
        stats["last_request_age_seconds"] = 0;
    }
    
    return stats.dump();
}

std::string RateLimiter::getAllStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    nlohmann::json stats;
    
    // 全局统计
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
    
    stats["global"]["total_requests"] = m_totalRequests.load();
    stats["global"]["blocked_requests"] = m_blockedRequests.load();
    stats["global"]["uptime_seconds"] = uptime;
    stats["global"]["active_clients"] = m_clientRecords.size();
    stats["global"]["whitelist_size"] = m_whitelist.size();
    stats["global"]["blacklist_size"] = m_blacklist.size();
    
    // 配置信息
    stats["config"]["default"]["max_requests"] = m_defaultConfig.maxRequests;
    stats["config"]["default"]["window_seconds"] = m_defaultConfig.windowSeconds;
    stats["config"]["default"]["burst_size"] = m_defaultConfig.burstSize;
    
    // 端点配置
    for (const auto& [endpoint, config] : m_endpointConfigs) {
        stats["config"]["endpoints"][endpoint]["max_requests"] = config.maxRequests;
        stats["config"]["endpoints"][endpoint]["window_seconds"] = config.windowSeconds;
        stats["config"]["endpoints"][endpoint]["burst_size"] = config.burstSize;
    }
    
    // 客户端统计
    for (const auto& [clientId, record] : m_clientRecords) {
        auto windowAge = std::chrono::duration_cast<std::chrono::seconds>(now - record.windowStart).count();
        auto lastRequestAge = std::chrono::duration_cast<std::chrono::seconds>(now - record.lastRequest).count();
        
        stats["clients"][clientId]["request_count"] = record.requestCount;
        stats["clients"][clientId]["window_age_seconds"] = windowAge;
        stats["clients"][clientId]["last_request_age_seconds"] = lastRequestAge;
        stats["clients"][clientId]["whitelisted"] = isWhitelisted(clientId);
        stats["clients"][clientId]["blacklisted"] = isBlacklisted(clientId);
    }
    
    return stats.dump();
}

void RateLimiter::cleanupExpiredRecords(int maxAgeSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto maxAge = std::chrono::seconds(maxAgeSeconds);
    
    // 清理客户端记录
    auto clientIt = m_clientRecords.begin();
    while (clientIt != m_clientRecords.end()) {
        if (now - clientIt->second.lastRequest > maxAge) {
            LOG_DEBUG() << "[RateLimiter] Cleaning up expired record for client: " << clientIt->first;
            clientIt = m_clientRecords.erase(clientIt);
        } else {
            ++clientIt;
        }
    }
    
    // 清理黑名单中的过期记录
    auto blacklistIt = m_blacklist.begin();
    while (blacklistIt != m_blacklist.end()) {
        if (blacklistIt->second != std::chrono::steady_clock::time_point{} && 
            now > blacklistIt->second) {
            LOG_INFO() << "[RateLimiter] Removing expired blacklist entry: " << blacklistIt->first;
            blacklistIt = m_blacklist.erase(blacklistIt);
        } else {
            ++blacklistIt;
        }
    }
}

void RateLimiter::resetClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clientRecords.erase(clientId);
    LOG_INFO() << "[RateLimiter] Reset client: " << clientId;
}

void RateLimiter::resetAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_clientRecords.clear();
    m_totalRequests = 0;
    m_blockedRequests = 0;
    m_startTime = std::chrono::steady_clock::now();
    LOG_INFO() << "[RateLimiter] Reset all client records and statistics";
}

void RateLimiter::addToWhitelist(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_whitelist.insert(clientId);
    LOG_INFO() << "[RateLimiter] Added to whitelist: " << clientId;
}

void RateLimiter::removeFromWhitelist(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_whitelist.erase(clientId);
    LOG_INFO() << "[RateLimiter] Removed from whitelist: " << clientId;
}

bool RateLimiter::isWhitelisted(const std::string& clientId) const {
    return m_whitelist.find(clientId) != m_whitelist.end();
}

void RateLimiter::addToBlacklist(const std::string& clientId, int durationSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto expireTime = std::chrono::steady_clock::time_point{};
    if (durationSeconds > 0) {
        expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    }
    
    m_blacklist[clientId] = expireTime;
    LOG_WARN() << "[RateLimiter] Added to blacklist: " << clientId 
               << " (duration: " << (durationSeconds > 0 ? std::to_string(durationSeconds) + "s" : "permanent") << ")";
}

void RateLimiter::removeFromBlacklist(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blacklist.erase(clientId);
    LOG_INFO() << "[RateLimiter] Removed from blacklist: " << clientId;
}

bool RateLimiter::isBlacklisted(const std::string& clientId) const {
    auto it = m_blacklist.find(clientId);
    if (it == m_blacklist.end()) {
        return false;
    }
    
    // 检查是否过期
    if (it->second != std::chrono::steady_clock::time_point{} && 
        std::chrono::steady_clock::now() > it->second) {
        return false;
    }
    
    return true;
}

RateLimiter::RateLimitConfig RateLimiter::getConfigForEndpoint(const std::string& endpoint) const {
    auto it = m_endpointConfigs.find(endpoint);
    return (it != m_endpointConfigs.end()) ? it->second : m_defaultConfig;
}

bool RateLimiter::updateClientRecord(ClientRecord& record, const RateLimitConfig& config) {
    auto now = std::chrono::steady_clock::now();
    auto windowDuration = std::chrono::seconds(config.windowSeconds);
    
    // 检查是否需要重置窗口
    if (now - record.windowStart >= windowDuration) {
        record.windowStart = now;
        record.requestCount = 0;
    }
    
    // 更新最后请求时间
    record.lastRequest = now;
    
    // 检查是否超过限制
    if (record.requestCount >= config.maxRequests) {
        return false;
    }
    
    // 增加请求计数
    record.requestCount++;
    return true;
}

int RateLimiter::calculateRemaining(const ClientRecord& record, const RateLimitConfig& config) const {
    return std::max(0, config.maxRequests - record.requestCount);
}

int RateLimiter::calculateResetTime(const ClientRecord& record, const RateLimitConfig& config) const {
    auto now = std::chrono::steady_clock::now();
    auto windowEnd = record.windowStart + std::chrono::seconds(config.windowSeconds);
    
    if (windowEnd > now) {
        return std::chrono::duration_cast<std::chrono::seconds>(windowEnd - now).count();
    }
    
    return 0;
}
