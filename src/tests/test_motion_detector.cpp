#include <cassert>

#include "motion/MotionDetector.h"

int main() {
    airmix::motion::MotionDetector detector;

    assert(detector.detect({}) == airmix::motion::GestureType::None);
    assert(detector.detect({.ay = 0.7f, .az = 0.75f}) == airmix::motion::GestureType::TiltLeft);
    assert(detector.detect({.ay = -0.7f, .az = 0.75f}) == airmix::motion::GestureType::TiltRight);
    assert(detector.detect({.ax = 1.8f, .az = 0.8f}) == airmix::motion::GestureType::Punch);
    assert(detector.detect({.az = -0.8f}) == airmix::motion::GestureType::Hit);
    assert(detector.detect({.gx = 300.0f}) == airmix::motion::GestureType::Rotate);
    assert(detector.detect({.ax = 1.55f, .ay = 1.0f, .az = 1.45f, .gy = 180.0f}) == airmix::motion::GestureType::Shake);
    assert(detector.detect({.az = 1.7f}) == airmix::motion::GestureType::Lift);
    assert(detector.detect({.az = 0.2f}) == airmix::motion::GestureType::Drop);

    return 0;
}
