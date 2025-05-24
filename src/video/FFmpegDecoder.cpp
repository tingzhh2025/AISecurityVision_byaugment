#include "FFmpegDecoder.h"
#include "../core/TaskManager.h"
#include <iostream>
#include <chrono>

std::atomic<int> FFmpegRAII::s_refCount{0};

FFmpegRAII::FFmpegRAII() {
    if (s_refCount.fetch_add(1) == 0) {
        // Initialize FFmpeg on first use
        std::cout << "[FFmpeg] Initializing FFmpeg libraries" << std::endl;
        // av_register_all(); // Deprecated in newer FFmpeg versions
        avformat_network_init();
    }
}

FFmpegRAII::~FFmpegRAII() {
    if (s_refCount.fetch_sub(1) == 1) {
        // Cleanup FFmpeg on last destruction
        std::cout << "[FFmpeg] Cleaning up FFmpeg libraries" << std::endl;
        avformat_network_deinit();
    }
}

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
    
    static FFmpegRAII ffmpegInit; // Ensure FFmpeg is initialized
}

FFmpegDecoder::~FFmpegDecoder() {
    cleanup();
}

bool FFmpegDecoder::initialize(const VideoSource& source) {
    m_source = source;
    
    std::cout << "[FFmpegDecoder] Initializing decoder for: " << source.url << std::endl;
    
    try {
        if (!openStream()) {
            logError("Failed to open stream");
            return false;
        }
        
        if (!setupDecoder()) {
            logError("Failed to setup decoder");
            return false;
        }
        
        if (!setupScaler()) {
            logError("Failed to setup scaler");
            return false;
        }
        
        m_initialized.store(true);
        m_connected.store(true);
        
        std::cout << "[FFmpegDecoder] Decoder initialized successfully" << std::endl;
        std::cout << "[FFmpegDecoder] Stream info: " << getWidth() << "x" << getHeight() 
                  << " @ " << getFrameRate() << " FPS, codec: " << getCodecName() << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception during initialization: " + std::string(e.what()));
        cleanup();
        return false;
    }
}

bool FFmpegDecoder::openStream() {
    // Allocate format context
    m_formatContext = avformat_alloc_context();
    if (!m_formatContext) {
        return false;
    }
    
    // Set options for RTSP streams
    AVDictionary* options = nullptr;
    if (m_source.protocol == "rtsp") {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "max_delay", "500000", 0);
        av_dict_set(&options, "timeout", "5000000", 0);
    }
    
    // Open input
    int ret = avformat_open_input(&m_formatContext, m_source.url.c_str(), nullptr, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        logError("Could not open input", ret);
        return false;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        logError("Could not find stream information", ret);
        return false;
    }
    
    // Find video stream
    m_videoStreamIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &m_codec, 0);
    if (m_videoStreamIndex < 0) {
        logError("Could not find video stream");
        return false;
    }
    
    m_videoStream = m_formatContext->streams[m_videoStreamIndex];
    
    return true;
}

bool FFmpegDecoder::setupDecoder() {
    // Allocate codec context
    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        return false;
    }
    
    // Copy codec parameters
    int ret = avcodec_parameters_to_context(m_codecContext, m_videoStream->codecpar);
    if (ret < 0) {
        logError("Could not copy codec parameters", ret);
        return false;
    }
    
    // Try hardware decoding first
    if (m_useHardwareDecoding) {
        // TODO: Setup CUDA hardware decoding
        // For now, fall back to software decoding
        std::cout << "[FFmpegDecoder] Hardware decoding not implemented, using software" << std::endl;
    }
    
    // Open codec
    ret = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (ret < 0) {
        logError("Could not open codec", ret);
        return false;
    }
    
    // Allocate frames
    m_frame = av_frame_alloc();
    m_frameRGB = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_frame || !m_frameRGB || !m_packet) {
        return false;
    }
    
    return true;
}

