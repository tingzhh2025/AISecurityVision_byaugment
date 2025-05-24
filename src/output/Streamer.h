#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <opencv2/opencv.hpp>

// FFmpeg forward declarations
extern "C" {
    struct AVFormatContext;
    struct AVCodecContext;
    struct AVStream;
    struct AVFrame;
    struct SwsContext;
}

struct FrameResult;

/**
 * @brief Streaming protocol types
 */
enum class StreamProtocol {
    MJPEG,
    RTMP
};

/**
 * @brief Streaming configuration for both MJPEG and RTMP
 */
struct StreamConfig {
    StreamProtocol protocol = StreamProtocol::MJPEG;
    int width = 640;
    int height = 480;
    int fps = 15;
    int quality = 80;           // JPEG quality (1-100) for MJPEG
    int bitrate = 2000000;      // Video bitrate for RTMP (bps)
    int port = 8000;           // HTTP server port for MJPEG
    bool enableOverlays = true; // Show detection overlays
    std::string endpoint = "/stream.mjpg";  // MJPEG endpoint
    std::string rtmpUrl = "";   // RTMP server URL (e.g., "rtmp://localhost/live/test")
};

/**
 * @brief Multi-protocol streaming server (MJPEG/RTMP)
 *
 * This class provides:
 * - Real-time MJPEG streaming over HTTP
 * - Real-time RTMP streaming to external servers
 * - Configurable resolution, frame rate, and bitrate
 * - Detection overlay rendering
 * - Multi-client support (MJPEG)
 * - Automatic frame buffering
 * - FFmpeg-based RTMP encoding
 */
class Streamer {
public:
    Streamer();
    ~Streamer();

    // Initialization
    bool initialize(const std::string& sourceId);
    void cleanup();

    // Configuration
    void setConfig(const StreamConfig& config);
    StreamConfig getConfig() const;

    // Frame processing
    void processFrame(const FrameResult& result);

    // Server control
    bool startServer();
    void stopServer();
    bool isServerRunning() const;

    // RTMP control
    bool startRtmpStream();
    void stopRtmpStream();
    bool isRtmpStreaming() const;

    // Statistics
    size_t getConnectedClients() const;
    double getStreamFps() const;
    std::string getStreamUrl() const;
    bool isStreamHealthy() const;

private:
    // Internal structures
    struct ClientConnection {
        int socket;
        std::string address;
        std::chrono::steady_clock::time_point connectTime;

        ClientConnection(int s, const std::string& addr)
            : socket(s), address(addr), connectTime(std::chrono::steady_clock::now()) {}
    };

    struct FrameData {
        cv::Mat frame;
        std::vector<uint8_t> jpegData;
        std::chrono::steady_clock::time_point timestamp;

        FrameData() : timestamp(std::chrono::steady_clock::now()) {}
    };

    // Internal methods
    void serverThread();
    void clientHandlerThread(int clientSocket, const std::string& clientAddr);
    void frameProcessingThread();
    void rtmpStreamingThread();

    // MJPEG methods
    bool setupHttpServer();
    void handleHttpRequest(int clientSocket, const std::string& request);
    void sendHttpHeaders(int clientSocket);
    void sendMjpegFrame(int clientSocket, const std::vector<uint8_t>& jpegData);

    // RTMP methods
    bool setupRtmpEncoder();
    void cleanupRtmpEncoder();
    bool encodeAndSendRtmpFrame(const cv::Mat& frame);
    bool initializeRtmpStream();

    cv::Mat renderOverlays(const cv::Mat& frame, const FrameResult& result);
    void drawDetections(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                       const std::vector<std::string>& labels);
    void drawTrackingIds(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                        const std::vector<int>& trackIds);
    void drawTimestamp(cv::Mat& frame);

    // Enhanced overlay methods for Task 39
    cv::Scalar getDetectionColor(size_t index, const std::string& label);
    void drawCornerMarkers(cv::Mat& frame, const cv::Rect& bbox, const cv::Scalar& color, int size);
    void drawROIs(cv::Mat& frame, const FrameResult& result);
    void drawFaceRecognition(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                            const std::vector<std::string>& faceIds);
    void drawLicensePlates(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                          const std::vector<std::string>& plateNumbers);
    void drawBehaviorEvents(cv::Mat& frame, const std::vector<BehaviorEvent>& events);
    void drawSystemInfo(cv::Mat& frame, const FrameResult& result);

    std::vector<uint8_t> encodeJpeg(const cv::Mat& frame);
    cv::Mat resizeFrame(const cv::Mat& frame, int targetWidth, int targetHeight);

    void addFrameToBuffer(const FrameData& frameData);
    bool getLatestFrame(FrameData& frameData);
    void cleanupOldFrames();

    // Forward declarations for FFmpeg structures (use actual types)
    // These will be properly declared when FFmpeg headers are included

    // Member variables
    std::string m_sourceId;
    StreamConfig m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_serverRunning{false};
    std::atomic<bool> m_rtmpStreaming{false};
    mutable std::mutex m_mutex;

    // Threading
    std::thread m_serverThread;
    std::thread m_frameProcessingThread;
    std::thread m_rtmpStreamingThread;
    std::vector<std::thread> m_clientThreads;

    // HTTP server
    int m_serverSocket = -1;
    std::vector<std::unique_ptr<ClientConnection>> m_clients;
    mutable std::mutex m_clientsMutex;

    // Frame buffering
    std::queue<FrameData> m_frameBuffer;
    std::condition_variable m_frameCondition;
    mutable std::mutex m_frameBufferMutex;
    static constexpr size_t MAX_BUFFER_SIZE = 10;

    // Statistics
    std::atomic<size_t> m_frameCount{0};
    std::atomic<double> m_streamFps{0.0};
    std::chrono::steady_clock::time_point m_lastFpsUpdate;

    // RTMP streaming
    AVFormatContext* m_rtmpFormatContext = nullptr;
    AVCodecContext* m_rtmpCodecContext = nullptr;
    AVStream* m_rtmpStream = nullptr;
    AVFrame* m_rtmpFrame = nullptr;
    SwsContext* m_swsContext = nullptr;
    int64_t m_rtmpFrameCount = 0;
    mutable std::mutex m_rtmpMutex;

    // Constants
    static constexpr int LISTEN_BACKLOG = 10;
    static constexpr int FRAME_TIMEOUT_MS = 5000;
    static constexpr size_t MAX_CLIENTS = 10;
};
