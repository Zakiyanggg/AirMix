#pragma once

#include <algorithm>
#include <cmath>

#include "audio/AudioBuffer.h"

namespace airmix::audio::effects {

class GateEffect {
public:
    explicit GateEffect(float sampleRate = 48000.0f) : sampleRate_(sampleRate) {}

    void reset(float sampleRate) {
        sampleRate_ = sampleRate;
        phaseSec_ = 0.0f;
    }

    void setParameters(float tempoBpm, float depth, float dutyCycle) {
        tempoBpm_ = std::clamp(tempoBpm, 30.0f, 240.0f);
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        dutyCycle_ = std::clamp(dutyCycle, 0.05f, 0.95f);
    }

    void process(AudioBuffer& buffer) {
        if (depth_ <= 0.001f) {
            return;
        }

        const float beatSec = 60.0f / tempoBpm_;
        const float stepSec = 1.0f / sampleRate_;
        const float closedGain = 1.0f - depth_;

        for (auto& sample : buffer) {
            const bool open = phaseSec_ < beatSec * dutyCycle_;
            sample *= open ? 1.0f : closedGain;

            phaseSec_ += stepSec;
            if (phaseSec_ >= beatSec) {
                phaseSec_ -= beatSec;
            }
        }
    }

private:
    float sampleRate_ = 48000.0f;
    float tempoBpm_ = 120.0f;
    float depth_ = 0.0f;
    float dutyCycle_ = 0.50f;
    float phaseSec_ = 0.0f;
};

}  // namespace airmix::audio::effects

