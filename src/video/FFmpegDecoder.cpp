#include "FFmpegDecoder.h"
#include "../core/TaskManager.h"
#include <iostream>
#include <chrono>
#include <thread>

#include "../core/Logger.h"
using namespace AISecurityVision;
#ifdef HAVE_FFMPEG
std::atomic<int> FFmpegRAII::s_refCount{0};

FFmpegRAII::FFmpegRAII() {
    if (s_refCount.fetch_add(1) == 0) {
        LOG_INFO() << "[FFmpeg] Initializing FFmpeg libraries";
        avformat_network_init();
    }
}

FFmpegRAII::~FFmpegRAII() {
    if (s_refCount.fetch_sub(1) == 1) {
        LOG_INFO() << "[FFmpeg] Cleaning up FFmpeg libraries";
        avformat_network_deinit();
    }
}
#else
FFmpegRAII::FFmpegRAII() {
    LOG_INFO() << "[FFmpeg] FFmpeg not available - using stub implementation";
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
    LOG_INFO() << "[FFmpegDecoder] Initializing decoder for: " << source.url;

#ifdef HAVE_FFMPEG
    // Real FFmpeg implementation
    LOG_INFO() << "[FFmpegDecoder] Using real FFmpeg implementation";

    // Initialize FFmpeg contexts
    m_formatContext = nullptr;
    m_codecContext = nullptr;
    m_swsContext = nullptr;
    m_frame = nullptr;
    m_frameRGB = nullptr;
    m_packet = nullptr;
    m_buffer = nullptr;

    // Open input stream
    if (!openStream()) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to open stream: " << source.url;
        return false;
    }

    // Setup decoder
    if (!setupDecoder()) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to setup decoder";
        cleanup();
        return false;
    }

    // Setup scaler
    if (!setupScaler()) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to setup scaler";
        cleanup();
        return false;
    }

    m_initialized.store(true);
    m_connected.store(true);
    LOG_INFO() << "[FFmpegDecoder] Successfully initialized for " << source.url;
    return true;
#else
    LOG_INFO() << "[FFmpegDecoder] Using stub implementation (FFmpeg not available)";
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
    // Real FFmpeg frame decoding
    if (!m_formatContext || !m_codecContext || !m_frame || !m_frameRGB) {
        return false;
    }

    // Read packet
    int ret = av_read_frame(m_formatContext, m_packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            LOG_INFO() << "[FFmpegDecoder] End of stream reached";
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            LOG_ERROR() << "[FFmpegDecoder] Error reading frame: " << errbuf;
        }
        return false;
    }

    // Check if packet belongs to video stream
    if (m_packet->stream_index != m_videoStreamIndex) {
        av_packet_unref(m_packet);
        return getNextFrame(frame, timestamp); // Recursively try next packet
    }

    // Send packet to decoder
    ret = avcodec_send_packet(m_codecContext, m_packet);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR() << "[FFmpegDecoder] Error sending packet: " << errbuf;
        av_packet_unref(m_packet);
        return false;
    }

    // Receive frame from decoder
    ret = avcodec_receive_frame(m_codecContext, m_frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(m_packet);
            return getNextFrame(frame, timestamp); // Try next packet
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            LOG_ERROR() << "[FFmpegDecoder] Error receiving frame: " << errbuf;
            av_packet_unref(m_packet);
            return false;
        }
    }

    // Convert frame to BGR format
    sws_scale(m_swsContext, (uint8_t const * const *)m_frame->data,
              m_frame->linesize, 0, m_codecContext->height,
              m_frameRGB->data, m_frameRGB->linesize);

    // Create OpenCV Mat from frame data
    frame = cv::Mat(m_codecContext->height, m_codecContext->width, CV_8UC3, m_frameRGB->data[0], m_frameRGB->linesize[0]);
    frame = frame.clone(); // Make a copy to avoid data corruption

    // Set timestamp
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Cleanup
    av_packet_unref(m_packet);

    auto end = std::chrono::high_resolution_clock::now();
    m_decodeTime.store(std::chrono::duration<double, std::milli>(end - start).count());
    m_decodedFrames.fetch_add(1);

    return true;
#else
    // Stub implementation - create a test frame
    frame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::putText(frame, "Test Frame - No FFmpeg", cv::Point(50, 240),
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);

    // Add timestamp
    std::string timeStr = "Frame: " + std::to_string(m_decodedFrames.load());
    cv::putText(frame, timeStr, cv::Point(50, 280),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 1);

    // Add some animation - moving rectangle
    int x = (m_decodedFrames.load() * 2) % (640 - 100);
    cv::rectangle(frame, cv::Point(x, 350), cv::Point(x + 100, 400), cv::Scalar(255, 0, 0), -1);

    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto end = std::chrono::high_resolution_clock::now();
    m_decodeTime.store(std::chrono::duration<double, std::milli>(end - start).count());
    m_decodedFrames.fetch_add(1);

    // Simulate frame rate
    std::this_thread::sleep_for(std::chrono::milliseconds(40)); // ~25 FPS

    return true;
#endif
}

