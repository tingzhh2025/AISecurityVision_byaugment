#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <opencv2/opencv.hpp>
#include <httplib.h>

// Forward declarations
struct ROI;
struct IntrusionRule;
struct AlarmConfig;
#include <memory>

/**
 * @brief HTTP API Service for system control and monitoring
 *
 * Provides RESTful endpoints for:
 * - Video source management
 * - System status and monitoring
 * - AI model configuration
 * - Behavior rule management
 * - Face/plate database operations
 */
class APIService {
public:
    explicit APIService(int port = 8080);
    ~APIService();

    // Service control
    bool start();
    void stop();
    bool isRunning() const;

    // Configuration
    void setPort(int port);
    int getPort() const;

    // Enhanced polygon validation with detailed error reporting
    struct PolygonValidationResult {
        bool isValid;
        std::string errorMessage;
        std::string errorCode;
        double area;
        bool isClosed;
        bool isConvex;
        bool hasSelfIntersection;

        PolygonValidationResult() : isValid(false), area(0.0), isClosed(false),
                                  isConvex(false), hasSelfIntersection(false) {}
    };

    // Public validation methods for testing
    bool validateROIPolygon(const std::vector<cv::Point>& polygon);
    PolygonValidationResult validateROIPolygonDetailed(const std::vector<cv::Point>& polygon);

private:
    // HTTP server implementation
    void serverThread();
    void setupRoutes();

    // Route handlers
    void handleGetStatus(const std::string& request, std::string& response);
    void handlePostVideoSource(const std::string& request, std::string& response);
    void handleDeleteVideoSource(const std::string& request, std::string& response);
    void handleGetVideoSources(const std::string& request, std::string& response);

    // ONVIF discovery handlers
    void handleGetDiscoverDevices(const std::string& request, std::string& response);
    void handlePostAddDiscoveredDevice(const std::string& request, std::string& response);

    // Recording API handlers
    void handlePostRecordStart(const std::string& request, std::string& response);
    void handlePostRecordStop(const std::string& request, std::string& response);
    void handlePostRecordConfig(const std::string& request, std::string& response);
    void handleGetRecordStatus(const std::string& request, std::string& response);

    // System monitoring handlers
    void handleGetSystemMetrics(const std::string& request, std::string& response);
    void handleGetPipelineStats(const std::string& request, std::string& response);
    void handleGetSystemStats(const std::string& request, std::string& response);

    // Streaming configuration handlers
    void handlePostStreamConfig(const std::string& request, std::string& response);
    void handleGetStreamConfig(const std::string& request, std::string& response);
    void handlePostStreamStart(const std::string& request, std::string& response);
    void handlePostStreamStop(const std::string& request, std::string& response);
    void handleGetStreamStatus(const std::string& request, std::string& response);

    // Behavior rule management handlers
    void handlePostRules(const std::string& request, std::string& response);
    void handleGetRules(const std::string& request, std::string& response);
    void handleGetRule(const std::string& request, std::string& response, const std::string& ruleId);
    void handlePutRule(const std::string& request, std::string& response, const std::string& ruleId);
    void handleDeleteRule(const std::string& request, std::string& response, const std::string& ruleId);

    // ROI management handlers
    void handlePostROIs(const std::string& request, std::string& response);
    void handleGetROIs(const std::string& request, std::string& response);
    void handleGetROI(const std::string& request, std::string& response, const std::string& roiId);
    void handlePutROI(const std::string& request, std::string& response, const std::string& roiId);
    void handleDeleteROI(const std::string& request, std::string& response, const std::string& roiId);

    // Bulk ROI operations handler - Task 72
    void handlePostBulkROIs(const std::string& request, std::string& response);

    // ReID configuration handlers - Task 76
    void handlePostReIDConfig(const std::string& request, std::string& response);
    void handleGetReIDConfig(const std::string& request, std::string& response);
    void handlePutReIDThreshold(const std::string& request, std::string& response);
    void handleGetReIDStatus(const std::string& request, std::string& response);

