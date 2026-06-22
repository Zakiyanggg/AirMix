#include <cassert>
#include <cmath>

#include "mapping/GestureToEffectMapper.h"

int main() {
    airmix::mapping::GestureToEffectMapper mapper;

    const auto initial = mapper.update(
        airmix::motion::GestureType::None,
        {},
        {},
        0.0);
    assert(initial.activeMode == airmix::controls::EffectMode::HighPass);
    assert(!initial.bypass);

    const auto stutter = mapper.update(
        airmix::motion::GestureType::Shake,
        {.ax = 1.4f, .az = 1.4f, .gy = 180.0f},
        {},
        0.5);
    assert(stutter.activeMode == airmix::controls::EffectMode::Stutter);
    assert(stutter.stutterTrigger);

    const auto held = mapper.update(
        airmix::motion::GestureType::TiltLeft,
        {.ay = 1.0f, .az = 0.7f},
        {.holdDown = true},
        1.0);
    assert(held.hold);
    assert(held.activeMode == airmix::controls::EffectMode::Stutter);
    assert(!held.stutterTrigger);

    const auto bypassed = mapper.update(
        airmix::motion::GestureType::TiltRight,
        {.ay = -1.0f, .az = 0.7f},
        {.bypassPressed = true},
        1.5);
    assert(bypassed.bypass);

    const auto modeChanged = mapper.update(
        airmix::motion::GestureType::None,
        {},
        {.bypassPressed = true, .effectModePressed = true},
        2.0);
    assert(!modeChanged.bypass);
    assert(modeChanged.activeMode == airmix::controls::EffectMode::LowPass);

    return 0;
}

