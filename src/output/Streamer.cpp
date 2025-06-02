#include "Streamer.h"
#include "../core/VideoPipeline.h"
#include "../ai/BehaviorAnalyzer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cctype>
#include "../core/Logger.h"
using namespace AISecurityVision;

// FFmpeg includes for RTMP streaming
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

// Helper function for FFmpeg error messages
static std::string ffmpeg_error_string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return std::string(errbuf);
}

Streamer::Streamer() : m_lastFpsUpdate(std::chrono::steady_clock::now()) {
    LOG_INFO() << "[Streamer] Creating multi-protocol streamer";

    // Initialize FFmpeg (thread-safe in newer versions)
    static std::once_flag ffmpeg_init_flag;
    std::call_once(ffmpeg_init_flag, []() {
        av_log_set_level(AV_LOG_WARNING);
        LOG_INFO() << "[Streamer] FFmpeg initialized";
    });
}

Streamer::~Streamer() {
    cleanup();
}

bool Streamer::initialize(const std::string& sourceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sourceId = sourceId;
    m_running.store(true);

    // Start frame processing thread
    m_frameProcessingThread = std::thread(&Streamer::frameProcessingThread, this);

    // Initialize based on protocol
    if (m_config.protocol == StreamProtocol::MJPEG) {
        // Start HTTP server for MJPEG
        if (!startServer()) {
            LOG_ERROR() << "[Streamer] Failed to start HTTP server for " << sourceId;
            return false;
        }
        LOG_INFO() << "[Streamer] Initialized MJPEG streamer for " << sourceId
                  << " on port " << m_config.port;
    } else if (m_config.protocol == StreamProtocol::RTMP) {
        // Start RTMP stream
        if (!startRtmpStream()) {
            LOG_ERROR() << "[Streamer] Failed to start RTMP stream for " << sourceId;
            return false;
        }
        LOG_INFO() << "[Streamer] Initialized RTMP streamer for " << sourceId
                  << " to " << m_config.rtmpUrl;
    }

    return true;
}

void Streamer::cleanup() {
    LOG_INFO() << "[Streamer] Cleaning up streamer for " << m_sourceId;

    stopServer();
    stopRtmpStream();
    m_running.store(false);

    // Wake up frame processing thread
    {
        std::lock_guard<std::mutex> lock(m_frameBufferMutex);
        m_frameCondition.notify_all();
    }

    // Join threads
    if (m_frameProcessingThread.joinable()) {
        m_frameProcessingThread.join();
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    if (m_rtmpStreamingThread.joinable()) {
        m_rtmpStreamingThread.join();
    }

    // Join client threads
    for (auto& thread : m_clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_clientThreads.clear();

    // Close server socket
    if (m_serverSocket >= 0) {
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    LOG_INFO() << "[Streamer] Cleanup complete for " << m_sourceId;
}

void Streamer::setConfig(const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    LOG_INFO() << "[Streamer] Updated config: " << config.width << "x" << config.height
              << "@" << config.fps << "fps, quality=" << config.quality;
}

StreamConfig Streamer::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void Streamer::processFrame(const FrameResult& result) {
    if (!m_running.load() || result.frame.empty()) {
        return;
    }

    // Create frame data
    FrameData frameData;

    // Render overlays if enabled
    if (m_config.enableOverlays) {
        frameData.frame = renderOverlays(result.frame, result);
    } else {
        frameData.frame = result.frame.clone();
    }

    // Resize frame to target resolution
    frameData.frame = resizeFrame(frameData.frame, m_config.width, m_config.height);

    // Process based on protocol
    if (m_config.protocol == StreamProtocol::MJPEG) {
        // Encode to JPEG for MJPEG streaming
        frameData.jpegData = encodeJpeg(frameData.frame);
        // Add to buffer for MJPEG clients
        addFrameToBuffer(frameData);
    } else if (m_config.protocol == StreamProtocol::RTMP) {
        // Send frame directly to RTMP stream
        if (m_rtmpStreaming.load()) {
            encodeAndSendRtmpFrame(frameData.frame);
        }
    }

    // Update FPS statistics
    m_frameCount.fetch_add(1);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_lastFpsUpdate).count();
    if (elapsed >= 1.0) {
        double fps = m_frameCount.load() / elapsed;
        m_streamFps.store(fps);
        m_frameCount.store(0);
        m_lastFpsUpdate = now;
    }
}

bool Streamer::startServer() {
    if (m_serverRunning.load()) {
        return true;
    }

    if (!setupHttpServer()) {
        return false;
    }

    m_serverRunning.store(true);
    m_serverThread = std::thread(&Streamer::serverThread, this);

    LOG_INFO() << "[Streamer] HTTP server started on port " << m_config.port;
    return true;
}

void Streamer::stopServer() {
    if (!m_serverRunning.load()) {
        return;
    }

    LOG_INFO() << "[Streamer] Stopping HTTP server...";
    m_serverRunning.store(false);

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& client : m_clients) {
            if (client->socket >= 0) {
                close(client->socket);
            }
        }
        m_clients.clear();
    }
}

