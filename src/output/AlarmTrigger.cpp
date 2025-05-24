#include "AlarmTrigger.h"
#include "../core/VideoPipeline.h"
#include <iostream>

AlarmTrigger::AlarmTrigger() {}
AlarmTrigger::~AlarmTrigger() {}

bool AlarmTrigger::initialize() {
    std::cout << "[AlarmTrigger] Initialized (stub)" << std::endl;
    return true;
}

void AlarmTrigger::triggerAlarm(const FrameResult& result) {
    std::cout << "[AlarmTrigger] Alarm triggered!" << std::endl;
}