bool FFmpegDecoder::openStream() {
#ifdef HAVE_FFMPEG
    // Open input stream
    int ret = avformat_open_input(&m_formatContext, m_source.url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR() << "[FFmpegDecoder] Failed to open input: " << errbuf;
        return false;
    }

    // Retrieve stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR() << "[FFmpegDecoder] Failed to find stream info: " << errbuf;
        return false;
    }

    // Find video stream
    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        LOG_ERROR() << "[FFmpegDecoder] No video stream found";
        return false;
    }

    m_videoStream = m_formatContext->streams[m_videoStreamIndex];
    LOG_INFO() << "[FFmpegDecoder] Found video stream: " << m_videoStreamIndex
              << " (" << m_videoStream->codecpar->width << "x" << m_videoStream->codecpar->height << ")";

    return true;
#else
    return true;
#endif
}

bool FFmpegDecoder::setupDecoder() {
#ifdef HAVE_FFMPEG
    // Find decoder
    m_codec = avcodec_find_decoder(m_videoStream->codecpar->codec_id);
    if (!m_codec) {
        LOG_ERROR() << "[FFmpegDecoder] Codec not found";
        return false;
    }

    // Allocate codec context
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to allocate codec context";
        return false;
    }

    // Copy codec parameters
    int ret = avcodec_parameters_to_context(m_codecContext, m_videoStream->codecpar);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR() << "[FFmpegDecoder] Failed to copy codec parameters: " << errbuf;
        return false;
    }

    // Open codec
    ret = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        LOG_ERROR() << "[FFmpegDecoder] Failed to open codec: " << errbuf;
        return false;
    }

    // Allocate frames
    m_frame = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    if (!m_frame || !m_frameRGB) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to allocate frames";
        return false;
    }

    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to allocate packet";
        return false;
    }

    LOG_INFO() << "[FFmpegDecoder] Decoder setup complete: " << m_codec->name
              << " (" << m_codecContext->width << "x" << m_codecContext->height << ")";

    return true;
#else
    return true;
#endif
}

bool FFmpegDecoder::setupScaler() {
#ifdef HAVE_FFMPEG
    // Calculate buffer size for RGB frame
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_codecContext->width, m_codecContext->height, 1);
    m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    if (!m_buffer) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to allocate buffer";
        return false;
    }

    // Assign buffer to frame
    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer,
                        AV_PIX_FMT_BGR24, m_codecContext->width, m_codecContext->height, 1);

    // Initialize scaler context
    m_swsContext = sws_getContext(
        m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!m_swsContext) {
        LOG_ERROR() << "[FFmpegDecoder] Failed to initialize scaler context";
        return false;
    }

    LOG_INFO() << "[FFmpegDecoder] Scaler setup complete";
    return true;
#else
    return true;
#endif
}

void FFmpegDecoder::cleanup() {
#ifdef HAVE_FFMPEG
    // Cleanup FFmpeg resources
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_buffer) {
        av_free(m_buffer);
        m_buffer = nullptr;
    }

    if (m_frameRGB) {
        av_frame_free(&m_frameRGB);
        m_frameRGB = nullptr;
    }

    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }

    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        m_formatContext = nullptr;
    }

    LOG_INFO() << "[FFmpegDecoder] Cleanup completed";
#endif
    m_connected.store(false);
    m_initialized.store(false);
}

bool FFmpegDecoder::reconnect() {
    LOG_INFO() << "[FFmpegDecoder] Attempting to reconnect...";
    cleanup();
    return initialize(m_source);
}

void FFmpegDecoder::logError(const std::string& message, int errorCode) {
    if (errorCode != 0) {
        LOG_ERROR() << "[FFmpegDecoder] " << message << " (error code: " << errorCode << ")";
    } else {
        LOG_ERROR() << "[FFmpegDecoder] " << message;
    }
}

// Getters
bool FFmpegDecoder::isConnected() const { return m_connected.load(); }

int FFmpegDecoder::getWidth() const {
#ifdef HAVE_FFMPEG
    if (m_codecContext) {
        return m_codecContext->width;
    }
#endif
    return 640;
}

int FFmpegDecoder::getHeight() const {
#ifdef HAVE_FFMPEG
    if (m_codecContext) {
        return m_codecContext->height;
    }
#endif
    return 480;
}

double FFmpegDecoder::getFrameRate() const {
#ifdef HAVE_FFMPEG
    if (m_videoStream) {
        AVRational fps = m_videoStream->avg_frame_rate;
        if (fps.den > 0) {
            return (double)fps.num / fps.den;
        }
    }
#endif
    return 25.0;
}

int64_t FFmpegDecoder::getDuration() const {
#ifdef HAVE_FFMPEG
    if (m_formatContext) {
        return m_formatContext->duration;
    }
#endif
    return 0;
}

std::string FFmpegDecoder::getCodecName() const {
#ifdef HAVE_FFMPEG
    if (m_codec) {
        return std::string(m_codec->name);
    }
#endif
    return "stub";
}

size_t FFmpegDecoder::getDecodedFrames() const { return m_decodedFrames.load(); }
double FFmpegDecoder::getDecodeTime() const { return m_decodeTime.load(); }
bool FFmpegDecoder::seekToTimestamp(int64_t timestamp) { return false; }
