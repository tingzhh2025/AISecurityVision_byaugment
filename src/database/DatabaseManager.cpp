#include "DatabaseManager.h"
#include <iostream>
#include <sstream>
#include <cstring>

DatabaseManager::DatabaseManager()
    : m_db(nullptr), m_insertEventStmt(nullptr), m_insertFaceStmt(nullptr),
      m_insertPlateStmt(nullptr), m_insertROIStmt(nullptr), m_selectEventsStmt(nullptr),
      m_selectFacesStmt(nullptr), m_selectPlatesStmt(nullptr), m_selectROIsStmt(nullptr) {
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
        std::cerr << "[DatabaseManager] " << m_lastError << std::endl;
        return false;
    }

    // Enable foreign keys
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Create tables
    if (!createTables()) {
        std::cerr << "[DatabaseManager] Failed to create tables" << std::endl;
        return false;
    }

    // Prepare statements
    if (!prepareStatements()) {
        std::cerr << "[DatabaseManager] Failed to prepare statements" << std::endl;
        return false;
    }

    std::cout << "[DatabaseManager] Initialized with database: " << dbPath << std::endl;
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