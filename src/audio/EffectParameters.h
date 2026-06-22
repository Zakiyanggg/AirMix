#pragma once

#include "controls/ControlState.h"

namespace airmix::audio {

struct EffectParameters {
    controls::EffectMode activeMode = controls::EffectMode::HighPass;

    float highPassCutoffHz = 120.0f;
    float lowPassCutoffHz = 18000.0f;

    float gateDepth = 0.0f;
    float gateDutyCycle = 0.50f;

    float delayMs = 250.0f;
    float delayFeedback = 0.28f;
    float delayMix = 0.0f;
    bool delayThrowTrigger = false;

    bool stutterTrigger = false;
    float stutterDurationSec = 0.25f;
    float stutterCaptureSec = 0.08f;

    float volume = 1.0f;
    float tempoBpm = 120.0f;

    bool hold = false;
    bool bypass = false;
};

}  // namespace airmix::audio