bool Streamer::isServerRunning() const {
    return m_serverRunning.load();
}

size_t Streamer::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
}

double Streamer::getStreamFps() const {
    return m_streamFps.load();
}

std::string Streamer::getStreamUrl() const {
    if (m_config.protocol == StreamProtocol::MJPEG) {
        return "http://localhost:" + std::to_string(m_config.port) + m_config.endpoint;
    } else if (m_config.protocol == StreamProtocol::RTMP) {
        return m_config.rtmpUrl;
    }
    return "";
}

bool Streamer::setupHttpServer() {
    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0) {
        LOG_ERROR() << "[Streamer] Failed to create socket: " << strerror(errno);
        return false;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR() << "[Streamer] Failed to set socket options: " << strerror(errno);
        close(m_serverSocket);
        return false;
    }

    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_config.port);

    if (bind(m_serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR() << "[Streamer] Failed to bind socket to port " << m_config.port
                  << ": " << strerror(errno);
        close(m_serverSocket);
        return false;
    }

    // Listen for connections
    if (listen(m_serverSocket, LISTEN_BACKLOG) < 0) {
        LOG_ERROR() << "[Streamer] Failed to listen on socket: " << strerror(errno);
        close(m_serverSocket);
        return false;
    }

    return true;
}

void Streamer::serverThread() {
    LOG_INFO() << "[Streamer] Server thread started";

    while (m_serverRunning.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (m_serverRunning.load()) {
                LOG_ERROR() << "[Streamer] Failed to accept connection: " << strerror(errno);
            }
            continue;
        }

        // Check client limit
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            if (m_clients.size() >= MAX_CLIENTS) {
                LOG_INFO() << "[Streamer] Maximum clients reached, rejecting connection";
                close(clientSocket);
                continue;
            }
        }

        // Get client address
        std::string clientAddrStr = inet_ntoa(clientAddr.sin_addr);
        LOG_INFO() << "[Streamer] New client connected: " << clientAddrStr;

        // Create client thread
        m_clientThreads.emplace_back(&Streamer::clientHandlerThread, this, clientSocket, clientAddrStr);
    }

    LOG_INFO() << "[Streamer] Server thread stopped";
}

void Streamer::clientHandlerThread(int clientSocket, const std::string& clientAddr) {
    LOG_INFO() << "[Streamer] Client handler started for " << clientAddr;

    // Add client to list
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.push_back(std::make_unique<ClientConnection>(clientSocket, clientAddr));
    }

    try {
        // Read HTTP request
        char buffer[1024] = {0};
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            LOG_ERROR() << "[Streamer] Failed to read request from " << clientAddr;
            close(clientSocket);
            return;
        }

        std::string request(buffer, bytesRead);
        handleHttpRequest(clientSocket, request);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[Streamer] Exception in client handler: " << e.what();
    }

    // Remove client from list
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.erase(
            std::remove_if(m_clients.begin(), m_clients.end(),
                [clientSocket](const std::unique_ptr<ClientConnection>& client) {
                    return client->socket == clientSocket;
                }),
            m_clients.end()
        );
    }

    close(clientSocket);
    LOG_INFO() << "[Streamer] Client handler stopped for " << clientAddr;
}

void Streamer::handleHttpRequest(int clientSocket, const std::string& request) {
    // Parse request line
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    LOG_INFO() << "[Streamer] HTTP Request: " << method << " " << path;

    // Check if this is a request for our MJPEG stream
    if (method == "GET" && path == m_config.endpoint) {
        // Send MJPEG headers
        sendHttpHeaders(clientSocket);

        // Stream frames
        while (m_serverRunning.load()) {
            FrameData frameData;
            if (getLatestFrame(frameData)) {
                sendMjpegFrame(clientSocket, frameData.jpegData);

                // Control frame rate
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / m_config.fps));
            } else {
                // No frame available, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } else if (method == "OPTIONS") {
        // Handle CORS preflight request
        std::string response = "HTTP/1.1 200 OK\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n"
                              "Content-Length: 0\r\n"
                              "\r\n";
        send(clientSocket, response.c_str(), response.length(), 0);
    } else {
        // Send 404 response
        std::string response = "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 13\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "\r\n"
                              "404 Not Found";
        send(clientSocket, response.c_str(), response.length(), 0);
    }
}

