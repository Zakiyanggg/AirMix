#include <cassert>
#include <cmath>

#include "tempo/TapTempo.h"

namespace {

bool near(float actual, float expected, float tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    airmix::tempo::TapTempo tempo;

    assert(!tempo.hasTempo());
    assert(near(tempo.bpm(), 120.0f, 0.01f));

    tempo.tap(0.0);
    assert(!tempo.hasTempo());

    tempo.tap(0.5);
    assert(tempo.hasTempo());
    assert(near(tempo.bpm(), 120.0f, 0.01f));

    tempo.tap(1.0);
    assert(near(tempo.bpm(), 120.0f, 0.01f));

    tempo.tap(4.5);
    assert(!tempo.hasTempo());
    assert(near(tempo.bpm(), 120.0f, 0.01f));

    tempo.tap(5.0);
    assert(tempo.hasTempo());
    assert(near(tempo.bpm(), 120.0f, 0.01f));

    return 0;
}

