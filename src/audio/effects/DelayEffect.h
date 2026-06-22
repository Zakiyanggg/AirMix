#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "audio/AudioBuffer.h"

namespace airmix::audio::effects {

class DelayEffect {
public:
    explicit DelayEffect(float sampleRate = 48000.0f, float maxDelaySec = 2.0f)
        : sampleRate_(sampleRate),
          delayLine_(static_cast<std::size_t>(sampleRate * maxDelaySec), 0.0f) {}

    void reset(float sampleRate) {
        sampleRate_ = sampleRate;
        delayLine_.assign(static_cast<std::size_t>(sampleRate_ * 2.0f), 0.0f);
        writeIndex_ = 0;
        currentMix_ = 0.0f;
    }

    void setParameters(float delayMs, float feedback, float mix) {
        delayMs_ = std::clamp(delayMs, 30.0f, 1000.0f);
        feedback_ = std::clamp(feedback, 0.0f, 0.85f);
        currentMix_ = std::clamp(mix, 0.0f, 0.90f);
        updateDelaySamples();
    }

    void triggerThrow(float delayMs, float feedback, float mix) {
        setParameters(delayMs, feedback, mix);
        tailActive_ = true;
    }

    bool hasTail() const {
        return tailActive_ || currentMix_ > 0.001f;
    }

    void process(AudioBuffer& buffer) {
        if (delayLine_.empty() || currentMix_ <= 0.001f) {
            return;
        }

        for (auto& sample : buffer) {
            const std::size_t readIndex = (writeIndex_ + delayLine_.size() - delaySamples_) % delayLine_.size();
            const float delayed = delayLine_[readIndex];
            const float input = sample;

            delayLine_[writeIndex_] = clampSample(input + delayed * feedback_);
            sample = clampSample(input * (1.0f - currentMix_) + delayed * currentMix_);

            writeIndex_ = (writeIndex_ + 1) % delayLine_.size();
        }

        if (tailActive_) {
            currentMix_ *= 0.88f;
            if (currentMix_ <= 0.01f) {
                currentMix_ = 0.0f;
                tailActive_ = false;
            }
        }
    }

private:
    void updateDelaySamples() {
        delaySamples_ = static_cast<std::size_t>((delayMs_ / 1000.0f) * sampleRate_);
        delaySamples_ = std::clamp<std::size_t>(delaySamples_, 1, delayLine_.empty() ? 1 : delayLine_.size() - 1);
    }

    float sampleRate_ = 48000.0f;
    float delayMs_ = 250.0f;
    float feedback_ = 0.28f;
    float currentMix_ = 0.0f;
    bool tailActive_ = false;
    std::vector<float> delayLine_;
    std::size_t delaySamples_ = 12000;
    std::size_t writeIndex_ = 0;
};

}  // namespace airmix::audio::effects

