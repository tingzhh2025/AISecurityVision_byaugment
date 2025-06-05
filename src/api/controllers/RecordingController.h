#pragma once

#include "BaseController.h"
#include <string>
#include <vector>

namespace AISecurityVision {

/**
 * @brief Controller for recording management API endpoints (NEW - Phase 3)
 * 
 * Handles all recording-related functionality including:
 * - Recording retrieval and listing
 * - Recording deletion
 * - Recording download
 * - Recording metadata management
 */
class RecordingController : public BaseController {
public:
    RecordingController() = default;
    ~RecordingController() override = default;

    /**
     * @brief Recording information structure
     */
    struct RecordingInfo {
        std::string id;
        std::string camera_id;
        std::string filename;
        std::string start_time;
        std::string end_time;
        size_t file_size;
        int duration_seconds;
        std::string event_type;
        bool is_available;
    };

    // Recording management endpoints
    void handleGetRecordings(const std::string& request, std::string& response);
    void handleGetRecording(const std::string& recordingId, std::string& response);
    void handleDeleteRecording(const std::string& recordingId, std::string& response);
    void handleDownloadRecording(const std::string& recordingId, std::string& response);

private:
    std::string getControllerName() const override { return "RecordingController"; }

    // Helper methods
    std::vector<RecordingInfo> getRecordingsFromDatabase();
    RecordingInfo getRecordingById(const std::string& recordingId);
    bool deleteRecordingFile(const std::string& recordingId);
    std::string getRecordingFilePath(const std::string& recordingId);
    
    // JSON serialization
    std::string serializeRecording(const RecordingInfo& recording);
    std::string serializeRecordingList(const std::vector<RecordingInfo>& recordings);
};

} // namespace AISecurityVision
