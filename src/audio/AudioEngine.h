#pragma once

#include "AudioInput.h"
#include "AudioOutput.h"
#include "EffectParameters.h"
#include "EffectProcessor.h"

namespace airmix::audio {

class AudioEngine {
public:
    AudioEngine(AudioInput& input, AudioOutput& output, EffectProcessor& processor)
        : input_(input), output_(output), processor_(processor) {}

    bool processBlock(const EffectParameters& parameters) {
        AudioBuffer buffer;
        if (!input_.read(buffer)) {
            return false;
        }

        processor_.process(buffer, parameters);
        return output_.write(buffer);
    }

private:
    AudioInput& input_;
    AudioOutput& output_;
    EffectProcessor& processor_;
};

}  // namespace airmix::audio

