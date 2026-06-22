#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>

namespace airmix::tempo {

class TapTempo {
public:
    void reset() {
        lastTapSec_ = -1.0;
        intervalsSec_.clear();
        bpm_ = defaultBpm_;
        valid_ = false;
    }

    void tap(double timestampSec) {
        if (lastTapSec_ >= 0.0) {
            const double intervalSec = timestampSec - lastTapSec_;
            if (intervalSec >= minIntervalSec_ && intervalSec <= maxIntervalSec_) {
                intervalsSec_.push_back(intervalSec);
                while (intervalsSec_.size() > maxIntervals_) {
                    intervalsSec_.pop_front();
                }
                recalculateBpm();
            } else {
                intervalsSec_.clear();
                valid_ = false;
                bpm_ = defaultBpm_;
            }
        }

        lastTapSec_ = timestampSec;
    }

    float bpm() const {
        return bpm_;
    }

    bool hasTempo() const {
        return valid_;
    }

private:
    void recalculateBpm() {
        if (intervalsSec_.empty()) {
            return;
        }

        double sum = 0.0;
        for (double interval : intervalsSec_) {
            sum += interval;
        }
        const double averageInterval = sum / static_cast<double>(intervalsSec_.size());
        bpm_ = static_cast<float>(60.0 / averageInterval);
        bpm_ = std::clamp(bpm_, 30.0f, 240.0f);
        valid_ = true;
    }

    static constexpr float defaultBpm_ = 120.0f;
    static constexpr double minIntervalSec_ = 0.25;
    static constexpr double maxIntervalSec_ = 2.0;
    static constexpr std::size_t maxIntervals_ = 4;

    double lastTapSec_ = -1.0;
    std::deque<double> intervalsSec_;
    float bpm_ = defaultBpm_;
    bool valid_ = false;
};

}  // namespace airmix::tempo
