#pragma once

#include <string>

struct FrameResult;

class Recorder {
public:
    Recorder();
    ~Recorder();
    
    bool initialize(const std::string& sourceId);
    void processFrame(const FrameResult& result);
};
