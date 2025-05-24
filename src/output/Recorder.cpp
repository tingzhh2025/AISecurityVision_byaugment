#include "Recorder.h"
#include "../core/VideoPipeline.h"
#include <iostream>

Recorder::Recorder() {}
Recorder::~Recorder() {}

bool Recorder::initialize(const std::string& sourceId) {
    std::cout << "[Recorder] Initialized for " << sourceId << " (stub)" << std::endl;
    return true;
}

void Recorder::processFrame(const FrameResult& result) {
    // Stub implementation
}
