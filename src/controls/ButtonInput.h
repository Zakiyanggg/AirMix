#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace airmix::controls {

struct ButtonFrame {
    bool effectModePressed = false;
    bool holdDown = false;
    bool tapPressed = false;
    bool bypassPressed = false;
    bool resetPressed = false;
};

class ButtonInput {
public:
    virtual ~ButtonInput() = default;

    virtual bool read(ButtonFrame& buttons) = 0;
};

class SimulatedButtonInput final : public ButtonInput {
public:
    explicit SimulatedButtonInput(std::vector<ButtonFrame> sequence) : sequence_(std::move(sequence)) {}

    bool read(ButtonFrame& buttons) override {
        if (index_ < sequence_.size()) {
            buttons = sequence_[index_++];
        } else {
            buttons = ButtonFrame{};
        }
        return true;
    }

private:
    std::vector<ButtonFrame> sequence_;
    std::size_t index_ = 0;

    // TODO: Replace this simulator with GPIO scanning, interrupts, and debounce.
};

}  // namespace airmix::controls

