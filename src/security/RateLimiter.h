#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <memory>
#include <atomic>
#include "../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief API限流器 - 防止恶意攻击和过度使用
 * 
 * 实现基于令牌桶算法的API限流机制，支持：
 * - 基于IP地址的限流
 * - 基于用户的限流
 * - 可配置的限流规则
 * - 自动清理过期记录
 */
class RateLimiter {
public:
    /**
     * @brief 限流规则配置
     */
    struct RateLimitConfig {
        int maxRequests;        // 最大请求数
        int windowSeconds;      // 时间窗口(秒)
        int burstSize;          // 突发请求大小
        
        RateLimitConfig(int max = 100, int window = 60, int burst = 10)
            : maxRequests(max), windowSeconds(window), burstSize(burst) {}
    };

    /**
     * @brief 限流结果
     */
    struct RateLimitResult {
        bool allowed;           // 是否允许请求
        int remaining;          // 剩余请求数
        int resetTime;          // 重置时间(秒)
        std::string message;    // 结果消息
        
        RateLimitResult(bool allow = true, int remain = 0, int reset = 0, const std::string& msg = "")
            : allowed(allow), remaining(remain), resetTime(reset), message(msg) {}
    };

private:
    /**
     * @brief 客户端请求记录
     */
    struct ClientRecord {
        int requestCount;                                   // 请求计数
        std::chrono::steady_clock::time_point windowStart; // 窗口开始时间
        std::chrono::steady_clock::time_point lastRequest; // 最后请求时间
        
        ClientRecord() : requestCount(0), 
                        windowStart(std::chrono::steady_clock::now()),
                        lastRequest(std::chrono::steady_clock::now()) {}
    };

public:
    /**
     * @brief 构造函数
     * @param defaultConfig 默认限流配置
     */
    explicit RateLimiter(const RateLimitConfig& defaultConfig = RateLimitConfig());

    /**
     * @brief 析构函数
     */
    ~RateLimiter() = default;

    /**
     * @brief 检查请求是否被限流
     * @param clientId 客户端标识(IP地址或用户ID)
     * @param endpoint API端点(可选，用于不同端点的不同限制)
     * @return 限流检查结果
     */
    RateLimitResult checkRequest(const std::string& clientId, const std::string& endpoint = "");

    /**
     * @brief 设置特定端点的限流配置
     * @param endpoint API端点
     * @param config 限流配置
     */
    void setEndpointConfig(const std::string& endpoint, const RateLimitConfig& config);

    /**
     * @brief 设置默认限流配置
     * @param config 默认配置
     */
    void setDefaultConfig(const RateLimitConfig& config);

    /**
     * @brief 获取客户端统计信息
     * @param clientId 客户端标识
     * @return 统计信息JSON字符串
     */
    std::string getClientStats(const std::string& clientId) const;

    /**
     * @brief 获取所有客户端统计信息
     * @return 统计信息JSON字符串
     */
    std::string getAllStats() const;

    /**
     * @brief 清理过期的客户端记录
     * @param maxAgeSeconds 最大保留时间(秒)
     */
    void cleanupExpiredRecords(int maxAgeSeconds = 3600);

    /**
     * @brief 重置客户端限流状态
     * @param clientId 客户端标识
     */
    void resetClient(const std::string& clientId);

    /**
     * @brief 重置所有客户端限流状态
     */
    void resetAll();

    /**
     * @brief 添加客户端到白名单
     * @param clientId 客户端标识
     */
    void addToWhitelist(const std::string& clientId);

    /**
     * @brief 从白名单移除客户端
     * @param clientId 客户端标识
     */
    void removeFromWhitelist(const std::string& clientId);

    /**
     * @brief 检查客户端是否在白名单中
     * @param clientId 客户端标识
     * @return 是否在白名单中
     */
    bool isWhitelisted(const std::string& clientId) const;

    /**
     * @brief 添加客户端到黑名单
     * @param clientId 客户端标识
     * @param durationSeconds 黑名单持续时间(秒)，0表示永久
     */
    void addToBlacklist(const std::string& clientId, int durationSeconds = 0);

    /**
     * @brief 从黑名单移除客户端
     * @param clientId 客户端标识
     */
    void removeFromBlacklist(const std::string& clientId);

    /**
     * @brief 检查客户端是否在黑名单中
     * @param clientId 客户端标识
     * @return 是否在黑名单中
     */
    bool isBlacklisted(const std::string& clientId) const;

private:
    /**
     * @brief 获取端点配置
     * @param endpoint API端点
     * @return 限流配置
     */
    RateLimitConfig getConfigForEndpoint(const std::string& endpoint) const;

    /**
     * @brief 更新客户端记录
     * @param record 客户端记录
     * @param config 限流配置
     * @return 是否允许请求
     */
    bool updateClientRecord(ClientRecord& record, const RateLimitConfig& config);

    /**
     * @brief 计算剩余请求数
     * @param record 客户端记录
     * @param config 限流配置
     * @return 剩余请求数
     */
    int calculateRemaining(const ClientRecord& record, const RateLimitConfig& config) const;

    /**
     * @brief 计算重置时间
     * @param record 客户端记录
     * @param config 限流配置
     * @return 重置时间(秒)
     */
    int calculateResetTime(const ClientRecord& record, const RateLimitConfig& config) const;

private:
    mutable std::mutex m_mutex;                                     // 线程安全锁
    RateLimitConfig m_defaultConfig;                                // 默认配置
    std::unordered_map<std::string, RateLimitConfig> m_endpointConfigs; // 端点配置
    std::unordered_map<std::string, ClientRecord> m_clientRecords;  // 客户端记录
    std::unordered_set<std::string> m_whitelist;                    // 白名单
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_blacklist; // 黑名单
    
    // 统计信息
    mutable std::atomic<uint64_t> m_totalRequests{0};              // 总请求数
    mutable std::atomic<uint64_t> m_blockedRequests{0};            // 被阻止的请求数
    mutable std::chrono::steady_clock::time_point m_startTime;     // 启动时间
};

} // namespace AISecurityVision
