#include "DatabaseManager.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <nlohmann/json.hpp>

#include "../core/Logger.h"
using namespace AISecurityVision;
DatabaseManager::DatabaseManager()
    : m_db(nullptr), m_insertEventStmt(nullptr), m_insertFaceStmt(nullptr),
      m_insertPlateStmt(nullptr), m_insertROIStmt(nullptr), m_selectEventsStmt(nullptr),
      m_selectFacesStmt(nullptr), m_selectPlatesStmt(nullptr), m_selectROIsStmt(nullptr),
      m_insertConfigStmt(nullptr), m_updateConfigStmt(nullptr), m_selectConfigStmt(nullptr),
      m_deleteConfigStmt(nullptr), m_insertCameraConfigStmt(nullptr), m_updateCameraConfigStmt(nullptr),
      m_selectCameraConfigStmt(nullptr), m_deleteCameraConfigStmt(nullptr),
      // User authentication statements (NEW - Phase 2)
      m_insertUserStmt(nullptr), m_selectUserByIdStmt(nullptr), m_selectUserByUsernameStmt(nullptr),
      m_updateUserStmt(nullptr), m_deleteUserStmt(nullptr), m_updateUserLastLoginStmt(nullptr),
      // Session management statements (NEW - Phase 2)
      m_insertSessionStmt(nullptr), m_selectSessionByIdStmt(nullptr), m_updateSessionStmt(nullptr),
      m_deleteSessionStmt(nullptr), m_deleteUserSessionsStmt(nullptr), m_deleteExpiredSessionsStmt(nullptr) {
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_dbPath = dbPath;

    // Open database
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = "Cannot open database: " + std::string(sqlite3_errmsg(m_db));
        LOG_ERROR() << "[DatabaseManager] " << m_lastError;
        return false;
    }

    // Enable foreign keys
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Create tables
    if (!createTables()) {
        LOG_ERROR() << "[DatabaseManager] Failed to create tables";
        return false;
    }

    // Prepare statements
    if (!prepareStatements()) {
        LOG_ERROR() << "[DatabaseManager] Failed to prepare statements";
        return false;
    }

    LOG_INFO() << "[DatabaseManager] Initialized with database: " << dbPath;
    return true;
}

void DatabaseManager::close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    finalizeStatements();

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool DatabaseManager::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_db != nullptr;
}