void Streamer::sendHttpHeaders(int clientSocket) {
    std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=--mjpegboundary\r\n"
        "Cache-Control: no-cache\r\n"
        "Pragma: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n";

    send(clientSocket, headers.c_str(), headers.length(), 0);
}

void Streamer::sendMjpegFrame(int clientSocket, const std::vector<uint8_t>& jpegData) {
    if (jpegData.empty()) {
        return;
    }

    // Send boundary and headers
    std::ostringstream frameHeader;
    frameHeader << "--mjpegboundary\r\n"
                << "Content-Type: image/jpeg\r\n"
                << "Content-Length: " << jpegData.size() << "\r\n"
                << "\r\n";

    std::string headerStr = frameHeader.str();
    if (send(clientSocket, headerStr.c_str(), headerStr.length(), 0) < 0) {
        return; // Client disconnected
    }

    // Send JPEG data
    if (send(clientSocket, jpegData.data(), jpegData.size(), 0) < 0) {
        return; // Client disconnected
    }

    // Send trailing CRLF
    const char* trailer = "\r\n";
    send(clientSocket, trailer, 2, 0);
}

void Streamer::frameProcessingThread() {
    LOG_INFO() << "[Streamer] Frame processing thread started";

    while (m_running.load()) {
        std::unique_lock<std::mutex> lock(m_frameBufferMutex);

        // Wait for frames or shutdown
        m_frameCondition.wait_for(lock, std::chrono::milliseconds(FRAME_TIMEOUT_MS),
            [this] { return !m_frameBuffer.empty() || !m_running.load(); });

        if (!m_running.load()) {
            break;
        }

        // Clean up old frames
        cleanupOldFrames();

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO() << "[Streamer] Frame processing thread stopped";
}

cv::Mat Streamer::renderOverlays(const cv::Mat& frame, const FrameResult& result) {
    cv::Mat overlayFrame = frame.clone();

    // Draw ROIs first (background layer)
    drawROIs(overlayFrame, result);

    // Draw detections with enhanced information
    if (!result.detections.empty()) {
        drawDetections(overlayFrame, result.detections, result.labels);
    }

    // Draw tracking IDs
    if (!result.trackIds.empty() && result.trackIds.size() == result.detections.size()) {
        drawTrackingIds(overlayFrame, result.detections, result.trackIds);
    }

    // Draw face recognition results
    if (!result.faceIds.empty()) {
        drawFaceRecognition(overlayFrame, result.detections, result.faceIds);
    }

    // Draw license plate recognition results
    if (!result.plateNumbers.empty()) {
        drawLicensePlates(overlayFrame, result.detections, result.plateNumbers);
    }

    // Draw behavior events and alarms
    if (!result.events.empty()) {
        drawBehaviorEvents(overlayFrame, result.events);
    }

    // Draw timestamp (top layer)
    drawTimestamp(overlayFrame);

    // Draw system info overlay
    drawSystemInfo(overlayFrame, result);

    return overlayFrame;
}

void Streamer::drawDetections(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                             const std::vector<std::string>& labels) {
    for (size_t i = 0; i < detections.size(); ++i) {
        const cv::Rect& bbox = detections[i];

        // Use different colors for different object types
        cv::Scalar bboxColor = getDetectionColor(i, labels.size() > i ? labels[i] : "");

        // Draw bounding box with thicker line for better visibility
        cv::rectangle(frame, bbox, bboxColor, 3);

        // Draw corner markers for better visibility
        int cornerSize = 15;
        drawCornerMarkers(frame, bbox, bboxColor, cornerSize);

        // Draw label with confidence if available
        if (i < labels.size() && !labels[i].empty()) {
            std::string label = labels[i];

            // Extract confidence if present (format: "class:confidence")
            std::string displayText = label;
            if (label.find(':') != std::string::npos) {
                size_t colonPos = label.find(':');
                std::string className = label.substr(0, colonPos);
                std::string confidence = label.substr(colonPos + 1);
                displayText = className + " (" + confidence + ")";
            }

            int baseline = 0;
            cv::Size textSize = cv::getTextSize(displayText, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);

            cv::Point textOrg(bbox.x, bbox.y - 8);

            // Draw background rectangle with rounded corners effect
            cv::Rect textRect(textOrg.x - 3, textOrg.y - textSize.height - baseline - 3,
                             textSize.width + 6, textSize.height + baseline + 6);
            cv::rectangle(frame, textRect, bboxColor, -1);
            cv::rectangle(frame, textRect, cv::Scalar(0, 0, 0), 1);

            cv::putText(frame, displayText, textOrg, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                       cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }
    }
}

void Streamer::drawTrackingIds(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                              const std::vector<int>& trackIds) {
    for (size_t i = 0; i < std::min(detections.size(), trackIds.size()); ++i) {
        const cv::Rect& bbox = detections[i];
        int trackId = trackIds[i];

        if (trackId >= 0) {
            std::string idText = "ID:" + std::to_string(trackId);
            cv::Point textPos(bbox.x + bbox.width - 60, bbox.y + 20);

            cv::putText(frame, idText, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                       cv::Scalar(255, 255, 0), 1);
        }
    }
}

void Streamer::drawTimestamp(cv::Mat& frame) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();

    std::string timestamp = ss.str();
    cv::Point textPos(10, frame.rows - 10);

    // Draw background rectangle
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(timestamp, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);
    cv::rectangle(frame,
                 cv::Point(textPos.x - 5, textPos.y - textSize.height - baseline - 5),
                 cv::Point(textPos.x + textSize.width + 5, textPos.y + baseline + 5),
                 cv::Scalar(0, 0, 0), -1);

    cv::putText(frame, timestamp, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.6,
               cv::Scalar(255, 255, 255), 1);
}

std::vector<uint8_t> Streamer::encodeJpeg(const cv::Mat& frame) {
    std::vector<uint8_t> jpegData;

    if (frame.empty()) {
        return jpegData;
    }

    // Set JPEG compression parameters
    std::vector<int> compressionParams;
    compressionParams.push_back(cv::IMWRITE_JPEG_QUALITY);
    compressionParams.push_back(m_config.quality);

    // Encode frame to JPEG
    if (!cv::imencode(".jpg", frame, jpegData, compressionParams)) {
        LOG_ERROR() << "[Streamer] Failed to encode frame to JPEG";
        jpegData.clear();
    }

    return jpegData;
}

cv::Mat Streamer::resizeFrame(const cv::Mat& frame, int targetWidth, int targetHeight) {
    if (frame.empty()) {
        return frame;
    }

    // Check if resize is needed
    if (frame.cols == targetWidth && frame.rows == targetHeight) {
        return frame;
    }

    cv::Mat resizedFrame;
    cv::resize(frame, resizedFrame, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_LINEAR);

    return resizedFrame;
}

void Streamer::addFrameToBuffer(const FrameData& frameData) {
    std::lock_guard<std::mutex> lock(m_frameBufferMutex);

    // Add frame to buffer
    m_frameBuffer.push(frameData);

    // Remove old frames if buffer is full
    while (m_frameBuffer.size() > MAX_BUFFER_SIZE) {
        m_frameBuffer.pop();
    }

    // Notify waiting threads
    m_frameCondition.notify_all();
}

bool Streamer::getLatestFrame(FrameData& frameData) {
    std::lock_guard<std::mutex> lock(m_frameBufferMutex);

    if (m_frameBuffer.empty()) {
        return false;
    }

    // Get the latest frame (back of queue)
    frameData = m_frameBuffer.back();

    // Clear older frames, keep only the latest
    while (m_frameBuffer.size() > 1) {
        m_frameBuffer.pop();
    }

    return true;
}

void Streamer::cleanupOldFrames() {
    // This method is called from frameProcessingThread with lock already held
    auto now = std::chrono::steady_clock::now();

    while (!m_frameBuffer.empty()) {
        const auto& frontFrame = m_frameBuffer.front();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - frontFrame.timestamp).count();

        if (age > FRAME_TIMEOUT_MS) {
            m_frameBuffer.pop();
        } else {
            break; // Frames are ordered by time, so we can stop here
        }
    }
}

