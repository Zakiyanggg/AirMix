#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "audio/AudioBuffer.h"

namespace airmix::audio::effects {

class StutterEffect {
public:
    explicit StutterEffect(float sampleRate = 48000.0f) : sampleRate_(sampleRate) {}

    void reset(float sampleRate) {
        sampleRate_ = sampleRate;
        captured_.clear();
        readIndex_ = 0;
        remainingSamples_ = 0;
    }

    void triggerFromBuffer(const AudioBuffer& source, float durationSec, float captureSec) {
        const std::size_t captureSamples = std::max<std::size_t>(
            1,
            std::min<std::size_t>(source.size(), static_cast<std::size_t>(captureSec * sampleRate_)));

        captured_.assign(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(captureSamples));
        readIndex_ = 0;
        remainingSamples_ = std::max<std::size_t>(captureSamples, static_cast<std::size_t>(durationSec * sampleRate_));
    }

    bool isActive() const {
        return remainingSamples_ > 0 && !captured_.empty();
    }

    void process(AudioBuffer& buffer) {
        if (!isActive()) {
            return;
        }

        for (auto& sample : buffer) {
            sample = captured_[readIndex_];
            readIndex_ = (readIndex_ + 1) % captured_.size();

            if (remainingSamples_ > 0) {
                --remainingSamples_;
            }
            if (remainingSamples_ == 0) {
                break;
            }
        }
    }

private:
    float sampleRate_ = 48000.0f;
    std::vector<float> captured_;
    std::size_t readIndex_ = 0;
    std::size_t remainingSamples_ = 0;
};

}  // namespace airmix::audio::effects

