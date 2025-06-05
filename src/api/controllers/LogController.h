#pragma once

#include "BaseController.h"
#include <string>
#include <vector>

namespace AISecurityVision {

/**
 * @brief Controller for system log management API endpoints (NEW - Phase 3)
 * 
 * Handles all logging-related functionality including:
 * - Log retrieval and filtering
 * - Log level management
 * - Log file access
 * - System diagnostics
 */
class LogController : public BaseController {
public:
    LogController() = default;
    ~LogController() override = default;

    /**
     * @brief Log entry structure
     */
    struct LogEntry {
        std::string timestamp;
        std::string level;
        std::string component;
        std::string message;
        std::string thread_id;
        std::string file;
        int line_number;
    };

    /**
     * @brief Log query parameters
     */
    struct LogQuery {
        std::string level = "all";      // all, error, warn, info, debug
        std::string component = "all";  // all, or specific component
        std::string start_time;         // ISO timestamp
        std::string end_time;           // ISO timestamp
        int limit = 100;                // Maximum number of entries
        int offset = 0;                 // Pagination offset
        std::string search;             // Search term
    };

    // Log management endpoints
    void handleGetLogs(const std::string& request, std::string& response);
    void handleGetLogStats(const std::string& request, std::string& response);

private:
    std::string getControllerName() const override { return "LogController"; }

    // Helper methods
    std::vector<LogEntry> getLogsFromFile(const LogQuery& query);
    LogQuery parseLogQuery(const std::string& queryString);
    std::string serializeLogEntry(const LogEntry& entry);
    std::string serializeLogList(const std::vector<LogEntry>& logs);
    
    // Log file management
    std::string getLogFilePath();
    std::vector<std::string> getAvailableLogFiles();
};

} // namespace AISecurityVision