// RTMP streaming methods
bool Streamer::startRtmpStream() {
    if (m_rtmpStreaming.load()) {
        return true;
    }

    if (m_config.rtmpUrl.empty()) {
        LOG_ERROR() << "[Streamer] RTMP URL not configured";
        return false;
    }

    if (!setupRtmpEncoder()) {
        LOG_ERROR() << "[Streamer] Failed to setup RTMP encoder";
        return false;
    }

    m_rtmpStreaming.store(true);
    m_rtmpStreamingThread = std::thread(&Streamer::rtmpStreamingThread, this);

    LOG_INFO() << "[Streamer] RTMP stream started to " << m_config.rtmpUrl;
    return true;
}

void Streamer::stopRtmpStream() {
    if (!m_rtmpStreaming.load()) {
        return;
    }

    LOG_INFO() << "[Streamer] Stopping RTMP stream...";
    m_rtmpStreaming.store(false);

    // Cleanup RTMP encoder
    cleanupRtmpEncoder();
}

bool Streamer::isRtmpStreaming() const {
    return m_rtmpStreaming.load();
}

bool Streamer::isStreamHealthy() const {
    if (m_config.protocol == StreamProtocol::MJPEG) {
        return isServerRunning();
    } else if (m_config.protocol == StreamProtocol::RTMP) {
        return isRtmpStreaming();
    }
    return false;
}