bool DatabaseManager::createTables() {
    const char* createEventsTable = R"(
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            camera_id TEXT NOT NULL,
            event_type TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            video_path TEXT,
            metadata TEXT,
            confidence REAL DEFAULT 0.0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    const char* createFacesTable = R"(
        CREATE TABLE IF NOT EXISTS faces (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            image_path TEXT,
            embedding BLOB,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    const char* createLicensePlatesTable = R"(
        CREATE TABLE IF NOT EXISTS license_plates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            plate_number TEXT NOT NULL,
            region TEXT,
            image_path TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    const char* createROIsTable = R"(
        CREATE TABLE IF NOT EXISTS rois (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            roi_id TEXT NOT NULL UNIQUE,
            camera_id TEXT NOT NULL,
            name TEXT NOT NULL,
            polygon_data TEXT NOT NULL,
            enabled BOOLEAN DEFAULT 1,
            priority INTEGER DEFAULT 1,
            start_time TEXT,
            end_time TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    const char* createConfigTable = R"(
        CREATE TABLE IF NOT EXISTS config (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category TEXT NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(category, key)
        );
    )";

    const char* createCameraConfigTable = R"(
        CREATE TABLE IF NOT EXISTS camera_config (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            camera_id TEXT NOT NULL UNIQUE,
            config_json TEXT NOT NULL,
            enabled BOOLEAN DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    // User authentication tables (NEW - Phase 2)
    const char* createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id TEXT NOT NULL UNIQUE,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            role TEXT DEFAULT 'user',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            last_login DATETIME,
            enabled BOOLEAN DEFAULT 1
        );
    )";

    const char* createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            session_id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            expires_at DATETIME NOT NULL,
            active BOOLEAN DEFAULT 1,
            FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
        );
    )";

    // Create indexes for better performance
    const char* createIndexes = R"(
        CREATE INDEX IF NOT EXISTS idx_events_camera_id ON events(camera_id);
        CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);
        CREATE INDEX IF NOT EXISTS idx_events_type ON events(event_type);
        CREATE INDEX IF NOT EXISTS idx_faces_name ON faces(name);
        CREATE INDEX IF NOT EXISTS idx_plates_number ON license_plates(plate_number);
        CREATE INDEX IF NOT EXISTS idx_rois_roi_id ON rois(roi_id);
        CREATE INDEX IF NOT EXISTS idx_rois_camera_id ON rois(camera_id);
        CREATE INDEX IF NOT EXISTS idx_rois_enabled ON rois(enabled);
        CREATE INDEX IF NOT EXISTS idx_rois_priority ON rois(priority);
        CREATE INDEX IF NOT EXISTS idx_config_category ON config(category);
        CREATE INDEX IF NOT EXISTS idx_config_key ON config(key);
        CREATE INDEX IF NOT EXISTS idx_camera_config_camera_id ON camera_config(camera_id);
        CREATE INDEX IF NOT EXISTS idx_camera_config_enabled ON camera_config(enabled);
        CREATE INDEX IF NOT EXISTS idx_users_user_id ON users(user_id);
        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
        CREATE INDEX IF NOT EXISTS idx_users_enabled ON users(enabled);
        CREATE INDEX IF NOT EXISTS idx_sessions_user_id ON sessions(user_id);
        CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);
        CREATE INDEX IF NOT EXISTS idx_sessions_active ON sessions(active);
    )";

    char* errMsg = nullptr;

    // Execute table creation
    if (sqlite3_exec(m_db, createEventsTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create events table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createFacesTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create faces table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createLicensePlatesTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create license_plates table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createROIsTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create rois table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createConfigTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create config table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createCameraConfigTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create camera_config table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    // Create user authentication tables (NEW - Phase 2)
    if (sqlite3_exec(m_db, createUsersTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create users table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createSessionsTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create sessions table: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createIndexes, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        m_lastError = "Failed to create indexes: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DatabaseManager::prepareStatements() {
    // Prepare insert event statement
    const char* insertEventSql = R"(
        INSERT INTO events (camera_id, event_type, timestamp, video_path, metadata, confidence)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertEventSql, -1, &m_insertEventStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert event statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare insert face statement
    const char* insertFaceSql = R"(
        INSERT INTO faces (name, image_path, embedding, created_at)
        VALUES (?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertFaceSql, -1, &m_insertFaceStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert face statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare insert license plate statement
    const char* insertPlateSql = R"(
        INSERT INTO license_plates (plate_number, region, image_path, created_at)
        VALUES (?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertPlateSql, -1, &m_insertPlateStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert plate statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare insert ROI statement
    const char* insertROISql = R"(
        INSERT INTO rois (roi_id, camera_id, name, polygon_data, enabled, priority, start_time, end_time, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertROISql, -1, &m_insertROIStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert ROI statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare configuration statements
    const char* insertConfigSql = R"(
        INSERT OR REPLACE INTO config (category, key, value, updated_at)
        VALUES (?, ?, ?, datetime('now'));
    )";

    if (sqlite3_prepare_v2(m_db, insertConfigSql, -1, &m_insertConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* selectConfigSql = R"(
        SELECT value FROM config WHERE category = ? AND key = ?;
    )";

    if (sqlite3_prepare_v2(m_db, selectConfigSql, -1, &m_selectConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteConfigSql = R"(
        DELETE FROM config WHERE category = ? AND (? = '' OR key = ?);
    )";

    if (sqlite3_prepare_v2(m_db, deleteConfigSql, -1, &m_deleteConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare camera configuration statements
    const char* insertCameraConfigSql = R"(
        INSERT OR REPLACE INTO camera_config (camera_id, config_json, enabled, updated_at)
        VALUES (?, ?, ?, datetime('now'));
    )";

    if (sqlite3_prepare_v2(m_db, insertCameraConfigSql, -1, &m_insertCameraConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert camera config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* selectCameraConfigSql = R"(
        SELECT config_json FROM camera_config WHERE camera_id = ? AND enabled = 1;
    )";

    if (sqlite3_prepare_v2(m_db, selectCameraConfigSql, -1, &m_selectCameraConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select camera config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteCameraConfigSql = R"(
        DELETE FROM camera_config WHERE camera_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, deleteCameraConfigSql, -1, &m_deleteCameraConfigStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete camera config statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare user authentication statements (NEW - Phase 2)
    const char* insertUserSql = R"(
        INSERT INTO users (user_id, username, password_hash, role, created_at, enabled)
        VALUES (?, ?, ?, ?, datetime('now'), ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertUserSql, -1, &m_insertUserStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert user statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* selectUserByIdSql = R"(
        SELECT id, user_id, username, password_hash, role, created_at, last_login, enabled
        FROM users WHERE user_id = ? AND enabled = 1;
    )";

    if (sqlite3_prepare_v2(m_db, selectUserByIdSql, -1, &m_selectUserByIdStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select user by id statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* selectUserByUsernameSql = R"(
        SELECT id, user_id, username, password_hash, role, created_at, last_login, enabled
        FROM users WHERE username = ? AND enabled = 1;
    )";

    if (sqlite3_prepare_v2(m_db, selectUserByUsernameSql, -1, &m_selectUserByUsernameStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select user by username statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* updateUserSql = R"(
        UPDATE users SET username = ?, password_hash = ?, role = ?, enabled = ? WHERE user_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, updateUserSql, -1, &m_updateUserStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare update user statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteUserSql = R"(
        UPDATE users SET enabled = 0 WHERE user_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, deleteUserSql, -1, &m_deleteUserStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete user statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* updateUserLastLoginSql = R"(
        UPDATE users SET last_login = datetime('now') WHERE user_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, updateUserLastLoginSql, -1, &m_updateUserLastLoginStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare update user last login statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    // Prepare session management statements (NEW - Phase 2)
    const char* insertSessionSql = R"(
        INSERT INTO sessions (session_id, user_id, created_at, expires_at, active)
        VALUES (?, ?, datetime('now'), ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, insertSessionSql, -1, &m_insertSessionStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare insert session statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* selectSessionByIdSql = R"(
        SELECT session_id, user_id, created_at, expires_at, active
        FROM sessions WHERE session_id = ? AND active = 1 AND expires_at > datetime('now');
    )";

    if (sqlite3_prepare_v2(m_db, selectSessionByIdSql, -1, &m_selectSessionByIdStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select session by id statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* updateSessionSql = R"(
        UPDATE sessions SET expires_at = ?, active = ? WHERE session_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, updateSessionSql, -1, &m_updateSessionStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare update session statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteSessionSql = R"(
        UPDATE sessions SET active = 0 WHERE session_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, deleteSessionSql, -1, &m_deleteSessionStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete session statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteUserSessionsSql = R"(
        UPDATE sessions SET active = 0 WHERE user_id = ?;
    )";

    if (sqlite3_prepare_v2(m_db, deleteUserSessionsSql, -1, &m_deleteUserSessionsStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete user sessions statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    const char* deleteExpiredSessionsSql = R"(
        UPDATE sessions SET active = 0 WHERE expires_at <= datetime('now');
    )";

    if (sqlite3_prepare_v2(m_db, deleteExpiredSessionsSql, -1, &m_deleteExpiredSessionsStmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete expired sessions statement: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

void DatabaseManager::finalizeStatements() {
    if (m_insertEventStmt) {
        sqlite3_finalize(m_insertEventStmt);
        m_insertEventStmt = nullptr;
    }
    if (m_insertFaceStmt) {
        sqlite3_finalize(m_insertFaceStmt);
        m_insertFaceStmt = nullptr;
    }
    if (m_insertPlateStmt) {
        sqlite3_finalize(m_insertPlateStmt);
        m_insertPlateStmt = nullptr;
    }
    if (m_insertROIStmt) {
        sqlite3_finalize(m_insertROIStmt);
        m_insertROIStmt = nullptr;
    }
    if (m_selectEventsStmt) {
        sqlite3_finalize(m_selectEventsStmt);
        m_selectEventsStmt = nullptr;
    }
    if (m_selectFacesStmt) {
        sqlite3_finalize(m_selectFacesStmt);
        m_selectFacesStmt = nullptr;
    }
    if (m_selectPlatesStmt) {
        sqlite3_finalize(m_selectPlatesStmt);
        m_selectPlatesStmt = nullptr;
    }
    if (m_selectROIsStmt) {
        sqlite3_finalize(m_selectROIsStmt);
        m_selectROIsStmt = nullptr;
    }

    // Finalize configuration statements
    if (m_insertConfigStmt) {
        sqlite3_finalize(m_insertConfigStmt);
        m_insertConfigStmt = nullptr;
    }
    if (m_updateConfigStmt) {
        sqlite3_finalize(m_updateConfigStmt);
        m_updateConfigStmt = nullptr;
    }
    if (m_selectConfigStmt) {
        sqlite3_finalize(m_selectConfigStmt);
        m_selectConfigStmt = nullptr;
    }
    if (m_deleteConfigStmt) {
        sqlite3_finalize(m_deleteConfigStmt);
        m_deleteConfigStmt = nullptr;
    }
    if (m_insertCameraConfigStmt) {
        sqlite3_finalize(m_insertCameraConfigStmt);
        m_insertCameraConfigStmt = nullptr;
    }
    if (m_updateCameraConfigStmt) {
        sqlite3_finalize(m_updateCameraConfigStmt);
        m_updateCameraConfigStmt = nullptr;
    }
    if (m_selectCameraConfigStmt) {
        sqlite3_finalize(m_selectCameraConfigStmt);
        m_selectCameraConfigStmt = nullptr;
    }
    if (m_deleteCameraConfigStmt) {
        sqlite3_finalize(m_deleteCameraConfigStmt);
        m_deleteCameraConfigStmt = nullptr;
    }

    // Finalize user authentication statements (NEW - Phase 2)
    if (m_insertUserStmt) {
        sqlite3_finalize(m_insertUserStmt);
        m_insertUserStmt = nullptr;
    }
    if (m_selectUserByIdStmt) {
        sqlite3_finalize(m_selectUserByIdStmt);
        m_selectUserByIdStmt = nullptr;
    }
    if (m_selectUserByUsernameStmt) {
        sqlite3_finalize(m_selectUserByUsernameStmt);
        m_selectUserByUsernameStmt = nullptr;
    }
    if (m_updateUserStmt) {
        sqlite3_finalize(m_updateUserStmt);
        m_updateUserStmt = nullptr;
    }
    if (m_deleteUserStmt) {
        sqlite3_finalize(m_deleteUserStmt);
        m_deleteUserStmt = nullptr;
    }
    if (m_updateUserLastLoginStmt) {
        sqlite3_finalize(m_updateUserLastLoginStmt);
        m_updateUserLastLoginStmt = nullptr;
    }

    // Finalize session management statements (NEW - Phase 2)
    if (m_insertSessionStmt) {
        sqlite3_finalize(m_insertSessionStmt);
        m_insertSessionStmt = nullptr;
    }
    if (m_selectSessionByIdStmt) {
        sqlite3_finalize(m_selectSessionByIdStmt);
        m_selectSessionByIdStmt = nullptr;
    }
    if (m_updateSessionStmt) {
        sqlite3_finalize(m_updateSessionStmt);
        m_updateSessionStmt = nullptr;
    }
    if (m_deleteSessionStmt) {
        sqlite3_finalize(m_deleteSessionStmt);
        m_deleteSessionStmt = nullptr;
    }
    if (m_deleteUserSessionsStmt) {
        sqlite3_finalize(m_deleteUserSessionsStmt);
        m_deleteUserSessionsStmt = nullptr;
    }
    if (m_deleteExpiredSessionsStmt) {
        sqlite3_finalize(m_deleteExpiredSessionsStmt);
        m_deleteExpiredSessionsStmt = nullptr;
    }
}

bool DatabaseManager::insertEvent(const EventRecord& event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertEventStmt) {
        m_lastError = "Insert event statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertEventStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertEventStmt, 1, event.camera_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertEventStmt, 2, event.event_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertEventStmt, 3, event.timestamp.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertEventStmt, 4, event.video_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertEventStmt, 5, event.metadata.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(m_insertEventStmt, 6, event.confidence);

    // Execute
    int rc = sqlite3_step(m_insertEventStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert event: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<EventRecord> DatabaseManager::getEvents(const std::string& cameraId,
                                                   const std::string& eventType,
                                                   int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<EventRecord> events;

    // Build query dynamically based on filters
    std::string query = "SELECT id, camera_id, event_type, timestamp, video_path, metadata, confidence FROM events WHERE 1=1";

    if (!cameraId.empty()) {
        query += " AND camera_id = '" + cameraId + "'";
    }
    if (!eventType.empty()) {
        query += " AND event_type = '" + eventType + "'";
    }

    query += " ORDER BY timestamp DESC LIMIT " + std::to_string(limit);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select events query: " + std::string(sqlite3_errmsg(m_db));
        return events;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EventRecord event;
        event.id = sqlite3_column_int(stmt, 0);
        event.camera_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        event.event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        event.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        const char* videoPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (videoPath) event.video_path = videoPath;

        const char* metadata = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (metadata) event.metadata = metadata;

        event.confidence = sqlite3_column_double(stmt, 6);

        events.push_back(event);
    }

    sqlite3_finalize(stmt);
    return events;
}

std::string DatabaseManager::vectorToBlob(const std::vector<float>& vec) {
    std::string blob;
    blob.resize(vec.size() * sizeof(float));
    std::memcpy(&blob[0], vec.data(), blob.size());
    return blob;
}

std::vector<float> DatabaseManager::blobToVector(const void* blob, int size) {
    std::vector<float> vec;
    if (blob && size > 0) {
        vec.resize(size / sizeof(float));
        std::memcpy(vec.data(), blob, size);
    }
    return vec;
}

int DatabaseManager::getLastInsertId() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

std::string DatabaseManager::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

bool DatabaseManager::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(m_mutex);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, query.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        m_lastError = "Query execution failed: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DatabaseManager::deleteEvent(int eventId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string query = "DELETE FROM events WHERE id = " + std::to_string(eventId);
    return executeQuery(query);
}

bool DatabaseManager::deleteOldEvents(int daysOld) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string query = "DELETE FROM events WHERE timestamp < datetime('now', '-" +
                       std::to_string(daysOld) + " days')";
    return executeQuery(query);
}

bool DatabaseManager::insertFace(const FaceRecord& face) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertFaceStmt) {
        m_lastError = "Insert face statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertFaceStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertFaceStmt, 1, face.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertFaceStmt, 2, face.image_path.c_str(), -1, SQLITE_STATIC);

    // Convert embedding vector to blob
    if (!face.embedding.empty()) {
        std::string blob = vectorToBlob(face.embedding);
        sqlite3_bind_blob(m_insertFaceStmt, 3, blob.data(), blob.size(), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(m_insertFaceStmt, 3);
    }

    sqlite3_bind_text(m_insertFaceStmt, 4, face.created_at.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_insertFaceStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert face: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<FaceRecord> DatabaseManager::getFaces() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FaceRecord> faces;

    const char* query = "SELECT id, name, image_path, embedding, created_at FROM faces ORDER BY created_at DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select faces query: " + std::string(sqlite3_errmsg(m_db));
        return faces;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FaceRecord face;
        face.id = sqlite3_column_int(stmt, 0);
        face.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (imagePath) face.image_path = imagePath;

        // Get embedding blob
        const void* blob = sqlite3_column_blob(stmt, 3);
        int blobSize = sqlite3_column_bytes(stmt, 3);
        if (blob && blobSize > 0) {
            face.embedding = blobToVector(blob, blobSize);
        }

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (createdAt) face.created_at = createdAt;

        faces.push_back(face);
    }

    sqlite3_finalize(stmt);
    return faces;
}

FaceRecord DatabaseManager::getFaceById(int faceId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FaceRecord face;

    const char* query = "SELECT id, name, image_path, embedding, created_at FROM faces WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select face by id query: " + std::string(sqlite3_errmsg(m_db));
        return face;
    }

    sqlite3_bind_int(stmt, 1, faceId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        face.id = sqlite3_column_int(stmt, 0);
        face.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (imagePath) face.image_path = imagePath;

        // Get embedding blob
        const void* blob = sqlite3_column_blob(stmt, 3);
        int blobSize = sqlite3_column_bytes(stmt, 3);
        if (blob && blobSize > 0) {
            face.embedding = blobToVector(blob, blobSize);
        }

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (createdAt) face.created_at = createdAt;
    }

    sqlite3_finalize(stmt);
    return face;
}

FaceRecord DatabaseManager::getFaceByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FaceRecord face;

    const char* query = "SELECT id, name, image_path, embedding, created_at FROM faces WHERE name = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select face by name query: " + std::string(sqlite3_errmsg(m_db));
        return face;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        face.id = sqlite3_column_int(stmt, 0);
        face.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (imagePath) face.image_path = imagePath;

        // Get embedding blob
        const void* blob = sqlite3_column_blob(stmt, 3);
        int blobSize = sqlite3_column_bytes(stmt, 3);
        if (blob && blobSize > 0) {
            face.embedding = blobToVector(blob, blobSize);
        }

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (createdAt) face.created_at = createdAt;
    }

    sqlite3_finalize(stmt);
    return face;
}

bool DatabaseManager::updateFace(const FaceRecord& face) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* query = "UPDATE faces SET name = ?, image_path = ?, embedding = ? WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare update face query: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, face.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, face.image_path.c_str(), -1, SQLITE_STATIC);

    if (!face.embedding.empty()) {
        std::string blob = vectorToBlob(face.embedding);
        sqlite3_bind_blob(stmt, 3, blob.data(), blob.size(), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 3);
    }

    sqlite3_bind_int(stmt, 4, face.id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to update face: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteFace(int faceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string query = "DELETE FROM faces WHERE id = " + std::to_string(faceId);
    return executeQuery(query);
}

bool DatabaseManager::insertLicensePlate(const LicensePlateRecord& plate) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertPlateStmt) {
        m_lastError = "Insert license plate statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertPlateStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertPlateStmt, 1, plate.plate_number.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertPlateStmt, 2, plate.region.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertPlateStmt, 3, plate.image_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertPlateStmt, 4, plate.created_at.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_insertPlateStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert license plate: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<LicensePlateRecord> DatabaseManager::getLicensePlates() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LicensePlateRecord> plates;

    const char* query = "SELECT id, plate_number, region, image_path, created_at FROM license_plates ORDER BY created_at DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select license plates query: " + std::string(sqlite3_errmsg(m_db));
        return plates;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LicensePlateRecord plate;
        plate.id = sqlite3_column_int(stmt, 0);
        plate.plate_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* region = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (region) plate.region = region;

        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (imagePath) plate.image_path = imagePath;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (createdAt) plate.created_at = createdAt;

        plates.push_back(plate);
    }

    sqlite3_finalize(stmt);
    return plates;
}

LicensePlateRecord DatabaseManager::getLicensePlateById(int plateId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    LicensePlateRecord plate;

    const char* query = "SELECT id, plate_number, region, image_path, created_at FROM license_plates WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select license plate by id query: " + std::string(sqlite3_errmsg(m_db));
        return plate;
    }

    sqlite3_bind_int(stmt, 1, plateId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        plate.id = sqlite3_column_int(stmt, 0);
        plate.plate_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* region = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (region) plate.region = region;

        const char* imagePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (imagePath) plate.image_path = imagePath;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (createdAt) plate.created_at = createdAt;
    }

    sqlite3_finalize(stmt);
    return plate;
}

bool DatabaseManager::deleteLicensePlate(int plateId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string query = "DELETE FROM license_plates WHERE id = " + std::to_string(plateId);
    return executeQuery(query);
}

// ROI operations implementation
bool DatabaseManager::insertROI(const ROIRecord& roi) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertROIStmt) {
        m_lastError = "Insert ROI statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertROIStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertROIStmt, 1, roi.roi_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertROIStmt, 2, roi.camera_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertROIStmt, 3, roi.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertROIStmt, 4, roi.polygon_data.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_insertROIStmt, 5, roi.enabled ? 1 : 0);
    sqlite3_bind_int(m_insertROIStmt, 6, roi.priority);

    // Bind time fields (can be NULL)
    if (!roi.start_time.empty()) {
        sqlite3_bind_text(m_insertROIStmt, 7, roi.start_time.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(m_insertROIStmt, 7);
    }

    if (!roi.end_time.empty()) {
        sqlite3_bind_text(m_insertROIStmt, 8, roi.end_time.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(m_insertROIStmt, 8);
    }

    sqlite3_bind_text(m_insertROIStmt, 9, roi.created_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertROIStmt, 10, roi.updated_at.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_insertROIStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert ROI: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<ROIRecord> DatabaseManager::getROIs(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ROIRecord> rois;

    std::string query = "SELECT id, roi_id, camera_id, name, polygon_data, enabled, priority, start_time, end_time, created_at, updated_at FROM rois";

    if (!cameraId.empty()) {
        query += " WHERE camera_id = '" + cameraId + "'";
    }

    query += " ORDER BY priority DESC, created_at ASC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select ROIs query: " + std::string(sqlite3_errmsg(m_db));
        return rois;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ROIRecord roi;
        roi.id = sqlite3_column_int(stmt, 0);
        roi.roi_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        roi.camera_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        roi.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        roi.polygon_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        roi.enabled = sqlite3_column_int(stmt, 5) != 0;
        roi.priority = sqlite3_column_int(stmt, 6);

        const char* startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (startTime) roi.start_time = startTime;

        const char* endTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (endTime) roi.end_time = endTime;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if (createdAt) roi.created_at = createdAt;

        const char* updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        if (updatedAt) roi.updated_at = updatedAt;

        rois.push_back(roi);
    }

    sqlite3_finalize(stmt);
    return rois;
}

ROIRecord DatabaseManager::getROIById(const std::string& roiId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ROIRecord roi;

    const char* query = "SELECT id, roi_id, camera_id, name, polygon_data, enabled, priority, start_time, end_time, created_at, updated_at FROM rois WHERE roi_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select ROI by ID query: " + std::string(sqlite3_errmsg(m_db));
        return roi;
    }

    sqlite3_bind_text(stmt, 1, roiId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        roi.id = sqlite3_column_int(stmt, 0);
        roi.roi_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        roi.camera_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        roi.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        roi.polygon_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        roi.enabled = sqlite3_column_int(stmt, 5) != 0;
        roi.priority = sqlite3_column_int(stmt, 6);

        const char* startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (startTime) roi.start_time = startTime;

        const char* endTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (endTime) roi.end_time = endTime;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if (createdAt) roi.created_at = createdAt;

        const char* updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        if (updatedAt) roi.updated_at = updatedAt;
    }

    sqlite3_finalize(stmt);
    return roi;
}

ROIRecord DatabaseManager::getROIByDatabaseId(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ROIRecord roi;

    const char* query = "SELECT id, roi_id, camera_id, name, polygon_data, enabled, priority, start_time, end_time, created_at, updated_at FROM rois WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select ROI by database ID query: " + std::string(sqlite3_errmsg(m_db));
        return roi;
    }

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        roi.id = sqlite3_column_int(stmt, 0);
        roi.roi_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        roi.camera_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        roi.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        roi.polygon_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        roi.enabled = sqlite3_column_int(stmt, 5) != 0;
        roi.priority = sqlite3_column_int(stmt, 6);

        const char* startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (startTime) roi.start_time = startTime;

        const char* endTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if (endTime) roi.end_time = endTime;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if (createdAt) roi.created_at = createdAt;

        const char* updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        if (updatedAt) roi.updated_at = updatedAt;
    }

    sqlite3_finalize(stmt);
    return roi;
}

bool DatabaseManager::updateROI(const ROIRecord& roi) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* query = "UPDATE rois SET camera_id = ?, name = ?, polygon_data = ?, enabled = ?, priority = ?, start_time = ?, end_time = ?, updated_at = ? WHERE roi_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare update ROI query: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, roi.camera_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, roi.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, roi.polygon_data.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, roi.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 5, roi.priority);

    // Bind time fields (can be NULL)
    if (!roi.start_time.empty()) {
        sqlite3_bind_text(stmt, 6, roi.start_time.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 6);
    }

    if (!roi.end_time.empty()) {
        sqlite3_bind_text(stmt, 7, roi.end_time.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 7);
    }

    sqlite3_bind_text(stmt, 8, roi.updated_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, roi.roi_id.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to update ROI: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteROI(const std::string& roiId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* query = "DELETE FROM rois WHERE roi_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete ROI query: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, roiId.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete ROI: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteROIsByCameraId(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* query = "DELETE FROM rois WHERE camera_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare delete ROIs by camera ID query: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, cameraId.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete ROIs by camera ID: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

// Transaction support implementation - Task 72
bool DatabaseManager::beginTransaction() {
    std::lock_guard<std::mutex> lock(m_mutex);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        m_lastError = "Failed to begin transaction: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DatabaseManager::commitTransaction() {
    std::lock_guard<std::mutex> lock(m_mutex);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        m_lastError = "Failed to commit transaction: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DatabaseManager::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(m_mutex);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        m_lastError = "Failed to rollback transaction: " + std::string(errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// Bulk ROI operations implementation - Task 72
bool DatabaseManager::insertROIsBulk(const std::vector<ROIRecord>& rois) {
    if (rois.empty()) {
        return true;  // Nothing to insert
    }

    // Note: Transaction management is handled by the caller
    // This allows for better error handling and rollback control

    for (const auto& roi : rois) {
        if (!insertROI(roi)) {
            // Error message is already set by insertROI
            return false;
        }
    }

    return true;
}

bool DatabaseManager::updateROIsBulk(const std::vector<ROIRecord>& rois) {
    if (rois.empty()) {
        return true;  // Nothing to update
    }

    // Note: Transaction management is handled by the caller
    // This allows for better error handling and rollback control

    for (const auto& roi : rois) {
        if (!updateROI(roi)) {
            // Error message is already set by updateROI
            return false;
        }
    }

    return true;
}

bool DatabaseManager::deleteROIsBulk(const std::vector<std::string>& roiIds) {
    if (roiIds.empty()) {
        return true;  // Nothing to delete
    }

    // Note: Transaction management is handled by the caller
    // This allows for better error handling and rollback control

    for (const auto& roiId : roiIds) {
        if (!deleteROI(roiId)) {
            // Error message is already set by deleteROI
            return false;
        }
    }

    return true;
}

// Configuration operations implementation
bool DatabaseManager::saveConfig(const std::string& category, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertConfigStmt) {
        m_lastError = "Insert config statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertConfigStmt, 1, category.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertConfigStmt, 2, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertConfigStmt, 3, value.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_insertConfigStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to save config: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::string DatabaseManager::getConfig(const std::string& category, const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_selectConfigStmt) {
        m_lastError = "Select config statement not prepared";
        return defaultValue;
    }

    // Reset statement
    sqlite3_reset(m_selectConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_selectConfigStmt, 1, category.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_selectConfigStmt, 2, key.c_str(), -1, SQLITE_STATIC);

    // Execute
    if (sqlite3_step(m_selectConfigStmt) == SQLITE_ROW) {
        const char* value = reinterpret_cast<const char*>(sqlite3_column_text(m_selectConfigStmt, 0));
        if (value) {
            return std::string(value);
        }
    }

    return defaultValue;
}

bool DatabaseManager::deleteConfig(const std::string& category, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteConfigStmt) {
        m_lastError = "Delete config statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_deleteConfigStmt, 1, category.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_deleteConfigStmt, 2, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_deleteConfigStmt, 3, key.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_deleteConfigStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete config: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::map<std::string, std::string> DatabaseManager::getAllConfigs(const std::string& category) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::map<std::string, std::string> configs;

    std::string query = "SELECT key, value FROM config";
    if (!category.empty()) {
        query += " WHERE category = '" + category + "'";
    }
    query += " ORDER BY category, key";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select all configs query: " + std::string(sqlite3_errmsg(m_db));
        return configs;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        if (key && value) {
            configs[std::string(key)] = std::string(value);
        }
    }

    sqlite3_finalize(stmt);
    return configs;
}

// Camera configuration operations implementation
bool DatabaseManager::saveCameraConfig(const std::string& cameraId, const std::string& configJson) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertCameraConfigStmt) {
        m_lastError = "Insert camera config statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertCameraConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertCameraConfigStmt, 1, cameraId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertCameraConfigStmt, 2, configJson.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_insertCameraConfigStmt, 3, 1); // enabled = true

    // Execute
    int rc = sqlite3_step(m_insertCameraConfigStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to save camera config: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::string DatabaseManager::getCameraConfig(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_selectCameraConfigStmt) {
        m_lastError = "Select camera config statement not prepared";
        return "";
    }

    // Reset statement
    sqlite3_reset(m_selectCameraConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_selectCameraConfigStmt, 1, cameraId.c_str(), -1, SQLITE_STATIC);

    // Execute
    if (sqlite3_step(m_selectCameraConfigStmt) == SQLITE_ROW) {
        const char* configJson = reinterpret_cast<const char*>(sqlite3_column_text(m_selectCameraConfigStmt, 0));
        if (configJson) {
            return std::string(configJson);
        }
    }

    return "";
}

std::vector<std::string> DatabaseManager::getAllCameraIds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cameraIds;

    const char* query = "SELECT camera_id FROM camera_config WHERE enabled = 1 ORDER BY camera_id";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select camera ids query: " + std::string(sqlite3_errmsg(m_db));
        return cameraIds;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* cameraId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (cameraId) {
            cameraIds.push_back(std::string(cameraId));
        }
    }

    sqlite3_finalize(stmt);
    return cameraIds;
}

bool DatabaseManager::deleteCameraConfig(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteCameraConfigStmt) {
        m_lastError = "Delete camera config statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteCameraConfigStmt);

    // Bind parameters
    sqlite3_bind_text(m_deleteCameraConfigStmt, 1, cameraId.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_deleteCameraConfigStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete camera config: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

// Detection category configuration operations implementation
bool DatabaseManager::saveDetectionCategories(const std::vector<std::string>& enabledCategories) {
    try {
        // Convert vector to JSON array
        nlohmann::json categoriesJson = enabledCategories;
        std::string categoriesStr = categoriesJson.dump();

        // Save to config table
        bool result = saveConfig("detection_categories", "enabled_classes", categoriesStr);

        if (result) {
            LOG_INFO() << "[DatabaseManager] Saved " << enabledCategories.size()
                       << " enabled detection categories";
        } else {
            LOG_ERROR() << "[DatabaseManager] Failed to save detection categories";
        }

        return result;
    } catch (const std::exception& e) {
        LOG_ERROR() << "[DatabaseManager] Exception saving detection categories: " << e.what();
        return false;
    }
}

std::vector<std::string> DatabaseManager::getDetectionCategories() {
    std::vector<std::string> categories;

    try {
        // Get from config table
        std::string categoriesStr = getConfig("detection_categories", "enabled_classes", "");

        if (categoriesStr.empty()) {
            // Return default categories if none saved
            LOG_INFO() << "[DatabaseManager] No saved detection categories, returning defaults";
            return {"person", "car", "truck", "bicycle", "motorcycle", "bus"};
        }

        // Parse JSON array
        nlohmann::json categoriesJson = nlohmann::json::parse(categoriesStr);

        if (categoriesJson.is_array()) {
            for (const auto& category : categoriesJson) {
                if (category.is_string()) {
                    categories.push_back(category.get<std::string>());
                }
            }
        }

        LOG_INFO() << "[DatabaseManager] Loaded " << categories.size()
                   << " enabled detection categories";

    } catch (const std::exception& e) {
        LOG_ERROR() << "[DatabaseManager] Exception loading detection categories: " << e.what();
        // Return default categories on error
        return {"person", "car", "truck", "bicycle", "motorcycle", "bus"};
    }

    return categories;
}

bool DatabaseManager::resetDetectionCategories() {
    bool result = deleteConfig("detection_categories", "enabled_classes");

    if (result) {
        LOG_INFO() << "[DatabaseManager] Reset detection categories to defaults";
    } else {
        LOG_ERROR() << "[DatabaseManager] Failed to reset detection categories";
    }

    return result;
}

// User authentication operations implementation (NEW - Phase 2)
bool DatabaseManager::insertUser(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertUserStmt) {
        m_lastError = "Insert user statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertUserStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertUserStmt, 1, user.user_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertUserStmt, 2, user.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertUserStmt, 3, user.password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertUserStmt, 4, user.role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_insertUserStmt, 5, user.enabled ? 1 : 0);

    // Execute
    int rc = sqlite3_step(m_insertUserStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert user: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

UserRecord DatabaseManager::getUserById(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    UserRecord user;

    if (!m_selectUserByIdStmt) {
        m_lastError = "Select user by id statement not prepared";
        return user;
    }

    // Reset statement
    sqlite3_reset(m_selectUserByIdStmt);

    // Bind parameters
    sqlite3_bind_text(m_selectUserByIdStmt, 1, userId.c_str(), -1, SQLITE_STATIC);

    // Execute
    if (sqlite3_step(m_selectUserByIdStmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(m_selectUserByIdStmt, 0);
        user.user_id = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 1));
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 2));
        user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 3));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 4));

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 5));
        if (createdAt) user.created_at = createdAt;

        const char* lastLogin = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByIdStmt, 6));
        if (lastLogin) user.last_login = lastLogin;

        user.enabled = sqlite3_column_int(m_selectUserByIdStmt, 7) == 1;
    }

    return user;
}

UserRecord DatabaseManager::getUserByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    UserRecord user;

    if (!m_selectUserByUsernameStmt) {
        m_lastError = "Select user by username statement not prepared";
        return user;
    }

    // Reset statement
    sqlite3_reset(m_selectUserByUsernameStmt);

    // Bind parameters
    sqlite3_bind_text(m_selectUserByUsernameStmt, 1, username.c_str(), -1, SQLITE_STATIC);

    // Execute
    if (sqlite3_step(m_selectUserByUsernameStmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(m_selectUserByUsernameStmt, 0);
        user.user_id = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 1));
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 2));
        user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 3));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 4));

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 5));
        if (createdAt) user.created_at = createdAt;

        const char* lastLogin = reinterpret_cast<const char*>(sqlite3_column_text(m_selectUserByUsernameStmt, 6));
        if (lastLogin) user.last_login = lastLogin;

        user.enabled = sqlite3_column_int(m_selectUserByUsernameStmt, 7) == 1;
    }

    return user;
}

