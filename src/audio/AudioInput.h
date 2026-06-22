#pragma once

#include <cmath>

#include "AudioBuffer.h"

namespace airmix::audio {

class AudioInput {
public:
    virtual ~AudioInput() = default;

    virtual bool read(AudioBuffer& outBuffer) = 0;
    virtual AudioConfig config() const = 0;
};

class SimulatedAudioInput final : public AudioInput {
public:
    explicit SimulatedAudioInput(AudioConfig config, float frequencyHz = 220.0f)
        : config_(config), frequencyHz_(frequencyHz) {}

    bool read(AudioBuffer& outBuffer) override {
        outBuffer.resize(config_.blockSize * static_cast<std::size_t>(config_.channels));

        constexpr float kTwoPi = 6.28318530717958647692f;
        const float phaseStep = kTwoPi * frequencyHz_ / static_cast<float>(config_.sampleRate);
        const float overtoneStep = phaseStep * 2.01f;

        for (std::size_t frame = 0; frame < config_.blockSize; ++frame) {
            const float sample = 0.55f * std::sin(phase_) + 0.18f * std::sin(overtonePhase_);
            phase_ += phaseStep;
            overtonePhase_ += overtoneStep;

            if (phase_ >= kTwoPi) {
                phase_ -= kTwoPi;
            }
            if (overtonePhase_ >= kTwoPi) {
                overtonePhase_ -= kTwoPi;
            }

            for (int channel = 0; channel < config_.channels; ++channel) {
                outBuffer[frame * static_cast<std::size_t>(config_.channels) + static_cast<std::size_t>(channel)] = sample;
            }
        }

        return true;
    }

    AudioConfig config() const override {
        return config_;
    }

private:
    AudioConfig config_;
    float frequencyHz_ = 220.0f;
    float phase_ = 0.0f;
    float overtonePhase_ = 0.0f;

    // TODO: Replace this simulator with a Bluetooth A2DP sink, USB audio, line-in,
    // or platform-specific audio driver when moving to hardware.
};

}  // namespace airmix::audio