bool Streamer::setupRtmpEncoder() {
    std::lock_guard<std::mutex> lock(m_rtmpMutex);

    // Allocate format context
    int ret = avformat_alloc_output_context2(&m_rtmpFormatContext, nullptr, "flv", m_config.rtmpUrl.c_str());
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to allocate output context: " << ffmpeg_error_string(ret);
        return false;
    }

    // Find H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOG_ERROR() << "[Streamer] H.264 encoder not found";
        return false;
    }

    // Create video stream
    m_rtmpStream = avformat_new_stream(m_rtmpFormatContext, nullptr);
    if (!m_rtmpStream) {
        LOG_ERROR() << "[Streamer] Failed to create video stream";
        return false;
    }

    // Allocate codec context
    m_rtmpCodecContext = avcodec_alloc_context3(codec);
    if (!m_rtmpCodecContext) {
        LOG_ERROR() << "[Streamer] Failed to allocate codec context";
        return false;
    }

    // Set codec parameters
    m_rtmpCodecContext->codec_id = AV_CODEC_ID_H264;
    m_rtmpCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    m_rtmpCodecContext->width = m_config.width;
    m_rtmpCodecContext->height = m_config.height;
    m_rtmpCodecContext->time_base = {1, m_config.fps};
    m_rtmpCodecContext->framerate = {m_config.fps, 1};
    m_rtmpCodecContext->bit_rate = m_config.bitrate;
    m_rtmpCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_rtmpCodecContext->gop_size = m_config.fps; // I-frame every second

    // Set H.264 specific options for low latency
    av_opt_set(m_rtmpCodecContext->priv_data, "preset", "ultrafast", 0);
    av_opt_set(m_rtmpCodecContext->priv_data, "tune", "zerolatency", 0);

    // Some formats want stream headers to be separate
    if (m_rtmpFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_rtmpCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open codec
    ret = avcodec_open2(m_rtmpCodecContext, codec, nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to open codec: " << ffmpeg_error_string(ret);
        return false;
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(m_rtmpStream->codecpar, m_rtmpCodecContext);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to copy codec parameters: " << ffmpeg_error_string(ret);
        return false;
    }

    m_rtmpStream->time_base = m_rtmpCodecContext->time_base;

    // Allocate frame
    m_rtmpFrame = av_frame_alloc();
    if (!m_rtmpFrame) {
        LOG_ERROR() << "[Streamer] Failed to allocate frame";
        return false;
    }

    m_rtmpFrame->format = m_rtmpCodecContext->pix_fmt;
    m_rtmpFrame->width = m_rtmpCodecContext->width;
    m_rtmpFrame->height = m_rtmpCodecContext->height;

    ret = av_frame_get_buffer(m_rtmpFrame, 0);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to allocate frame buffer: " << ffmpeg_error_string(ret);
        return false;
    }

    // Initialize SWS context for color conversion
    m_swsContext = sws_getContext(
        m_config.width, m_config.height, AV_PIX_FMT_BGR24,
        m_config.width, m_config.height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!m_swsContext) {
        LOG_ERROR() << "[Streamer] Failed to initialize SWS context";
        return false;
    }

    // Open output file
    if (!(m_rtmpFormatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_rtmpFormatContext->pb, m_config.rtmpUrl.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOG_ERROR() << "[Streamer] Failed to open output URL: " << ffmpeg_error_string(ret);
            return false;
        }
    }

    // Write stream header
    ret = avformat_write_header(m_rtmpFormatContext, nullptr);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to write header: " << ffmpeg_error_string(ret);
        return false;
    }

    m_rtmpFrameCount = 0;
    LOG_INFO() << "[Streamer] RTMP encoder setup complete";
    return true;
}

void Streamer::cleanupRtmpEncoder() {
    std::lock_guard<std::mutex> lock(m_rtmpMutex);

    // Write trailer
    if (m_rtmpFormatContext) {
        av_write_trailer(m_rtmpFormatContext);
    }

    // Free resources
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_rtmpFrame) {
        av_frame_free(&m_rtmpFrame);
    }

    if (m_rtmpCodecContext) {
        avcodec_free_context(&m_rtmpCodecContext);
    }

    if (m_rtmpFormatContext) {
        if (!(m_rtmpFormatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_rtmpFormatContext->pb);
        }
        avformat_free_context(m_rtmpFormatContext);
        m_rtmpFormatContext = nullptr;
    }

    m_rtmpStream = nullptr;
    LOG_INFO() << "[Streamer] RTMP encoder cleanup complete";
}

