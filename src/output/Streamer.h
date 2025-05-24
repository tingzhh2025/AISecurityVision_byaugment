#pragma once

#include <string>

struct FrameResult;

class Streamer {
public:
    Streamer();
    ~Streamer();
    
    bool initialize(const std::string& sourceId);
    void processFrame(const FrameResult& result);
};
