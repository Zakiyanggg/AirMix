#pragma once

#include <algorithm>
#include <cmath>

#include "audio/AudioBuffer.h"

namespace airmix::audio::filters {

class LowPassFilter {
public:
    explicit LowPassFilter(float sampleRate = 48000.0f) : sampleRate_(sampleRate) {
        setCutoff(18000.0f);
    }

    void reset(float sampleRate) {
        sampleRate_ = sampleRate;
        state_ = 0.0f;
        setCutoff(cutoffHz_);
    }

    void setCutoff(float cutoffHz) {
        const float nyquist = sampleRate_ * 0.5f;
        cutoffHz_ = std::clamp(cutoffHz, 20.0f, nyquist * 0.90f);

        constexpr float kTwoPi = 6.28318530717958647692f;
        const float rc = 1.0f / (kTwoPi * cutoffHz_);
        const float dt = 1.0f / sampleRate_;
        alpha_ = dt / (rc + dt);
    }

    void process(AudioBuffer& buffer) {
        for (auto& sample : buffer) {
            state_ += alpha_ * (sample - state_);
            sample = state_;
        }
    }

    float cutoffHz() const {
        return cutoffHz_;
    }

private:
    float sampleRate_ = 48000.0f;
    float cutoffHz_ = 18000.0f;
    float alpha_ = 1.0f;
    float state_ = 0.0f;
};

}  // namespace airmix::audio::filters

