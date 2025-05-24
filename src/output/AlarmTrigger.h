#pragma once

struct FrameResult;

class AlarmTrigger {
public:
    AlarmTrigger();
    ~AlarmTrigger();
    
    bool initialize();
    void triggerAlarm(const FrameResult& result);
};