bool Streamer::encodeAndSendRtmpFrame(const cv::Mat& frame) {
    if (!m_rtmpStreaming.load() || frame.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_rtmpMutex);

    if (!m_rtmpFormatContext || !m_rtmpCodecContext || !m_rtmpFrame || !m_swsContext) {
        return false;
    }

    // Convert OpenCV Mat to AVFrame
    const uint8_t* srcData[1] = { frame.data };
    int srcLinesize[1] = { static_cast<int>(frame.step[0]) };

    int ret = sws_scale(m_swsContext, srcData, srcLinesize, 0, frame.rows,
                       m_rtmpFrame->data, m_rtmpFrame->linesize);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to convert frame: " << ffmpeg_error_string(ret);
        return false;
    }

    // Set frame timestamp
    m_rtmpFrame->pts = m_rtmpFrameCount++;

    // Encode frame
    ret = avcodec_send_frame(m_rtmpCodecContext, m_rtmpFrame);
    if (ret < 0) {
        LOG_ERROR() << "[Streamer] Failed to send frame to encoder: " << ffmpeg_error_string(ret);
        return false;
    }

    // Receive encoded packets
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR() << "[Streamer] Failed to allocate packet";
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_rtmpCodecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOG_ERROR() << "[Streamer] Failed to receive packet: " << ffmpeg_error_string(ret);
            av_packet_free(&packet);
            return false;
        }

        // Rescale packet timestamp
        av_packet_rescale_ts(packet, m_rtmpCodecContext->time_base, m_rtmpStream->time_base);
        packet->stream_index = m_rtmpStream->index;

        // Write packet
        ret = av_interleaved_write_frame(m_rtmpFormatContext, packet);
        if (ret < 0) {
            LOG_ERROR() << "[Streamer] Failed to write packet: " << ffmpeg_error_string(ret);
            av_packet_free(&packet);
            return false;
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    return true;
}

void Streamer::rtmpStreamingThread() {
    LOG_INFO() << "[Streamer] RTMP streaming thread started";

    while (m_rtmpStreaming.load()) {
        // This thread is mainly for monitoring and cleanup
        // Actual frame encoding happens in processFrame()
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO() << "[Streamer] RTMP streaming thread stopped";
}

// Enhanced overlay methods for Task 39
cv::Scalar Streamer::getDetectionColor(size_t index, const std::string& label) {
    // Define colors for different object types
    static const std::vector<cv::Scalar> colors = {
        cv::Scalar(0, 255, 0),    // Green - person
        cv::Scalar(255, 0, 0),    // Blue - vehicle
        cv::Scalar(0, 0, 255),    // Red - face
        cv::Scalar(255, 255, 0),  // Cyan - bicycle
        cv::Scalar(255, 0, 255),  // Magenta - motorcycle
        cv::Scalar(0, 255, 255),  // Yellow - bus/truck
        cv::Scalar(128, 0, 128),  // Purple - animal
        cv::Scalar(255, 165, 0),  // Orange - other
    };

    // Color based on object type if available
    if (!label.empty()) {
        std::string lowerLabel = label;
        std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);

        if (lowerLabel.find("person") != std::string::npos) return colors[0];
        if (lowerLabel.find("car") != std::string::npos || lowerLabel.find("vehicle") != std::string::npos) return colors[1];
        if (lowerLabel.find("face") != std::string::npos) return colors[2];
        if (lowerLabel.find("bicycle") != std::string::npos) return colors[3];
        if (lowerLabel.find("motorcycle") != std::string::npos) return colors[4];
        if (lowerLabel.find("bus") != std::string::npos || lowerLabel.find("truck") != std::string::npos) return colors[5];
    }

    // Fallback to index-based color
    return colors[index % colors.size()];
}

void Streamer::drawCornerMarkers(cv::Mat& frame, const cv::Rect& bbox, const cv::Scalar& color, int size) {
    int thickness = 3;

    // Top-left corner
    cv::line(frame, cv::Point(bbox.x, bbox.y), cv::Point(bbox.x + size, bbox.y), color, thickness);
    cv::line(frame, cv::Point(bbox.x, bbox.y), cv::Point(bbox.x, bbox.y + size), color, thickness);

    // Top-right corner
    cv::line(frame, cv::Point(bbox.x + bbox.width, bbox.y), cv::Point(bbox.x + bbox.width - size, bbox.y), color, thickness);
    cv::line(frame, cv::Point(bbox.x + bbox.width, bbox.y), cv::Point(bbox.x + bbox.width, bbox.y + size), color, thickness);

    // Bottom-left corner
    cv::line(frame, cv::Point(bbox.x, bbox.y + bbox.height), cv::Point(bbox.x + size, bbox.y + bbox.height), color, thickness);
    cv::line(frame, cv::Point(bbox.x, bbox.y + bbox.height), cv::Point(bbox.x, bbox.y + bbox.height - size), color, thickness);

    // Bottom-right corner
    cv::line(frame, cv::Point(bbox.x + bbox.width, bbox.y + bbox.height), cv::Point(bbox.x + bbox.width - size, bbox.y + bbox.height), color, thickness);
    cv::line(frame, cv::Point(bbox.x + bbox.width, bbox.y + bbox.height), cv::Point(bbox.x + bbox.width, bbox.y + bbox.height - size), color, thickness);
}

