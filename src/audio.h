#pragma once

#include <memory>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    void init();
    void cleanup();

    void update(float engineRpm, float lateralSpeed, bool grounded, float throttle);
    void triggerShift();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
