#include "RecordingController.h"
#include "../../database/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>
#include <fstream>

using namespace AISecurityVision;

void RecordingController::handleGetRecordings(const std::string& request, std::string& response) {
    try {
        // Get recordings from database/filesystem
        auto recordings = getRecordingsFromDatabase();

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"recordings\":[";

        for (size_t i = 0; i < recordings.size(); ++i) {
            if (i > 0) json << ",";
            json << serializeRecording(recordings[i]);
        }

        json << "],"
             << "\"total\":" << recordings.size() << ","
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved " + std::to_string(recordings.size()) + " recordings");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get recordings: " + std::string(e.what()), 500);
    }
}

void RecordingController::handleGetRecording(const std::string& recordingId, std::string& response) {
    try {
        if (recordingId.empty()) {
            response = createErrorResponse("Recording ID is required", 400);
            return;
        }

        RecordingInfo recording = getRecordingById(recordingId);
        if (recording.id.empty()) {
            response = createErrorResponse("Recording not found", 404);
            return;
        }

        response = createJsonResponse(serializeRecording(recording));
        logInfo("Retrieved recording: " + recordingId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get recording: " + std::string(e.what()), 500);
    }
}

void RecordingController::handleDeleteRecording(const std::string& recordingId, std::string& response) {
    try {
        if (recordingId.empty()) {
            response = createErrorResponse("Recording ID is required", 400);
            return;
        }

        // Check if recording exists
        RecordingInfo recording = getRecordingById(recordingId);
        if (recording.id.empty()) {
            response = createErrorResponse("Recording not found", 404);
            return;
        }

        // Delete recording file and database entry
        if (deleteRecordingFile(recordingId)) {
            std::ostringstream json;
            json << "{"
                 << "\"status\":\"success\","
                 << "\"message\":\"Recording deleted successfully\","
                 << "\"recording_id\":\"" << recordingId << "\","
                 << "\"deleted_at\":\"" << getCurrentTimestamp() << "\""
                 << "}";

            response = createJsonResponse(json.str());
            logInfo("Deleted recording: " + recordingId);
        } else {
            response = createErrorResponse("Failed to delete recording file", 500);
        }

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to delete recording: " + std::string(e.what()), 500);
    }
}

void RecordingController::handleDownloadRecording(const std::string& recordingId, std::string& response) {
    try {
        if (recordingId.empty()) {
            response = createErrorResponse("Recording ID is required", 400);
            return;
        }

        // Check if recording exists
        RecordingInfo recording = getRecordingById(recordingId);
        if (recording.id.empty()) {
            response = createErrorResponse("Recording not found", 404);
            return;
        }

        std::string filePath = getRecordingFilePath(recordingId);
        if (!std::filesystem::exists(filePath)) {
            response = createErrorResponse("Recording file not found", 404);
            return;
        }

        // For now, return download information
        // In a real implementation, this would stream the file
        std::ostringstream json;
        json << "{"
             << "\"download_url\":\"/recordings/" << recordingId << "/download\","
             << "\"filename\":\"" << recording.filename << "\","
             << "\"file_size\":" << recording.file_size << ","
             << "\"content_type\":\"video/mp4\","
             << "\"expires_at\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Prepared download for recording: " + recordingId);

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to prepare recording download: " + std::string(e.what()), 500);
    }
}

std::vector<RecordingController::RecordingInfo> RecordingController::getRecordingsFromDatabase() {
    std::vector<RecordingInfo> recordings;

    // For demonstration, return sample recordings
    // In a real implementation, this would query the database
    RecordingInfo recording1;
    recording1.id = "rec_001";
    recording1.camera_id = "camera_01";
    recording1.filename = "camera_01_20250604_120000.mp4";
    recording1.start_time = "2025-06-04T12:00:00Z";
    recording1.end_time = "2025-06-04T12:05:00Z";
    recording1.file_size = 52428800; // 50MB
    recording1.duration_seconds = 300;
    recording1.event_type = "motion_detection";
    recording1.is_available = true;

    RecordingInfo recording2;
    recording2.id = "rec_002";
    recording2.camera_id = "camera_02";
    recording2.filename = "camera_02_20250604_130000.mp4";
    recording2.start_time = "2025-06-04T13:00:00Z";
    recording2.end_time = "2025-06-04T13:10:00Z";
    recording2.file_size = 104857600; // 100MB
    recording2.duration_seconds = 600;
    recording2.event_type = "person_detection";
    recording2.is_available = true;

    recordings.push_back(recording1);
    recordings.push_back(recording2);

    return recordings;
}

RecordingController::RecordingInfo RecordingController::getRecordingById(const std::string& recordingId) {
    auto recordings = getRecordingsFromDatabase();
    
    for (const auto& recording : recordings) {
        if (recording.id == recordingId) {
            return recording;
        }
    }

    return RecordingInfo{}; // Return empty recording if not found
}

bool RecordingController::deleteRecordingFile(const std::string& recordingId) {
    try {
        std::string filePath = getRecordingFilePath(recordingId);
        
        if (std::filesystem::exists(filePath)) {
            return std::filesystem::remove(filePath);
        }
        
        // If file doesn't exist, consider deletion successful
        return true;
        
    } catch (const std::exception& e) {
        logError("Failed to delete recording file: " + std::string(e.what()));
        return false;
    }
}

std::string RecordingController::getRecordingFilePath(const std::string& recordingId) {
    // In a real implementation, this would map to actual file paths
    return "/var/recordings/" + recordingId + ".mp4";
}

std::string RecordingController::serializeRecording(const RecordingInfo& recording) {
    std::ostringstream json;
    json << "{"
         << "\"id\":\"" << recording.id << "\","
         << "\"camera_id\":\"" << recording.camera_id << "\","
         << "\"filename\":\"" << recording.filename << "\","
         << "\"start_time\":\"" << recording.start_time << "\","
         << "\"end_time\":\"" << recording.end_time << "\","
         << "\"file_size\":" << recording.file_size << ","
         << "\"duration_seconds\":" << recording.duration_seconds << ","
         << "\"event_type\":\"" << recording.event_type << "\","
         << "\"is_available\":" << (recording.is_available ? "true" : "false")
         << "}";
    
    return json.str();
}

std::string RecordingController::serializeRecordingList(const std::vector<RecordingInfo>& recordings) {
    std::ostringstream json;
    json << "[";
    
    for (size_t i = 0; i < recordings.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeRecording(recordings[i]);
    }
    
    json << "]";
    return json.str();
}
