#pragma once

#include "BaseController.h"
#include "../../core/ThreadPool.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <httplib.h>

namespace AISecurityVision {

/**
 * @brief Controller for camera-related API endpoints
 * 
 * Handles all camera management functionality including:
 * - Camera configuration (CRUD operations)
 * - Camera status and monitoring
 * - Video source management
 * - Stream URL generation
 * - ONVIF discovery
 * - Detection configuration
 */
class CameraController : public BaseController {
public:
    CameraController() = default;
    ~CameraController() override = default;

    // Camera configuration structure
    struct CameraConfig {
        std::string id;
        std::string name;
        std::string url;
        std::string protocol;
        std::string username;
        std::string password;
        int width;
        int height;
        int fps;
        int mjpeg_port;
        bool enabled;
    };

    // Initialize controller
    void initialize(TaskManager* taskManager,
                   ONVIFManager* onvifManager,
                   AISecurityVision::NetworkManager* networkManager) override;

    // Cleanup resources
    void cleanup();

    // Camera management endpoints
    void handleGetCameras(const std::string& request, std::string& response);
    void handlePostVideoSource(const std::string& request, std::string& response);
    void handleDeleteVideoSource(const std::string& request, std::string& response);
    void handleGetVideoSources(const std::string& request, std::string& response);
    void handleTestCameraConnection(const std::string& request, std::string& response);

    // Camera configuration endpoints
    void handleGetCameraConfigs(const std::string& request, std::string& response);
    void handlePostCameraConfig(const std::string& request, std::string& response);
    void handleDeleteCameraConfig(const std::string& cameraId, std::string& response);

    // ONVIF discovery endpoints
    void handleGetDiscoverDevices(const std::string& request, std::string& response);
    void handlePostAddDiscoveredDevice(const std::string& request, std::string& response);

    // Camera CRUD operations (NEW - Phase 1)
    void handleGetCamera(const std::string& cameraId, std::string& response);
    void handleUpdateCamera(const std::string& cameraId, const std::string& request, std::string& response);
    void handleDeleteCamera(const std::string& cameraId, std::string& response);
    void handleTestCamera(const std::string& request, std::string& response);

    // Detection category filtering endpoints
    void handleGetDetectionCategories(const std::string& request, std::string& response);
    void handlePostDetectionCategories(const std::string& request, std::string& response);
    void handleGetAvailableCategories(const std::string& request, std::string& response);
    void handleGetDetectionConfig(const std::string& request, std::string& response);
    void handlePostDetectionConfig(const std::string& request, std::string& response);
    void handlePutDetectionConfig(const std::string& request, std::string& response);
    void handleGetDetectionStats(const std::string& request, std::string& response);

    // Streaming configuration endpoints
    void handlePostStreamConfig(const std::string& request, std::string& response);
    void handleGetStreamConfig(const std::string& request, std::string& response);
    void handlePostStreamStart(const std::string& request, std::string& response);
    void handlePostStreamStop(const std::string& request, std::string& response);
    void handleGetStreamStatus(const std::string& request, std::string& response);

    // Video stream proxy handler
    void handleStreamProxy(const std::string& cameraId, const httplib::Request& req, httplib::Response& res);

    // Recording API handlers
    void handlePostRecordStart(const std::string& request, std::string& response);
    void handlePostRecordStop(const std::string& request, std::string& response);
    void handlePostRecordConfig(const std::string& request, std::string& response);
    void handleGetRecordStatus(const std::string& request, std::string& response);
    void handleGetRecordings(const std::string& request, std::string& response);

    // Face management handlers
    void handlePostFaceAdd(const httplib::Request& request, std::string& response);
    void handleGetFaces(const std::string& request, std::string& response);
    void handleDeleteFace(const std::string& request, std::string& response, const std::string& faceId);
    void handlePostFaceVerify(const httplib::Request& request, std::string& response);

    // ReID configuration handlers
    void handlePostReIDConfig(const std::string& request, std::string& response);
    void handleGetReIDConfig(const std::string& request, std::string& response);
    void handlePutReIDThreshold(const std::string& request, std::string& response);
    void handleGetReIDStatus(const std::string& request, std::string& response);

    // Cross-camera tracking handlers
    void handleGetCrossCameraTracks(const std::string& request, std::string& response);
    void handleGetCrossCameraConfig(const std::string& request, std::string& response);
    void handlePostCrossCameraConfig(const std::string& request, std::string& response);
    void handleGetCrossCameraStats(const std::string& request, std::string& response);
    void handlePostCrossCameraReset(const std::string& request, std::string& response);

    // ROI management handlers
    void handlePostROIs(const std::string& request, std::string& response);
    void handleGetROIs(const std::string& request, std::string& response);
    void handleGetROI(const std::string& request, std::string& response, const std::string& roiId);
    void handlePutROI(const std::string& request, std::string& response, const std::string& roiId);
    void handleDeleteROI(const std::string& request, std::string& response, const std::string& roiId);
    void handlePostBulkROIs(const std::string& request, std::string& response);

    // Behavior rule management handlers
    void handlePostRules(const std::string& request, std::string& response);
    void handleGetRules(const std::string& request, std::string& response);
    void handleGetRule(const std::string& request, std::string& response, const std::string& ruleId);
    void handlePutRule(const std::string& request, std::string& response, const std::string& ruleId);
    void handleDeleteRule(const std::string& request, std::string& response, const std::string& ruleId);

    // Configuration management
    void clearInMemoryConfigurations();
    void loadCameraConfigsFromDatabase();
    const std::vector<CameraConfig>& getCameraConfigs() const { return m_cameraConfigs; }

private:
    std::string getControllerName() const override { return "CameraController"; }

    // Camera configuration storage
    std::vector<CameraConfig> m_cameraConfigs;

    // Thread-safe asynchronous operation management
    std::shared_ptr<AISecurityVision::ThreadPool> m_threadPool;
    mutable std::mutex m_pendingOperationsMutex;
    std::unordered_set<std::string> m_pendingCameraOperations;

    // Utility methods for serialization
    std::string serializeCameraConfig(const CameraConfig& config);
    std::string serializeCameraConfigList(const std::vector<CameraConfig>& configs);
    bool deserializeCameraConfig(const std::string& json, CameraConfig& config);

    // Helper methods
    std::string extractIpFromUrl(const std::string& url);

    // Thread-safe operation management
    bool isOperationPending(const std::string& cameraId) const;
    void markOperationPending(const std::string& cameraId);
    void markOperationComplete(const std::string& cameraId);
};

} // namespace AISecurityVision
