#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace airmix::motion {

struct IMUSample {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 1.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;
    float dtSec = 1.0f / 100.0f;
};

class IMUInput {
public:
    virtual ~IMUInput() = default;

    virtual bool read(IMUSample& sample) = 0;
};

class SimulatedIMUInput final : public IMUInput {
public:
    explicit SimulatedIMUInput(std::vector<IMUSample> sequence) : sequence_(std::move(sequence)) {}

    bool read(IMUSample& sample) override {
        if (index_ < sequence_.size()) {
            sample = sequence_[index_++];
        } else {
            sample = IMUSample{};
        }
        return true;
    }

private:
    std::vector<IMUSample> sequence_;
    std::size_t index_ = 0;

    // TODO: Replace this simulator with an I2C/SPI IMU driver that normalizes
    // accelerometer samples to g and gyroscope samples to degrees per second.
};

}  // namespace airmix::motion
