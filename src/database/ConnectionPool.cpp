#include "ConnectionPool.h"
#include <nlohmann/json.hpp>
#include <algorithm>

using namespace AISecurityVision;

// Connection implementation
ConnectionPool::Connection::Connection(sqlite3* db, ConnectionPool* pool)
    : m_db(db), m_pool(pool), m_inTransaction(false) {
}

ConnectionPool::Connection::~Connection() {
    if (m_inTransaction) {
        rollbackTransaction();
    }
    
    if (m_pool && m_db) {
        m_pool->returnConnection(m_db);
    }
}

ConnectionPool::Connection::Connection(Connection&& other) noexcept
    : m_db(other.m_db), m_pool(other.m_pool), m_inTransaction(other.m_inTransaction) {
    other.m_db = nullptr;
    other.m_pool = nullptr;
    other.m_inTransaction = false;
}

ConnectionPool::Connection& ConnectionPool::Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        if (m_inTransaction) {
            rollbackTransaction();
        }
        if (m_pool && m_db) {
            m_pool->returnConnection(m_db);
        }
        
        m_db = other.m_db;
        m_pool = other.m_pool;
        m_inTransaction = other.m_inTransaction;
        
        other.m_db = nullptr;
        other.m_pool = nullptr;
        other.m_inTransaction = false;
    }
    return *this;
}

bool ConnectionPool::Connection::isValid() const {
    if (!m_db) return false;
    
    // 简单的连接测试
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, "SELECT 1", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_finalize(stmt);
        return true;
    }
    return false;
}

bool ConnectionPool::Connection::execute(const std::string& sql) {
    if (!m_db) return false;
    
    char* errorMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);
    
    if (errorMsg) {
        LOG_ERROR() << "[ConnectionPool] SQL error: " << errorMsg;
        sqlite3_free(errorMsg);
    }
    
    return rc == SQLITE_OK;
}

sqlite3_stmt* ConnectionPool::Connection::prepare(const std::string& sql) {
    if (!m_db) return nullptr;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR() << "[ConnectionPool] Failed to prepare statement: " << sqlite3_errmsg(m_db);
        return nullptr;
    }
    
    return stmt;
}

bool ConnectionPool::Connection::beginTransaction() {
    if (m_inTransaction) return false;
    
    bool success = execute("BEGIN TRANSACTION");
    if (success) {
        m_inTransaction = true;
    }
    return success;
}

bool ConnectionPool::Connection::commitTransaction() {
    if (!m_inTransaction) return false;
    
    bool success = execute("COMMIT");
    if (success) {
        m_inTransaction = false;
    }
    return success;
}

bool ConnectionPool::Connection::rollbackTransaction() {
    if (!m_inTransaction) return false;
    
    bool success = execute("ROLLBACK");
    m_inTransaction = false; // 无论是否成功都重置状态
    return success;
}

int64_t ConnectionPool::Connection::getLastInsertId() const {
    if (!m_db) return -1;
    return sqlite3_last_insert_rowid(m_db);
}

std::string ConnectionPool::Connection::getErrorMessage() const {
    if (!m_db) return "No database connection";
    return sqlite3_errmsg(m_db);
}

