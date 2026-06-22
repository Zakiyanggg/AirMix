#pragma once

#include <algorithm>
#include <cmath>

#include "audio/EffectParameters.h"
#include "controls/ButtonInput.h"
#include "controls/ControlState.h"
#include "motion/GestureType.h"
#include "motion/IMUInput.h"
#include "tempo/TapTempo.h"

namespace airmix::mapping {

class GestureToEffectMapper {
public:
    audio::EffectParameters update(
        motion::GestureType gesture,
        const motion::IMUSample& imu,
        const controls::ButtonFrame& buttons,
        double timestampSec) {
        if (buttons.resetPressed) {
            reset();
        }

        if (buttons.effectModePressed) {
            control_.selectedMode = controls::nextEffectMode(control_.selectedMode);
        }

        if (buttons.tapPressed) {
            tapTempo_.tap(timestampSec);
        }

        if (buttons.bypassPressed) {
            control_.bypass = !control_.bypass;
        }

        control_.hold = buttons.holdDown;

        if (control_.hold) {
            parameters_.hold = true;
            parameters_.bypass = control_.bypass;
            parameters_.tempoBpm = tapTempo_.bpm();
            parameters_.stutterTrigger = false;
            parameters_.delayThrowTrigger = false;
            return parameters_;
        }

        auto next = parameters_;
        next.activeMode = control_.selectedMode;
        next.tempoBpm = tapTempo_.bpm();
        next.hold = false;
        next.bypass = control_.bypass;
        next.stutterTrigger = false;
        next.delayThrowTrigger = false;

        decayMomentaryParameters(next);

        if (!next.bypass) {
            applyGesture(gesture, imu, next);
        }

        parameters_ = next;
        return parameters_;
    }

    void reset() {
        control_ = controls::ControlState{};
        parameters_ = audio::EffectParameters{};
        tapTempo_.reset();
    }

private:
    static float normalize(float value, float minValue, float maxValue) {
        if (maxValue <= minValue) {
            return 0.0f;
        }
        return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
    }

    static float lerp(float a, float b, float t) {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }

    void decayMomentaryParameters(audio::EffectParameters& parameters) const {
        parameters.gateDepth *= 0.90f;
        parameters.delayMix *= 0.82f;
        parameters.volume = lerp(parameters.volume, 1.0f, 0.25f);
    }

    void applyGesture(
        motion::GestureType gesture,
        const motion::IMUSample& imu,
        audio::EffectParameters& parameters) const {
        const float beatSec = 60.0f / std::max(parameters.tempoBpm, 1.0f);

        switch (gesture) {
            case motion::GestureType::TiltLeft: {
                const float tilt = normalize(imu.ay, 0.45f, 1.10f);
                parameters.activeMode = controls::EffectMode::HighPass;
                parameters.highPassCutoffHz = lerp(180.0f, 7200.0f, tilt);
                break;
            }
            case motion::GestureType::TiltRight: {
                const float tilt = normalize(-imu.ay, 0.45f, 1.10f);
                parameters.activeMode = controls::EffectMode::LowPass;
                parameters.lowPassCutoffHz = lerp(18000.0f, 1400.0f, tilt);
                break;
            }
            case motion::GestureType::Shake:
                parameters.activeMode = controls::EffectMode::Stutter;
                parameters.stutterTrigger = true;
                parameters.stutterDurationSec = beatSec * 0.5f;
                parameters.stutterCaptureSec = beatSec * 0.125f;
                break;
            case motion::GestureType::Punch:
            case motion::GestureType::Hit:
                parameters.activeMode = controls::EffectMode::Delay;
                parameters.delayThrowTrigger = true;
                parameters.delayMs = beatSec * 500.0f;
                parameters.delayFeedback = 0.42f;
                parameters.delayMix = 0.55f;
                break;
            case motion::GestureType::Rotate: {
                const float spin = normalize(
                    std::max({std::abs(imu.gx), std::abs(imu.gy), std::abs(imu.gz)}),
                    260.0f,
                    720.0f);
                parameters.activeMode = controls::EffectMode::Gate;
                parameters.gateDepth = lerp(0.55f, 0.92f, spin);
                parameters.gateDutyCycle = 0.45f;
                break;
            }
            case motion::GestureType::Lift:
                parameters.activeMode = controls::EffectMode::HighPass;
                parameters.highPassCutoffHz = 900.0f;
                parameters.volume = 1.06f;
                break;
            case motion::GestureType::Drop:
                parameters.activeMode = controls::EffectMode::LowPass;
                parameters.lowPassCutoffHz = 2600.0f;
                parameters.volume = 0.55f;
                break;
            case motion::GestureType::None:
                break;
        }
    }

    controls::ControlState control_;
    audio::EffectParameters parameters_;
    tempo::TapTempo tapTempo_;
};

}  // namespace airmix::mapping
