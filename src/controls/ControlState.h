#pragma once

namespace airmix::controls {

enum class EffectMode {
    HighPass,
    LowPass,
    Gate,
    Delay,
    Stutter,
};

inline const char* toString(EffectMode mode) {
    switch (mode) {
        case EffectMode::HighPass:
            return "High-pass";
        case EffectMode::LowPass:
            return "Low-pass";
        case EffectMode::Gate:
            return "Gate";
        case EffectMode::Delay:
            return "Delay";
        case EffectMode::Stutter:
            return "Stutter";
    }
    return "Unknown";
}

inline EffectMode nextEffectMode(EffectMode mode) {
    switch (mode) {
        case EffectMode::HighPass:
            return EffectMode::LowPass;
        case EffectMode::LowPass:
            return EffectMode::Gate;
        case EffectMode::Gate:
            return EffectMode::Delay;
        case EffectMode::Delay:
            return EffectMode::Stutter;
        case EffectMode::Stutter:
            return EffectMode::HighPass;
    }
    return EffectMode::HighPass;
}

struct ControlState {
    EffectMode selectedMode = EffectMode::HighPass;
    bool hold = false;
    bool bypass = false;
};

}  // namespace airmix::controls

