#include "Streamer.h"
#include "../core/VideoPipeline.h"
#include <iostream>

Streamer::Streamer() {}
Streamer::~Streamer() {}

bool Streamer::initialize(const std::string& sourceId) {
    std::cout << "[Streamer] Initialized for " << sourceId << " (stub)" << std::endl;
    return true;
}

void Streamer::processFrame(const FrameResult& result) {
    // Stub implementation
}
