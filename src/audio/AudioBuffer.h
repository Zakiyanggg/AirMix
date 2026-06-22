#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace airmix::audio {

using Sample = float;
using AudioBuffer = std::vector<Sample>;

struct AudioConfig {
    int sampleRate = 48000;
    std::size_t blockSize = 128;
    int channels = 1;
};

inline Sample clampSample(Sample value) {
    return std::clamp(value, -1.0f, 1.0f);
}

inline float bufferRms(const AudioBuffer& buffer) {
    if (buffer.empty()) {
        return 0.0f;
    }

    double sumSquares = 0.0;
    for (Sample sample : buffer) {
        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }
    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(buffer.size())));
}

inline float bufferPeak(const AudioBuffer& buffer) {
    float peak = 0.0f;
    for (Sample sample : buffer) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

}  // namespace airmix::audio