void Streamer::drawROIs(cv::Mat& frame, const FrameResult& result) {
    // Task 73: Draw active ROIs with priority-based color coding
    if (result.activeROIs.empty()) {
        return;
    }

    // Create overlay for semi-transparent fill
    cv::Mat overlay = frame.clone();

    for (const auto& roi : result.activeROIs) {
        if (roi.polygon.size() < 3) {
            continue; // Skip invalid polygons
        }

        // Get priority-based color (BGR format for OpenCV)
        cv::Scalar borderColor, fillColor;
        std::string priorityText;

        switch (roi.priority) {
            case 1:
                borderColor = cv::Scalar(0, 255, 0);      // Green
                fillColor = cv::Scalar(0, 255, 0, 80);    // Green with transparency
                priorityText = "Low";
                break;
            case 2:
                borderColor = cv::Scalar(0, 255, 255);    // Yellow
                fillColor = cv::Scalar(0, 255, 255, 80);  // Yellow with transparency
                priorityText = "Med-Low";
                break;
            case 3:
                borderColor = cv::Scalar(0, 165, 255);    // Orange
                fillColor = cv::Scalar(0, 165, 255, 80);  // Orange with transparency
                priorityText = "Medium";
                break;
            case 4:
                borderColor = cv::Scalar(0, 100, 255);    // Red-Orange
                fillColor = cv::Scalar(0, 100, 255, 80);  // Red-Orange with transparency
                priorityText = "High";
                break;
            case 5:
                borderColor = cv::Scalar(0, 0, 255);      // Red
                fillColor = cv::Scalar(0, 0, 255, 80);    // Red with transparency
                priorityText = "Critical";
                break;
            default:
                borderColor = cv::Scalar(255, 255, 255);  // White
                fillColor = cv::Scalar(255, 255, 255, 80); // White with transparency
                priorityText = "Default";
                break;
        }

        // Fill ROI polygon with semi-transparent color
        std::vector<std::vector<cv::Point>> contours = {roi.polygon};
        cv::fillPoly(overlay, contours, fillColor);

        // Draw ROI border
        cv::polylines(frame, roi.polygon, true, borderColor, 2);

        // Find top-left point for label placement
        cv::Point labelPos = roi.polygon[0];
        for (const auto& point : roi.polygon) {
            if (point.y < labelPos.y || (point.y == labelPos.y && point.x < labelPos.x)) {
                labelPos = point;
            }
        }

        // Draw ROI name and priority
        std::string roiLabel = roi.name + " (P" + std::to_string(roi.priority) + " - " + priorityText + ")";
        cv::putText(frame, roiLabel, cv::Point(labelPos.x, labelPos.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, borderColor, 2);

        // Draw time restriction info if applicable
        if (!roi.start_time.empty() && !roi.end_time.empty()) {
            std::string timeInfo = "Active: " + roi.start_time + "-" + roi.end_time;
            cv::putText(frame, timeInfo, cv::Point(labelPos.x, labelPos.y - 30),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, borderColor, 1);
        }

        // Draw ROI ID for debugging
        std::string idInfo = "ID: " + roi.id;
        cv::putText(frame, idInfo, cv::Point(labelPos.x, labelPos.y + 20),
                   cv::FONT_HERSHEY_SIMPLEX, 0.4, borderColor, 1);
    }

    // Blend overlay with original frame for semi-transparent effect
    cv::addWeighted(frame, 0.7, overlay, 0.3, 0, frame);
}

void Streamer::drawFaceRecognition(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                                  const std::vector<std::string>& faceIds) {
    for (size_t i = 0; i < std::min(detections.size(), faceIds.size()); ++i) {
        if (!faceIds[i].empty() && faceIds[i] != "unknown") {
            const cv::Rect& bbox = detections[i];

            // Draw face recognition result below the detection box
            std::string faceText = "Face: " + faceIds[i];
            cv::Point textPos(bbox.x, bbox.y + bbox.height + 20);

            // Background for face text
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(faceText, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::rectangle(frame,
                         cv::Point(textPos.x - 2, textPos.y - textSize.height - baseline - 2),
                         cv::Point(textPos.x + textSize.width + 2, textPos.y + baseline + 2),
                         cv::Scalar(255, 0, 0), -1);

            cv::putText(frame, faceText, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                       cv::Scalar(255, 255, 255), 1);
        }
    }
}

void Streamer::drawLicensePlates(cv::Mat& frame, const std::vector<cv::Rect>& detections,
                                const std::vector<std::string>& plateNumbers) {
    for (size_t i = 0; i < std::min(detections.size(), plateNumbers.size()); ++i) {
        if (!plateNumbers[i].empty()) {
            const cv::Rect& bbox = detections[i];

            // Draw license plate result below the detection box
            std::string plateText = "Plate: " + plateNumbers[i];
            cv::Point textPos(bbox.x, bbox.y + bbox.height + 40);

            // Background for plate text
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(plateText, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            cv::rectangle(frame,
                         cv::Point(textPos.x - 2, textPos.y - textSize.height - baseline - 2),
                         cv::Point(textPos.x + textSize.width + 2, textPos.y + baseline + 2),
                         cv::Scalar(0, 255, 0), -1);

            cv::putText(frame, plateText, textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                       cv::Scalar(0, 0, 0), 1);
        }
    }
}

void Streamer::drawBehaviorEvents(cv::Mat& frame, const std::vector<BehaviorEvent>& events) {
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];

        // Draw alarm indicator in top-right corner
        cv::Point alarmPos(frame.cols - 200, 30 + i * 30);

        // Choose color based on event type
        cv::Scalar eventColor;
        std::string eventText;

        // TODO: Use actual event type from BehaviorEvent structure
        // For now, use placeholder event types
        eventText = "ALARM: Intrusion Detected";
        eventColor = cv::Scalar(0, 0, 255); // Red for alarm

        // Draw alarm background
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(eventText, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &baseline);
        cv::rectangle(frame,
                     cv::Point(alarmPos.x - 5, alarmPos.y - textSize.height - baseline - 5),
                     cv::Point(alarmPos.x + textSize.width + 5, alarmPos.y + baseline + 5),
                     eventColor, -1);

        // Draw alarm text
        cv::putText(frame, eventText, alarmPos, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                   cv::Scalar(255, 255, 255), 2);

        // Draw blinking effect for active alarms
        static int blinkCounter = 0;
        blinkCounter = (blinkCounter + 1) % 20;
        if (blinkCounter < 10) {
            // Draw border around entire frame for alarm
            cv::rectangle(frame, cv::Point(0, 0), cv::Point(frame.cols-1, frame.rows-1),
                         eventColor, 8);
        }
    }
}

void Streamer::drawSystemInfo(cv::Mat& frame, const FrameResult& result) {
    // Draw system information in top-left corner
    std::vector<std::string> infoLines;

    // Add detection count
    infoLines.push_back("Detections: " + std::to_string(result.detections.size()));

    // Add tracking count
    if (!result.trackIds.empty()) {
        infoLines.push_back("Tracked: " + std::to_string(result.trackIds.size()));
    }

    // Add face recognition count
    int recognizedFaces = 0;
    for (const auto& faceId : result.faceIds) {
        if (!faceId.empty() && faceId != "unknown") {
            recognizedFaces++;
        }
    }
    if (recognizedFaces > 0) {
        infoLines.push_back("Faces: " + std::to_string(recognizedFaces));
    }

    // Add license plate count
    int recognizedPlates = 0;
    for (const auto& plate : result.plateNumbers) {
        if (!plate.empty()) {
            recognizedPlates++;
        }
    }
    if (recognizedPlates > 0) {
        infoLines.push_back("Plates: " + std::to_string(recognizedPlates));
    }

    // Add stream info
    infoLines.push_back("Stream: " + m_sourceId);
    infoLines.push_back("FPS: " + std::to_string(static_cast<int>(m_streamFps.load())));

    // Draw info background
    if (!infoLines.empty()) {
        int maxWidth = 0;
        int totalHeight = 0;
        int lineHeight = 25;

        for (const auto& line : infoLines) {
            int baseline = 0;
            cv::Size textSize = cv::getTextSize(line, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
            maxWidth = std::max(maxWidth, textSize.width);
            totalHeight += lineHeight;
        }

        // Draw semi-transparent background
        cv::rectangle(frame, cv::Point(5, 5),
                     cv::Point(maxWidth + 15, totalHeight + 15),
                     cv::Scalar(0, 0, 0, 128), -1);

        // Draw info lines
        for (size_t i = 0; i < infoLines.size(); ++i) {
            cv::Point textPos(10, 25 + i * lineHeight);
            cv::putText(frame, infoLines[i], textPos, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                       cv::Scalar(255, 255, 255), 1);
        }
    }
}
