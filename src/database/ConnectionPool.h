#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include "../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief SQLite数据库连接池
 * 
 * 提供高性能的数据库连接管理：
 * - 连接池管理和复用
 * - 自动连接健康检查
 * - 连接超时和清理
 * - 性能监控和统计
 * - 线程安全操作
 */
class ConnectionPool {
public:
    /**
     * @brief 连接池配置
     */
    struct PoolConfig {
        std::string dbPath;                 // 数据库文件路径
        int minConnections;                 // 最小连接数
        int maxConnections;                 // 最大连接数
        int connectionTimeoutSeconds;       // 连接超时时间(秒)
        int idleTimeoutSeconds;             // 空闲超时时间(秒)
        int healthCheckIntervalSeconds;     // 健康检查间隔(秒)
        bool enableWALMode;                 // 是否启用WAL模式
        bool enableForeignKeys;             // 是否启用外键约束
        int busyTimeoutMs;                  // 忙碌超时时间(毫秒)
        
        PoolConfig() : dbPath("aibox.db"), minConnections(2), maxConnections(10),
                      connectionTimeoutSeconds(30), idleTimeoutSeconds(300),
                      healthCheckIntervalSeconds(60), enableWALMode(true),
                      enableForeignKeys(true), busyTimeoutMs(5000) {}
    };

    /**
     * @brief 数据库连接包装器
     */
    class Connection {
    public:
        Connection(sqlite3* db, ConnectionPool* pool);
        ~Connection();
        
        // 禁止拷贝
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        
        // 允许移动
        Connection(Connection&& other) noexcept;
        Connection& operator=(Connection&& other) noexcept;
        
        /**
         * @brief 获取原始SQLite连接
         * @return SQLite连接指针
         */
        sqlite3* get() const { return m_db; }
        
        /**
         * @brief 检查连接是否有效
         * @return 是否有效
         */
        bool isValid() const;
        
        /**
         * @brief 执行SQL查询
         * @param sql SQL语句
         * @return 是否成功
         */
        bool execute(const std::string& sql);
        
        /**
         * @brief 准备SQL语句
         * @param sql SQL语句
         * @return 准备好的语句指针
         */
        sqlite3_stmt* prepare(const std::string& sql);
        
        /**
         * @brief 开始事务
         * @return 是否成功
         */
        bool beginTransaction();
        
        /**
         * @brief 提交事务
         * @return 是否成功
         */
        bool commitTransaction();
        
        /**
         * @brief 回滚事务
         * @return 是否成功
         */
        bool rollbackTransaction();
        
        /**
         * @brief 获取最后插入的行ID
         * @return 行ID
         */
        int64_t getLastInsertId() const;
        
        /**
         * @brief 获取错误消息
         * @return 错误消息
         */
        std::string getErrorMessage() const;

    private:
        sqlite3* m_db;
        ConnectionPool* m_pool;
        bool m_inTransaction;
    };

    /**
     * @brief 连接池统计信息
     */
    struct PoolStats {
        int totalConnections;               // 总连接数
        int activeConnections;              // 活跃连接数
        int idleConnections;                // 空闲连接数
        uint64_t totalRequests;             // 总请求数
        uint64_t successfulRequests;        // 成功请求数
        uint64_t failedRequests;            // 失败请求数
        double averageWaitTime;             // 平均等待时间(ms)
        std::chrono::system_clock::time_point startTime; // 启动时间
        
        PoolStats() : totalConnections(0), activeConnections(0), idleConnections(0),
                     totalRequests(0), successfulRequests(0), failedRequests(0),
                     averageWaitTime(0.0), startTime(std::chrono::system_clock::now()) {}
    };

private:
    /**
     * @brief 内部连接信息
     */
    struct ConnectionInfo {
        sqlite3* db;
        std::chrono::steady_clock::time_point lastUsed;
        bool inUse;
        
        ConnectionInfo(sqlite3* database) : db(database), 
                                          lastUsed(std::chrono::steady_clock::now()),
                                          inUse(false) {}
    };

public:
    /**
     * @brief 构造函数
     * @param config 连接池配置
     */
    explicit ConnectionPool(const PoolConfig& config = PoolConfig());

    /**
     * @brief 析构函数
     */
    ~ConnectionPool();

    /**
     * @brief 初始化连接池
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 关闭连接池
     */
    void shutdown();

    /**
     * @brief 获取数据库连接
     * @param timeoutMs 超时时间(毫秒)，0表示不超时
     * @return 数据库连接，如果获取失败返回nullptr
     */
    std::unique_ptr<Connection> getConnection(int timeoutMs = 0);

    /**
     * @brief 归还数据库连接
     * @param db SQLite连接指针
     */
    void returnConnection(sqlite3* db);

    /**
     * @brief 获取连接池统计信息
     * @return 统计信息
     */
    PoolStats getStats() const;

    /**
     * @brief 获取统计信息JSON字符串
     * @return JSON格式的统计信息
     */
    std::string getStatsJson() const;

    /**
     * @brief 检查连接池是否健康
     * @return 是否健康
     */
    bool isHealthy() const;

    /**
     * @brief 强制清理空闲连接
     */
    void cleanupIdleConnections();

    /**
     * @brief 设置连接池配置
     * @param config 新配置
     */
    void setConfig(const PoolConfig& config);

private:
    /**
     * @brief 创建新的数据库连接
     * @return SQLite连接指针，失败返回nullptr
     */
    sqlite3* createConnection();

    /**
     * @brief 配置数据库连接
     * @param db SQLite连接指针
     * @return 是否成功
     */
    bool configureConnection(sqlite3* db);

    /**
     * @brief 检查连接健康状态
     * @param db SQLite连接指针
     * @return 是否健康
     */
    bool isConnectionHealthy(sqlite3* db);

    /**
     * @brief 健康检查线程函数
     */
    void healthCheckThread();

    /**
     * @brief 清理过期的空闲连接
     */
    void cleanupExpiredConnections();

    /**
     * @brief 确保最小连接数
     */
    void ensureMinConnections();

    /**
     * @brief 更新统计信息
     * @param success 操作是否成功
     * @param waitTime 等待时间(ms)
     */
    void updateStats(bool success, double waitTime);

private:
    PoolConfig m_config;                                    // 连接池配置
    mutable std::mutex m_mutex;                             // 线程安全锁
    std::condition_variable m_condition;                    // 条件变量
    
    std::vector<std::unique_ptr<ConnectionInfo>> m_connections; // 连接列表
    std::queue<sqlite3*> m_availableConnections;           // 可用连接队列
    
    std::atomic<bool> m_initialized{false};                 // 是否已初始化
    std::atomic<bool> m_shutdown{false};                    // 是否已关闭
    
    std::unique_ptr<std::thread> m_healthCheckThread;      // 健康检查线程
    std::atomic<bool> m_healthCheckRunning{false};         // 健康检查是否运行
    
    mutable std::mutex m_statsMutex;                        // 统计信息锁
    PoolStats m_stats;                                      // 连接池统计
};

} // namespace AISecurityVision