bool DatabaseManager::updateUser(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_updateUserStmt) {
        m_lastError = "Update user statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_updateUserStmt);

    // Bind parameters
    sqlite3_bind_text(m_updateUserStmt, 1, user.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_updateUserStmt, 2, user.password_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_updateUserStmt, 3, user.role.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_updateUserStmt, 4, user.enabled ? 1 : 0);
    sqlite3_bind_text(m_updateUserStmt, 5, user.user_id.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_updateUserStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to update user: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteUser(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteUserStmt) {
        m_lastError = "Delete user statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteUserStmt);

    // Bind parameters
    sqlite3_bind_text(m_deleteUserStmt, 1, userId.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_deleteUserStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete user: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::updateUserLastLogin(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_updateUserLastLoginStmt) {
        m_lastError = "Update user last login statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_updateUserLastLoginStmt);

    // Bind parameters
    sqlite3_bind_text(m_updateUserLastLoginStmt, 1, userId.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_updateUserLastLoginStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to update user last login: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<UserRecord> DatabaseManager::getAllUsers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<UserRecord> users;

    const char* query = R"(
        SELECT id, user_id, username, password_hash, role, created_at, last_login, enabled
        FROM users WHERE enabled = 1 ORDER BY created_at DESC
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select all users query: " + std::string(sqlite3_errmsg(m_db));
        return users;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord user;
        user.id = sqlite3_column_int(stmt, 0);
        user.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (createdAt) user.created_at = createdAt;

        const char* lastLogin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (lastLogin) user.last_login = lastLogin;

        user.enabled = sqlite3_column_int(stmt, 7) == 1;
        users.push_back(user);
    }

    sqlite3_finalize(stmt);
    return users;
}

// Session management operations implementation (NEW - Phase 2)
bool DatabaseManager::insertSession(const SessionRecord& session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_insertSessionStmt) {
        m_lastError = "Insert session statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_insertSessionStmt);

    // Bind parameters
    sqlite3_bind_text(m_insertSessionStmt, 1, session.session_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertSessionStmt, 2, session.user_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(m_insertSessionStmt, 3, session.expires_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_insertSessionStmt, 4, session.active ? 1 : 0);

    // Execute
    int rc = sqlite3_step(m_insertSessionStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to insert session: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

SessionRecord DatabaseManager::getSessionById(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SessionRecord session;

    if (!m_selectSessionByIdStmt) {
        m_lastError = "Select session by id statement not prepared";
        return session;
    }

    // Reset statement
    sqlite3_reset(m_selectSessionByIdStmt);

    // Bind parameters
    sqlite3_bind_text(m_selectSessionByIdStmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);

    // Execute
    if (sqlite3_step(m_selectSessionByIdStmt) == SQLITE_ROW) {
        session.session_id = reinterpret_cast<const char*>(sqlite3_column_text(m_selectSessionByIdStmt, 0));
        session.user_id = reinterpret_cast<const char*>(sqlite3_column_text(m_selectSessionByIdStmt, 1));

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(m_selectSessionByIdStmt, 2));
        if (createdAt) session.created_at = createdAt;

        const char* expiresAt = reinterpret_cast<const char*>(sqlite3_column_text(m_selectSessionByIdStmt, 3));
        if (expiresAt) session.expires_at = expiresAt;

        session.active = sqlite3_column_int(m_selectSessionByIdStmt, 4) == 1;
    }

    return session;
}

bool DatabaseManager::updateSession(const SessionRecord& session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_updateSessionStmt) {
        m_lastError = "Update session statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_updateSessionStmt);

    // Bind parameters
    sqlite3_bind_text(m_updateSessionStmt, 1, session.expires_at.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(m_updateSessionStmt, 2, session.active ? 1 : 0);
    sqlite3_bind_text(m_updateSessionStmt, 3, session.session_id.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_updateSessionStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to update session: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteSessionStmt) {
        m_lastError = "Delete session statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteSessionStmt);

    // Bind parameters
    sqlite3_bind_text(m_deleteSessionStmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_deleteSessionStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete session: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteUserSessions(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteUserSessionsStmt) {
        m_lastError = "Delete user sessions statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteUserSessionsStmt);

    // Bind parameters
    sqlite3_bind_text(m_deleteUserSessionsStmt, 1, userId.c_str(), -1, SQLITE_STATIC);

    // Execute
    int rc = sqlite3_step(m_deleteUserSessionsStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete user sessions: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

bool DatabaseManager::deleteExpiredSessions() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_deleteExpiredSessionsStmt) {
        m_lastError = "Delete expired sessions statement not prepared";
        return false;
    }

    // Reset statement
    sqlite3_reset(m_deleteExpiredSessionsStmt);

    // Execute
    int rc = sqlite3_step(m_deleteExpiredSessionsStmt);
    if (rc != SQLITE_DONE) {
        m_lastError = "Failed to delete expired sessions: " + std::string(sqlite3_errmsg(m_db));
        return false;
    }

    return true;
}

std::vector<SessionRecord> DatabaseManager::getActiveSessions(const std::string& userId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SessionRecord> sessions;

    std::string query = R"(
        SELECT session_id, user_id, created_at, expires_at, active
        FROM sessions WHERE active = 1 AND expires_at > datetime('now')
    )";

    if (!userId.empty()) {
        query += " AND user_id = '" + userId + "'";
    }

    query += " ORDER BY created_at DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = "Failed to prepare select active sessions query: " + std::string(sqlite3_errmsg(m_db));
        return sessions;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SessionRecord session;
        session.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        session.user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (createdAt) session.created_at = createdAt;

        const char* expiresAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (expiresAt) session.expires_at = expiresAt;

        session.active = sqlite3_column_int(stmt, 4) == 1;
        sessions.push_back(session);
    }

    sqlite3_finalize(stmt);
    return sessions;
}
