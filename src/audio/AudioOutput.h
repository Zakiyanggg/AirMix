#pragma once

#include "AudioBuffer.h"

namespace airmix::audio {

class AudioOutput {
public:
    virtual ~AudioOutput() = default;

    virtual bool write(const AudioBuffer& buffer) = 0;
    virtual AudioConfig config() const = 0;
};

class SimulatedAudioOutput final : public AudioOutput {
public:
    explicit SimulatedAudioOutput(AudioConfig config) : config_(config) {}

    bool write(const AudioBuffer& buffer) override {
        ++blocksWritten_;
        lastRms_ = bufferRms(buffer);
        lastPeak_ = bufferPeak(buffer);
        return true;
    }

    AudioConfig config() const override {
        return config_;
    }

    int blocksWritten() const {
        return blocksWritten_;
    }

    float lastRms() const {
        return lastRms_;
    }

    float lastPeak() const {
        return lastPeak_;
    }

private:
    AudioConfig config_;
    int blocksWritten_ = 0;
    float lastRms_ = 0.0f;
    float lastPeak_ = 0.0f;

    // TODO: Replace this simulator with an I2S codec, DAC, class-D amplifier,
    // headphone driver, or routing layer for speaker/headphone output.
};

}  // namespace airmix::audio

