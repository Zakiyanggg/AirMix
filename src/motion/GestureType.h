#pragma once

namespace airmix::motion {

enum class GestureType {
    None,
    Shake,
    TiltLeft,
    TiltRight,
    Punch,
    Hit,
    Rotate,
    Lift,
    Drop,
};

inline const char* toString(GestureType gesture) {
    switch (gesture) {
        case GestureType::None:
            return "none";
        case GestureType::Shake:
            return "shake";
        case GestureType::TiltLeft:
            return "tilt left";
        case GestureType::TiltRight:
            return "tilt right";
        case GestureType::Punch:
            return "punch";
        case GestureType::Hit:
            return "hit";
        case GestureType::Rotate:
            return "rotate";
        case GestureType::Lift:
            return "lift";
        case GestureType::Drop:
            return "drop";
    }
    return "unknown";
}

}  // namespace airmix::motion

