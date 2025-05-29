#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <atomic>
#include "../core/VideoPipeline.h"  // For VideoSource definition

#ifdef HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

/**
 * @brief FFmpeg-based video decoder with GPU acceleration support
 *
 * This class handles video stream decoding from various sources:
 * - RTSP streams
 * - Local video files
 * - Network streams
 *
 * Features:
 * - Hardware-accelerated decoding (CUDA/NVDEC)
 * - Automatic format detection
 * - Frame rate control
 * - Error recovery and reconnection
 */
class FFmpegDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder();

    // Initialization
    bool initialize(const VideoSource& source);
    void cleanup();

    // Frame operations
    bool getNextFrame(cv::Mat& frame, int64_t& timestamp);
    bool seekToTimestamp(int64_t timestamp);

    // Stream control
    bool reconnect();
    bool isConnected() const;

    // Stream information
    int getWidth() const;
    int getHeight() const;
    double getFrameRate() const;
    int64_t getDuration() const;
    std::string getCodecName() const;

    // Statistics
    size_t getDecodedFrames() const;
    double getDecodeTime() const;

private:
    // Internal methods
    bool openStream();
    bool setupDecoder();
    bool setupScaler();
    void logError(const std::string& message, int errorCode = 0);

#ifdef HAVE_FFMPEG
    AVFrame* decodeFrame();
    bool convertFrame(AVFrame* avFrame, cv::Mat& cvFrame);

    // FFmpeg contexts
    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    SwsContext* m_swsContext;

    // Stream information
    int m_videoStreamIndex;
    AVStream* m_videoStream;
    const AVCodec* m_codec;

    // Frames and packets
    AVFrame* m_frame;
    AVFrame* m_frameRGB;
    AVPacket* m_packet;
    uint8_t* m_buffer;
#else
    // Stub implementation without FFmpeg
    void* m_formatContext;
    void* m_codecContext;
    void* m_swsContext;
    int m_videoStreamIndex;
    void* m_videoStream;
    void* m_codec;
    void* m_frame;
    void* m_frameRGB;
    void* m_packet;
    void* m_buffer;
#endif

    // Configuration
    VideoSource m_source;
    bool m_useHardwareDecoding;

    // State
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_initialized{false};

    // Statistics
    mutable std::atomic<size_t> m_decodedFrames{0};
    mutable std::atomic<double> m_decodeTime{0.0};

    // Timing
    std::chrono::steady_clock::time_point m_lastDecodeTime;

    // Constants
    static constexpr int BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    static constexpr int MAX_DECODE_ERRORS = 10;
    static constexpr int RECONNECT_TIMEOUT_MS = 5000;
};

/**
 * @brief RAII wrapper for FFmpeg resources
 */
class FFmpegRAII {
public:
    FFmpegRAII();
    ~FFmpegRAII();

private:
    static std::atomic<int> s_refCount;
};
