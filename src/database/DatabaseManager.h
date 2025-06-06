#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <map>

/**
 * @brief Event record structure for database storage
 */
struct EventRecord {
    int id = 0;
    std::string camera_id;
    std::string event_type;
    std::string timestamp;
    std::string video_path;
    std::string metadata;  // JSON string for additional data
    double confidence = 0.0;

    EventRecord() = default;
    EventRecord(const std::string& cameraId, const std::string& eventType,
                const std::string& videoPath, double conf = 0.0)
        : camera_id(cameraId), event_type(eventType), video_path(videoPath), confidence(conf) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        timestamp = buffer;
    }
};

/**
 * @brief Face record structure for database storage
 */
struct FaceRecord {
    int id = 0;
    std::string name;
    std::string image_path;
    std::vector<float> embedding;  // Face embedding vector
    std::string created_at;

    FaceRecord() = default;
    FaceRecord(const std::string& faceName, const std::string& imagePath)
        : name(faceName), image_path(imagePath) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        created_at = buffer;
    }
};

/**
 * @brief License plate record structure for database storage
 */
struct LicensePlateRecord {
    int id = 0;
    std::string plate_number;
    std::string region;
    std::string image_path;
    std::string created_at;

    LicensePlateRecord() = default;
    LicensePlateRecord(const std::string& plateNumber, const std::string& plateRegion,
                      const std::string& imagePath)
        : plate_number(plateNumber), region(plateRegion), image_path(imagePath) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        created_at = buffer;
    }
};

/**
 * @brief ROI record structure for database storage
 */
struct ROIRecord {
    int id = 0;
    std::string roi_id;
    std::string camera_id;
    std::string name;
    std::string polygon_data;  // JSON string of polygon coordinates
    bool enabled = true;
    int priority = 1;
    std::string start_time;    // ISO 8601 time format (HH:MM or HH:MM:SS)
    std::string end_time;      // ISO 8601 time format (HH:MM or HH:MM:SS)
    std::string created_at;
    std::string updated_at;

    ROIRecord() = default;
    ROIRecord(const std::string& roiId, const std::string& cameraId,
              const std::string& roiName, const std::string& polygonData)
        : roi_id(roiId), camera_id(cameraId), name(roiName), polygon_data(polygonData) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        created_at = buffer;
        updated_at = buffer;
    }
};

/**
 * @brief User record structure for authentication (NEW - Phase 2)
 */
struct UserRecord {
    int id = 0;
    std::string user_id;
    std::string username;
    std::string password_hash;
    std::string role = "user";
    std::string created_at;
    std::string last_login;
    bool enabled = true;

    UserRecord() = default;
    UserRecord(const std::string& userId, const std::string& userName,
               const std::string& passwordHash, const std::string& userRole = "user")
        : user_id(userId), username(userName), password_hash(passwordHash), role(userRole) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        created_at = buffer;
    }
};

/**
 * @brief Session record structure for authentication (NEW - Phase 2)
 */
struct SessionRecord {
    std::string session_id;
    std::string user_id;
    std::string created_at;
    std::string expires_at;
    bool active = true;

    SessionRecord() = default;
    SessionRecord(const std::string& sessionId, const std::string& userId, int expirationHours = 24)
        : session_id(sessionId), user_id(userId) {
        // Set current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        created_at = buffer;

        // Set expiration time
        auto expiry_time = now + std::chrono::hours(expirationHours);
        auto expiry_time_t = std::chrono::system_clock::to_time_t(expiry_time);
        auto expiry_tm = *std::localtime(&expiry_time_t);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &expiry_tm);
        expires_at = buffer;
    }
};

