#pragma once

#include <cmath>

#include "GestureType.h"
#include "IMUInput.h"

namespace airmix::motion {

class MotionDetector {
public:
    GestureType detect(const IMUSample& sample) const {
        const float accelMagnitude = std::sqrt(sample.ax * sample.ax + sample.ay * sample.ay + sample.az * sample.az);
        const float dynamicAccel = std::abs(accelMagnitude - 1.0f);
        const float gyroMagnitude = std::sqrt(sample.gx * sample.gx + sample.gy * sample.gy + sample.gz * sample.gz);

        if (sample.ax > 1.65f) {
            return GestureType::Punch;
        }
        if (sample.az < -0.65f || (sample.az < 0.15f && dynamicAccel > 0.70f)) {
            return GestureType::Hit;
        }
        if (dynamicAccel > 1.05f && gyroMagnitude > 140.0f) {
            return GestureType::Shake;
        }
        if (gyroMagnitude > 260.0f) {
            return GestureType::Rotate;
        }
        if (sample.az > 1.55f) {
            return GestureType::Lift;
        }
        if (sample.az < 0.35f && accelMagnitude < 0.75f) {
            return GestureType::Drop;
        }
        if (sample.ay > 0.45f) {
            return GestureType::TiltLeft;
        }
        if (sample.ay < -0.45f) {
            return GestureType::TiltRight;
        }
        return GestureType::None;
    }
};

}  // namespace airmix::motion