    // Web dashboard handlers
    void handleGetDashboard(const std::string& request, std::string& response);
    void handleStaticFile(const std::string& request, std::string& response, const std::string& filePath);

    // Face management handlers
    void handlePostFaceAdd(const httplib::Request& request, std::string& response);
    void handleGetFaces(const std::string& request, std::string& response);
    void handleDeleteFace(const std::string& request, std::string& response, const std::string& faceId);
    void handlePostFaceVerify(const httplib::Request& request, std::string& response);

    // Alarm configuration handlers
    void handlePostAlarmConfig(const std::string& request, std::string& response);
    void handleGetAlarmConfigs(const std::string& request, std::string& response);
    void handleGetAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handlePutAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handleDeleteAlarmConfig(const std::string& request, std::string& response, const std::string& configId);
    void handlePostTestAlarm(const std::string& request, std::string& response);
    void handleGetAlarmStatus(const std::string& request, std::string& response);

    // Frontend compatibility handlers
    void handleGetAlerts(const std::string& request, std::string& response);
    void handleGetSystemInfo(const std::string& request, std::string& response);
    void handleGetCameras(const std::string& request, std::string& response);
    void handleGetRecordings(const std::string& request, std::string& response);
    void handleGetDetectionConfig(const std::string& request, std::string& response);

    // Task 75: Cross-camera tracking handlers
    void handleGetCrossCameraTracks(const std::string& request, std::string& response);
    void handleGetCrossCameraConfig(const std::string& request, std::string& response);
    void handlePostCrossCameraConfig(const std::string& request, std::string& response);
    void handleGetCrossCameraStats(const std::string& request, std::string& response);
    void handlePostCrossCameraReset(const std::string& request, std::string& response);

    // Utility methods
    std::string createJsonResponse(const std::string& data, int statusCode = 200);
    std::string createErrorResponse(const std::string& error, int statusCode = 400);
    std::string parseJsonField(const std::string& json, const std::string& field);
    int parseJsonInt(const std::string& json, const std::string& field, int defaultValue = 0);
    float parseJsonFloat(const std::string& json, const std::string& field, float defaultValue = 0.0f);
    bool parseJsonBool(const std::string& json, const std::string& field, bool defaultValue = false);
    std::string getCurrentTimestamp();

    // File serving utilities
    std::string readFile(const std::string& filePath);
    std::string getMimeType(const std::string& filePath);
    std::string createFileResponse(const std::string& content, const std::string& mimeType, int statusCode = 200);
    bool fileExists(const std::string& filePath);

    // JSON serialization for behavior rules
    std::string serializeROI(const ROI& roi);
    std::string serializeIntrusionRule(const IntrusionRule& rule);
    std::string serializeROIList(const std::vector<ROI>& rois);
    std::string serializeRuleList(const std::vector<IntrusionRule>& rules);

    // JSON deserialization for behavior rules
    bool deserializeROI(const std::string& json, ROI& roi);
    bool deserializeIntrusionRule(const std::string& json, IntrusionRule& rule);

    // JSON serialization for alarm configurations
    std::string serializeAlarmConfig(const struct AlarmConfig& config);
    std::string serializeAlarmConfigList(const std::vector<struct AlarmConfig>& configs);

    // JSON deserialization for alarm configurations
    bool deserializeAlarmConfig(const std::string& json, struct AlarmConfig& config);

    // Task 75: Cross-camera tracking serialization
    std::string serializeCrossCameraTrack(const struct CrossCameraTrack& track);
    std::string serializeCrossCameraTrackList(const std::vector<struct CrossCameraTrack>& tracks);
    std::string serializeReIDMatch(const struct ReIDMatch& match);
    std::string serializeReIDMatchList(const std::vector<struct ReIDMatch>& matches);

    // Web interface utilities
    std::string loadWebFile(const std::string& filePath);

    // Member variables
    int m_port;
    std::atomic<bool> m_running{false};

    // ONVIF discovery manager
    std::unique_ptr<class ONVIFManager> m_onvifManager;
    std::thread m_serverThread;

    // HTTP server implementation
    std::unique_ptr<httplib::Server> m_httpServer;
};