// ConnectionPool implementation
ConnectionPool::ConnectionPool(const PoolConfig& config)
    : m_config(config) {
    LOG_INFO() << "[ConnectionPool] Initialized with config: "
               << "min=" << m_config.minConnections
               << ", max=" << m_config.maxConnections
               << ", db=" << m_config.dbPath;
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

bool ConnectionPool::initialize() {
    if (m_initialized) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 创建最小数量的连接
    for (int i = 0; i < m_config.minConnections; ++i) {
        sqlite3* db = createConnection();
        if (db) {
            m_connections.push_back(std::make_unique<ConnectionInfo>(db));
            m_availableConnections.push(db);
        } else {
            LOG_ERROR() << "[ConnectionPool] Failed to create initial connection " << i;
            return false;
        }
    }
    
    // 启动健康检查线程
    m_healthCheckRunning = true;
    m_healthCheckThread = std::make_unique<std::thread>(&ConnectionPool::healthCheckThread, this);
    
    m_initialized = true;
    m_stats.startTime = std::chrono::system_clock::now();
    
    LOG_INFO() << "[ConnectionPool] Initialized with " << m_connections.size() << " connections";
    return true;
}

void ConnectionPool::shutdown() {
    if (m_shutdown) {
        return;
    }
    
    m_shutdown = true;
    m_healthCheckRunning = false;
    m_condition.notify_all();
    
    if (m_healthCheckThread && m_healthCheckThread->joinable()) {
        m_healthCheckThread->join();
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 关闭所有连接
    for (auto& connInfo : m_connections) {
        if (connInfo->db) {
            sqlite3_close(connInfo->db);
        }
    }
    
    m_connections.clear();
    while (!m_availableConnections.empty()) {
        m_availableConnections.pop();
    }
    
    m_initialized = false;
    
    LOG_INFO() << "[ConnectionPool] Shutdown completed";
}

std::unique_ptr<ConnectionPool::Connection> ConnectionPool::getConnection(int timeoutMs) {
    if (!m_initialized || m_shutdown) {
        updateStats(false, 0);
        return nullptr;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // 等待可用连接
    if (timeoutMs > 0) {
        auto timeout = std::chrono::milliseconds(timeoutMs);
        if (!m_condition.wait_for(lock, timeout, [this] { return !m_availableConnections.empty() || m_shutdown; })) {
            auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            updateStats(false, waitTime);
            return nullptr;
        }
    } else {
        m_condition.wait(lock, [this] { return !m_availableConnections.empty() || m_shutdown; });
    }
    
    if (m_shutdown || m_availableConnections.empty()) {
        auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        updateStats(false, waitTime);
        return nullptr;
    }
    
    // 获取连接
    sqlite3* db = m_availableConnections.front();
    m_availableConnections.pop();
    
    // 更新连接信息
    for (auto& connInfo : m_connections) {
        if (connInfo->db == db) {
            connInfo->inUse = true;
            connInfo->lastUsed = std::chrono::steady_clock::now();
            break;
        }
    }
    
    auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    updateStats(true, waitTime);
    
    return std::make_unique<Connection>(db, this);
}

void ConnectionPool::returnConnection(sqlite3* db) {
    if (!db || m_shutdown) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 更新连接信息
    for (auto& connInfo : m_connections) {
        if (connInfo->db == db) {
            connInfo->inUse = false;
            connInfo->lastUsed = std::chrono::steady_clock::now();
            break;
        }
    }
    
    // 检查连接健康状态
    if (isConnectionHealthy(db)) {
        m_availableConnections.push(db);
        m_condition.notify_one();
    } else {
        // 连接不健康，关闭并创建新连接
        LOG_WARN() << "[ConnectionPool] Unhealthy connection detected, replacing...";
        
        // 从连接列表中移除
        m_connections.erase(
            std::remove_if(m_connections.begin(), m_connections.end(),
                [db](const auto& connInfo) { return connInfo->db == db; }),
            m_connections.end());
        
        sqlite3_close(db);
        
        // 创建新连接
        sqlite3* newDb = createConnection();
        if (newDb) {
            m_connections.push_back(std::make_unique<ConnectionInfo>(newDb));
            m_availableConnections.push(newDb);
            m_condition.notify_one();
        }
    }
}

ConnectionPool::PoolStats ConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    PoolStats stats = m_stats;
    
    // 更新实时统计
    std::lock_guard<std::mutex> poolLock(m_mutex);
    stats.totalConnections = m_connections.size();
    stats.idleConnections = m_availableConnections.size();
    stats.activeConnections = stats.totalConnections - stats.idleConnections;
    
    return stats;
}

std::string ConnectionPool::getStatsJson() const {
    auto stats = getStats();
    
    nlohmann::json json;
    json["total_connections"] = stats.totalConnections;
    json["active_connections"] = stats.activeConnections;
    json["idle_connections"] = stats.idleConnections;
    json["total_requests"] = stats.totalRequests;
    json["successful_requests"] = stats.successfulRequests;
    json["failed_requests"] = stats.failedRequests;
    json["average_wait_time_ms"] = stats.averageWaitTime;
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats.startTime).count();
    json["uptime_seconds"] = uptime;
    
    return json.dump();
}

bool ConnectionPool::isHealthy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized && !m_shutdown && !m_connections.empty();
}

