#pragma once

#include <algorithm>

#include "EffectParameters.h"
#include "audio/AudioBuffer.h"
#include "audio/effects/DelayEffect.h"
#include "audio/effects/GateEffect.h"
#include "audio/effects/StutterEffect.h"
#include "audio/filters/HighPassFilter.h"
#include "audio/filters/LowPassFilter.h"

namespace airmix::audio {

class EffectProcessor {
public:
    explicit EffectProcessor(AudioConfig config)
        : config_(config),
          lowPass_(static_cast<float>(config.sampleRate)),
          highPass_(static_cast<float>(config.sampleRate)),
          gate_(static_cast<float>(config.sampleRate)),
          delay_(static_cast<float>(config.sampleRate)),
          stutter_(static_cast<float>(config.sampleRate)) {}

    void reset() {
        lowPass_.reset(static_cast<float>(config_.sampleRate));
        highPass_.reset(static_cast<float>(config_.sampleRate));
        gate_.reset(static_cast<float>(config_.sampleRate));
        delay_.reset(static_cast<float>(config_.sampleRate));
        stutter_.reset(static_cast<float>(config_.sampleRate));
    }

    void process(AudioBuffer& buffer, const EffectParameters& parameters) {
        if (buffer.empty() || parameters.bypass) {
            return;
        }

        if (parameters.stutterTrigger) {
            stutter_.triggerFromBuffer(buffer, parameters.stutterDurationSec, parameters.stutterCaptureSec);
        }

        switch (parameters.activeMode) {
            case controls::EffectMode::HighPass:
                highPass_.setCutoff(parameters.highPassCutoffHz);
                highPass_.process(buffer);
                break;
            case controls::EffectMode::LowPass:
                lowPass_.setCutoff(parameters.lowPassCutoffHz);
                lowPass_.process(buffer);
                break;
            case controls::EffectMode::Gate:
                gate_.setParameters(parameters.tempoBpm, parameters.gateDepth, parameters.gateDutyCycle);
                gate_.process(buffer);
                break;
            case controls::EffectMode::Delay:
                delay_.setParameters(parameters.delayMs, parameters.delayFeedback, parameters.delayMix);
                delay_.process(buffer);
                break;
            case controls::EffectMode::Stutter:
                if (parameters.stutterTrigger || !stutter_.isActive()) {
                    stutter_.triggerFromBuffer(buffer, parameters.stutterDurationSec, parameters.stutterCaptureSec);
                }
                stutter_.process(buffer);
                break;
        }

        if (parameters.delayThrowTrigger) {
            delay_.triggerThrow(parameters.delayMs, parameters.delayFeedback, parameters.delayMix);
        }
        if (parameters.activeMode != controls::EffectMode::Delay && delay_.hasTail()) {
            delay_.process(buffer);
        }

        if (parameters.activeMode != controls::EffectMode::Stutter && stutter_.isActive()) {
            stutter_.process(buffer);
        }

        for (auto& sample : buffer) {
            sample = clampSample(sample * parameters.volume);
        }
    }

private:
    AudioConfig config_;
    filters::LowPassFilter lowPass_;
    filters::HighPassFilter highPass_;
    effects::GateEffect gate_;
    effects::DelayEffect delay_;
    effects::StutterEffect stutter_;
};

}  // namespace airmix::audio

