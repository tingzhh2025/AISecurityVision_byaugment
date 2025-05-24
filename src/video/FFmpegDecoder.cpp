#include "FFmpegDecoder.h"
#include "../core/TaskManager.h"
#include <iostream>
#include <chrono>

#ifdef HAVE_FFMPEG
std::atomic<int> FFmpegRAII::s_refCount{0};

FFmpegRAII::FFmpegRAII() {
    if (s_refCount.fetch_add(1) == 0) {
        std::cout << "[FFmpeg] Initializing FFmpeg libraries" << std::endl;
        avformat_network_init();
    }
}

FFmpegRAII::~FFmpegRAII() {
    if (s_refCount.fetch_sub(1) == 1) {
        std::cout << "[FFmpeg] Cleaning up FFmpeg libraries" << std::endl;
        avformat_network_deinit();
    }
}
#else
FFmpegRAII::FFmpegRAII() {
    std::cout << "[FFmpeg] FFmpeg not available - using stub implementation" << std::endl;
}

FFmpegRAII::~FFmpegRAII() {}
#endif

FFmpegDecoder::FFmpegDecoder() 
    : m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_swsContext(nullptr)
    , m_videoStreamIndex(-1)
    , m_videoStream(nullptr)
    , m_codec(nullptr)
    , m_frame(nullptr)
    , m_frameRGB(nullptr)
    , m_packet(nullptr)
    , m_buffer(nullptr)
    , m_useHardwareDecoding(true) {
    
    static FFmpegRAII ffmpegInit;
}

FFmpegDecoder::~FFmpegDecoder() {
    cleanup();
}

bool FFmpegDecoder::initialize(const VideoSource& source) {
    m_source = source;
    std::cout << "[FFmpegDecoder] Initializing decoder for: " << source.url << std::endl;
    
#ifdef HAVE_FFMPEG
    // TODO: Implement real FFmpeg initialization
    std::cout << "[FFmpegDecoder] FFmpeg implementation not complete yet" << std::endl;
    return false;
#else
    std::cout << "[FFmpegDecoder] Using stub implementation (FFmpeg not available)" << std::endl;
    m_initialized.store(true);
    m_connected.store(true);
    return true;
#endif
}

bool FFmpegDecoder::getNextFrame(cv::Mat& frame, int64_t& timestamp) {
    if (!m_connected.load() || !m_initialized.load()) {
        return false;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef HAVE_FFMPEG
    // TODO: Implement real frame decoding
    return false;
#else
    // Stub implementation - create a test frame
    frame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::putText(frame, "Test Frame - No FFmpeg", cv::Point(50, 240), 
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
    
    // Add timestamp
    std::string timeStr = "Frame: " + std::to_string(m_decodedFrames.load());
    cv::putText(frame, timeStr, cv::Point(50, 280), 
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 1);
#endif
    
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto end = std::chrono::high_resolution_clock::now();
    m_decodeTime.store(std::chrono::duration<double, std::milli>(end - start).count());
    m_decodedFrames.fetch_add(1);
    
    // Simulate frame rate
    std::this_thread::sleep_for(std::chrono::milliseconds(40)); // ~25 FPS
    
    return true;
}

bool FFmpegDecoder::openStream() {
#ifdef HAVE_FFMPEG
    // TODO: Implement FFmpeg stream opening
    return false;
#else
    return true;
#endif
}

bool FFmpegDecoder::setupDecoder() {
#ifdef HAVE_FFMPEG
    // TODO: Implement FFmpeg decoder setup
    return false;
#else
    return true;
#endif
}

bool FFmpegDecoder::setupScaler() {
#ifdef HAVE_FFMPEG
    // TODO: Implement FFmpeg scaler setup
    return false;
#else
    return true;
#endif
}

void FFmpegDecoder::cleanup() {
#ifdef HAVE_FFMPEG
    // TODO: Implement FFmpeg cleanup
#endif
    m_connected.store(false);
    m_initialized.store(false);
}

bool FFmpegDecoder::reconnect() {
    std::cout << "[FFmpegDecoder] Attempting to reconnect..." << std::endl;
    cleanup();
    return initialize(m_source);
}

void FFmpegDecoder::logError(const std::string& message, int errorCode) {
    if (errorCode != 0) {
        std::cerr << "[FFmpegDecoder] " << message << " (error code: " << errorCode << ")" << std::endl;
    } else {
        std::cerr << "[FFmpegDecoder] " << message << std::endl;
    }
}

// Getters
bool FFmpegDecoder::isConnected() const { return m_connected.load(); }
int FFmpegDecoder::getWidth() const { return 640; }
int FFmpegDecoder::getHeight() const { return 480; }
double FFmpegDecoder::getFrameRate() const { return 25.0; }
int64_t FFmpegDecoder::getDuration() const { return 0; }
std::string FFmpegDecoder::getCodecName() const { return "stub"; }
size_t FFmpegDecoder::getDecodedFrames() const { return m_decodedFrames.load(); }
double FFmpegDecoder::getDecodeTime() const { return m_decodeTime.load(); }
bool FFmpegDecoder::seekToTimestamp(int64_t timestamp) { return false; }