void ConnectionPool::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    cleanupExpiredConnections();
}

void ConnectionPool::setConfig(const PoolConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    LOG_INFO() << "[ConnectionPool] Configuration updated";
}

sqlite3* ConnectionPool::createConnection() {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(m_config.dbPath.c_str(), &db);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR() << "[ConnectionPool] Failed to open database: " << sqlite3_errmsg(db);
        if (db) {
            sqlite3_close(db);
        }
        return nullptr;
    }
    
    if (!configureConnection(db)) {
        sqlite3_close(db);
        return nullptr;
    }
    
    return db;
}

bool ConnectionPool::configureConnection(sqlite3* db) {
    if (!db) return false;
    
    // 设置忙碌超时
    sqlite3_busy_timeout(db, m_config.busyTimeoutMs);
    
    // 启用WAL模式
    if (m_config.enableWALMode) {
        char* errorMsg = nullptr;
        int rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL", nullptr, nullptr, &errorMsg);
        if (rc != SQLITE_OK) {
            LOG_WARN() << "[ConnectionPool] Failed to enable WAL mode: " << (errorMsg ? errorMsg : "unknown error");
            if (errorMsg) sqlite3_free(errorMsg);
        }
    }
    
    // 启用外键约束
    if (m_config.enableForeignKeys) {
        char* errorMsg = nullptr;
        int rc = sqlite3_exec(db, "PRAGMA foreign_keys=ON", nullptr, nullptr, &errorMsg);
        if (rc != SQLITE_OK) {
            LOG_WARN() << "[ConnectionPool] Failed to enable foreign keys: " << (errorMsg ? errorMsg : "unknown error");
            if (errorMsg) sqlite3_free(errorMsg);
        }
    }
    
    return true;
}

bool ConnectionPool::isConnectionHealthy(sqlite3* db) {
    if (!db) return false;
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }
    
    return false;
}

void ConnectionPool::healthCheckThread() {
    while (m_healthCheckRunning && !m_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(m_config.healthCheckIntervalSeconds));
        
        if (!m_healthCheckRunning || m_shutdown) {
            break;
        }
        
        cleanupExpiredConnections();
        ensureMinConnections();
    }
}

void ConnectionPool::cleanupExpiredConnections() {
    auto now = std::chrono::steady_clock::now();
    auto maxIdleTime = std::chrono::seconds(m_config.idleTimeoutSeconds);
    
    auto it = m_connections.begin();
    while (it != m_connections.end()) {
        if (!(*it)->inUse && (now - (*it)->lastUsed) > maxIdleTime) {
            // 连接已过期，关闭它
            sqlite3_close((*it)->db);
            it = m_connections.erase(it);
            
            LOG_DEBUG() << "[ConnectionPool] Cleaned up expired connection";
        } else {
            ++it;
        }
    }
}

void ConnectionPool::ensureMinConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    while (m_connections.size() < static_cast<size_t>(m_config.minConnections)) {
        sqlite3* db = createConnection();
        if (db) {
            m_connections.push_back(std::make_unique<ConnectionInfo>(db));
            m_availableConnections.push(db);
            m_condition.notify_one();
        } else {
            break;
        }
    }
}

void ConnectionPool::updateStats(bool success, double waitTime) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_stats.totalRequests++;
    
    if (success) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }
    
    // 更新平均等待时间
    double totalWaitTime = m_stats.averageWaitTime * (m_stats.totalRequests - 1) + waitTime;
    m_stats.averageWaitTime = totalWaitTime / m_stats.totalRequests;
}