bool FFmpegDecoder::setupScaler() {
    // Setup software scaler for format conversion
    m_swsContext = sws_getContext(
        m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!m_swsContext) {
        return false;
    }
    
    // Allocate buffer for RGB frame
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_codecContext->width, m_codecContext->height, 1);
    m_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    
    if (!m_buffer) {
        return false;
    }
    
    // Setup frame data pointers
    av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer, 
                        AV_PIX_FMT_BGR24, m_codecContext->width, m_codecContext->height, 1);
    
    return true;
}

bool FFmpegDecoder::getNextFrame(cv::Mat& frame, int64_t& timestamp) {
    if (!m_connected.load() || !m_initialized.load()) {
        return false;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    AVFrame* decodedFrame = decodeFrame();
    if (!decodedFrame) {
        return false;
    }
    
    if (!convertFrame(decodedFrame, frame)) {
        return false;
    }
    
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto end = std::chrono::high_resolution_clock::now();
    m_decodeTime.store(std::chrono::duration<double, std::milli>(end - start).count());
    m_decodedFrames.fetch_add(1);
    
    return true;
}

AVFrame* FFmpegDecoder::decodeFrame() {
    int ret;
    
    while (true) {
        ret = av_read_frame(m_formatContext, m_packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // End of stream
                return nullptr;
            } else {
                logError("Error reading frame", ret);
                return nullptr;
            }
        }
        
        if (m_packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_codecContext, m_packet);
            if (ret < 0) {
                av_packet_unref(m_packet);
                continue;
            }
            
            ret = avcodec_receive_frame(m_codecContext, m_frame);
            av_packet_unref(m_packet);
            
            if (ret == 0) {
                return m_frame;
            } else if (ret == AVERROR(EAGAIN)) {
                continue;
            } else {
                logError("Error decoding frame", ret);
                return nullptr;
            }
        }
        
        av_packet_unref(m_packet);
    }
}

bool FFmpegDecoder::convertFrame(AVFrame* avFrame, cv::Mat& cvFrame) {
    if (!avFrame || !m_swsContext) {
        return false;
    }
    
    // Convert frame to BGR24
    sws_scale(m_swsContext, avFrame->data, avFrame->linesize, 0, m_codecContext->height,
              m_frameRGB->data, m_frameRGB->linesize);
    
    // Create OpenCV Mat from converted frame
    cvFrame = cv::Mat(m_codecContext->height, m_codecContext->width, CV_8UC3, m_frameRGB->data[0]);
    
    return !cvFrame.empty();
}

void FFmpegDecoder::cleanup() {
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    if (m_buffer) {
        av_free(m_buffer);
        m_buffer = nullptr;
    }
    
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    
    if (m_frameRGB) {
        av_frame_free(&m_frameRGB);
    }
    
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }
    
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    
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
        char errorBuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(errorCode, errorBuf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "[FFmpegDecoder] " << message << ": " << errorBuf << std::endl;
    } else {
        std::cerr << "[FFmpegDecoder] " << message << std::endl;
    }
}

// Getters
bool FFmpegDecoder::isConnected() const { return m_connected.load(); }
int FFmpegDecoder::getWidth() const { return m_codecContext ? m_codecContext->width : 0; }
int FFmpegDecoder::getHeight() const { return m_codecContext ? m_codecContext->height : 0; }
double FFmpegDecoder::getFrameRate() const { 
    return m_videoStream ? av_q2d(m_videoStream->r_frame_rate) : 0.0; 
}
int64_t FFmpegDecoder::getDuration() const { 
    return m_formatContext ? m_formatContext->duration : 0; 
}
std::string FFmpegDecoder::getCodecName() const { 
    return m_codec ? m_codec->name : "unknown"; 
}
size_t FFmpegDecoder::getDecodedFrames() const { return m_decodedFrames.load(); }
double FFmpegDecoder::getDecodeTime() const { return m_decodeTime.load(); }