/**
 * @brief SQLite database manager with ORM-like functionality
 *
 * This class provides thread-safe database operations for the AI Security Vision System.
 * It manages event recordings, face recognition data, license plate records, and user authentication.
 */
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Database lifecycle
    bool initialize(const std::string& dbPath = "aibox.db");
    void close();
    bool isConnected() const;

    // Event operations
    bool insertEvent(const EventRecord& event);
    std::vector<EventRecord> getEvents(const std::string& cameraId = "",
                                     const std::string& eventType = "",
                                     int limit = 100);
    bool deleteEvent(int eventId);
    bool deleteOldEvents(int daysOld = 30);

    // Face operations
    bool insertFace(const FaceRecord& face);
    std::vector<FaceRecord> getFaces();
    FaceRecord getFaceById(int faceId);
    FaceRecord getFaceByName(const std::string& name);
    bool updateFace(const FaceRecord& face);
    bool deleteFace(int faceId);

    // License plate operations
    bool insertLicensePlate(const LicensePlateRecord& plate);
    std::vector<LicensePlateRecord> getLicensePlates();
    LicensePlateRecord getLicensePlateById(int plateId);
    bool deleteLicensePlate(int plateId);

    // ROI operations
    bool insertROI(const ROIRecord& roi);
    std::vector<ROIRecord> getROIs(const std::string& cameraId = "");
    ROIRecord getROIById(const std::string& roiId);
    ROIRecord getROIByDatabaseId(int id);
    bool updateROI(const ROIRecord& roi);
    bool deleteROI(const std::string& roiId);
    bool deleteROIsByCameraId(const std::string& cameraId);

    // Bulk ROI operations with transaction support - Task 72
    bool insertROIsBulk(const std::vector<ROIRecord>& rois);
    bool updateROIsBulk(const std::vector<ROIRecord>& rois);
    bool deleteROIsBulk(const std::vector<std::string>& roiIds);

    // Configuration operations
    bool saveConfig(const std::string& category, const std::string& key, const std::string& value);
    std::string getConfig(const std::string& category, const std::string& key, const std::string& defaultValue = "");
    bool deleteConfig(const std::string& category, const std::string& key = "");
    std::map<std::string, std::string> getAllConfigs(const std::string& category = "");

    // Camera configuration operations
    bool saveCameraConfig(const std::string& cameraId, const std::string& configJson);
    std::string getCameraConfig(const std::string& cameraId);
    std::vector<std::string> getAllCameraIds();
    bool deleteCameraConfig(const std::string& cameraId);

    // Detection category configuration operations
    bool saveDetectionCategories(const std::vector<std::string>& enabledCategories);
    std::vector<std::string> getDetectionCategories();
    bool resetDetectionCategories();

    // User authentication operations (NEW - Phase 2)
    bool insertUser(const UserRecord& user);
    UserRecord getUserById(const std::string& userId);
    UserRecord getUserByUsername(const std::string& username);
    bool updateUser(const UserRecord& user);
    bool deleteUser(const std::string& userId);
    bool updateUserLastLogin(const std::string& userId);
    std::vector<UserRecord> getAllUsers();

    // Session management operations (NEW - Phase 2)
    bool insertSession(const SessionRecord& session);
    SessionRecord getSessionById(const std::string& sessionId);
    bool updateSession(const SessionRecord& session);
    bool deleteSession(const std::string& sessionId);
    bool deleteUserSessions(const std::string& userId);
    bool deleteExpiredSessions();
    std::vector<SessionRecord> getActiveSessions(const std::string& userId = "");

    // Utility operations
    bool executeQuery(const std::string& query);
    int getLastInsertId();
    std::string getErrorMessage() const;

    // Transaction support - Task 72
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    // Internal methods
    bool createTables();
    bool prepareStatements();
    void finalizeStatements();

    // Helper methods
    std::string vectorToBlob(const std::vector<float>& vec);
    std::vector<float> blobToVector(const void* blob, int size);

    // Member variables
    sqlite3* m_db;
    mutable std::mutex m_mutex;
    std::string m_dbPath;
    std::string m_lastError;

    // Prepared statements for performance
    sqlite3_stmt* m_insertEventStmt;
    sqlite3_stmt* m_insertFaceStmt;
    sqlite3_stmt* m_insertPlateStmt;
    sqlite3_stmt* m_insertROIStmt;
    sqlite3_stmt* m_selectEventsStmt;
    sqlite3_stmt* m_selectFacesStmt;
    sqlite3_stmt* m_selectPlatesStmt;
    sqlite3_stmt* m_selectROIsStmt;

    // Configuration prepared statements
    sqlite3_stmt* m_insertConfigStmt;
    sqlite3_stmt* m_updateConfigStmt;
    sqlite3_stmt* m_selectConfigStmt;
    sqlite3_stmt* m_deleteConfigStmt;
    sqlite3_stmt* m_insertCameraConfigStmt;
    sqlite3_stmt* m_updateCameraConfigStmt;
    sqlite3_stmt* m_selectCameraConfigStmt;
    sqlite3_stmt* m_deleteCameraConfigStmt;

    // User authentication prepared statements (NEW - Phase 2)
    sqlite3_stmt* m_insertUserStmt;
    sqlite3_stmt* m_selectUserByIdStmt;
    sqlite3_stmt* m_selectUserByUsernameStmt;
    sqlite3_stmt* m_updateUserStmt;
    sqlite3_stmt* m_deleteUserStmt;
    sqlite3_stmt* m_updateUserLastLoginStmt;

    // Session management prepared statements (NEW - Phase 2)
    sqlite3_stmt* m_insertSessionStmt;
    sqlite3_stmt* m_selectSessionByIdStmt;
    sqlite3_stmt* m_updateSessionStmt;
    sqlite3_stmt* m_deleteSessionStmt;
    sqlite3_stmt* m_deleteUserSessionsStmt;
    sqlite3_stmt* m_deleteExpiredSessionsStmt;
};
