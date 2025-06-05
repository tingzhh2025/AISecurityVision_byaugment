#include "LogController.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>

using namespace AISecurityVision;

void LogController::handleGetLogs(const std::string& request, std::string& response) {
    try {
        // Parse query parameters from request
        LogQuery query;
        // In a real implementation, parse query parameters from HTTP request
        // For now, use defaults
        
        auto logs = getLogsFromFile(query);

        // Build response
        std::ostringstream json;
        json << "{"
             << "\"logs\":[";

        for (size_t i = 0; i < logs.size(); ++i) {
            if (i > 0) json << ",";
            json << serializeLogEntry(logs[i]);
        }

        json << "],"
             << "\"total\":" << logs.size() << ","
             << "\"query\":{"
             << "\"level\":\"" << query.level << "\","
             << "\"component\":\"" << query.component << "\","
             << "\"limit\":" << query.limit << ","
             << "\"offset\":" << query.offset
             << "},"
             << "\"timestamp\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved " + std::to_string(logs.size()) + " log entries");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get logs: " + std::string(e.what()), 500);
    }
}

void LogController::handleGetLogStats(const std::string& request, std::string& response) {
    try {
        // Get log statistics
        auto allLogs = getLogsFromFile(LogQuery{});
        
        int errorCount = 0;
        int warnCount = 0;
        int infoCount = 0;
        int debugCount = 0;
        
        for (const auto& log : allLogs) {
            if (log.level == "ERROR") errorCount++;
            else if (log.level == "WARN") warnCount++;
            else if (log.level == "INFO") infoCount++;
            else if (log.level == "DEBUG") debugCount++;
        }

        std::ostringstream json;
        json << "{"
             << "\"total_entries\":" << allLogs.size() << ","
             << "\"by_level\":{"
             << "\"error\":" << errorCount << ","
             << "\"warn\":" << warnCount << ","
             << "\"info\":" << infoCount << ","
             << "\"debug\":" << debugCount
             << "},"
             << "\"log_files\":" << getAvailableLogFiles().size() << ","
             << "\"last_updated\":\"" << getCurrentTimestamp() << "\""
             << "}";

        response = createJsonResponse(json.str());
        logInfo("Retrieved log statistics");

    } catch (const std::exception& e) {
        response = createErrorResponse("Failed to get log stats: " + std::string(e.what()), 500);
    }
}

std::vector<LogController::LogEntry> LogController::getLogsFromFile(const LogQuery& query) {
    std::vector<LogEntry> logs;

    // For demonstration, return sample log entries
    // In a real implementation, this would read from actual log files
    LogEntry entry1;
    entry1.timestamp = "2025-06-04T18:20:00.123Z";
    entry1.level = "INFO";
    entry1.component = "APIService";
    entry1.message = "API service started successfully";
    entry1.thread_id = "main";
    entry1.file = "APIService.cpp";
    entry1.line_number = 95;

    LogEntry entry2;
    entry2.timestamp = "2025-06-04T18:20:01.456Z";
    entry2.level = "INFO";
    entry2.component = "TaskManager";
    entry2.message = "TaskManager initialized";
    entry2.thread_id = "main";
    entry2.file = "TaskManager.cpp";
    entry2.line_number = 57;

    LogEntry entry3;
    entry3.timestamp = "2025-06-04T18:20:02.789Z";
    entry3.level = "WARN";
    entry3.component = "CameraController";
    entry3.message = "No cameras configured in database";
    entry3.thread_id = "main";
    entry3.file = "CameraController.cpp";
    entry3.line_number = 168;

    LogEntry entry4;
    entry4.timestamp = "2025-06-04T18:20:03.012Z";
    entry4.level = "ERROR";
    entry4.component = "NetworkManager";
    entry4.message = "Failed to bind to network interface";
    entry4.thread_id = "network";
    entry4.file = "NetworkManager.cpp";
    entry4.line_number = 45;

    logs.push_back(entry1);
    logs.push_back(entry2);
    logs.push_back(entry3);
    logs.push_back(entry4);

    // Apply filtering based on query
    if (query.level != "all") {
        logs.erase(std::remove_if(logs.begin(), logs.end(),
            [&query](const LogEntry& entry) {
                return entry.level != query.level;
            }), logs.end());
    }

    // Apply limit
    if (query.limit > 0 && logs.size() > static_cast<size_t>(query.limit)) {
        logs.resize(query.limit);
    }

    return logs;
}

LogController::LogQuery LogController::parseLogQuery(const std::string& queryString) {
    LogQuery query;
    
    // In a real implementation, parse HTTP query parameters
    // For now, return defaults
    
    return query;
}

std::string LogController::serializeLogEntry(const LogEntry& entry) {
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << entry.timestamp << "\","
         << "\"level\":\"" << entry.level << "\","
         << "\"component\":\"" << entry.component << "\","
         << "\"message\":\"" << entry.message << "\","
         << "\"thread_id\":\"" << entry.thread_id << "\","
         << "\"file\":\"" << entry.file << "\","
         << "\"line_number\":" << entry.line_number
         << "}";
    
    return json.str();
}

std::string LogController::serializeLogList(const std::vector<LogEntry>& logs) {
    std::ostringstream json;
    json << "[";
    
    for (size_t i = 0; i < logs.size(); ++i) {
        if (i > 0) json << ",";
        json << serializeLogEntry(logs[i]);
    }
    
    json << "]";
    return json.str();
}

std::string LogController::getLogFilePath() {
    // In a real implementation, this would return the actual log file path
    return "/var/log/aisecurityvision/system.log";
}

std::vector<std::string> LogController::getAvailableLogFiles() {
    std::vector<std::string> logFiles;
    
    // For demonstration, return sample log files
    logFiles.push_back("system.log");
    logFiles.push_back("error.log");
    logFiles.push_back("access.log");
    
    return logFiles;
}
