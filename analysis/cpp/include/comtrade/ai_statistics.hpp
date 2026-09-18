#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace comtrade::ai {
// Finite engineering values only. RMS includes DC and is sample-weighted.
// Scaling avoids overflowing x*x even for values near DBL_MAX.
struct Statistics {
    std::uint64_t count = 0;
    std::int64_t minTime = 0, maxTime = 0;
    double minimum = 0, maximum = 0, mean = 0, scale = 0, squares = 0;
    void add(double value, std::int64_t time) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Statistics requires finite values");
        if (!count || value < minimum) {
            minimum = value;
            minTime = time;
        }
        if (!count || value > maximum) {
            maximum = value;
            maxTime = time;
        }
        ++count;
        mean = mean * (double(count - 1) / double(count)) + value / double(count);
        const auto magnitude = std::abs(value);
        if (magnitude > scale) {
            const auto ratio = scale / magnitude;
            squares = 1 + squares * ratio * ratio;
            scale = magnitude;
        } else if (scale) {
            const auto ratio = magnitude / scale;
            squares += ratio * ratio;
        }
    }
    void merge(const Statistics &other) {
        if (!other.count)
            return;
        if (!count) {
            *this = other;
            return;
        }
        if (other.minimum < minimum) {
            minimum = other.minimum;
            minTime = other.minTime;
        }
        if (other.maximum > maximum) {
            maximum = other.maximum;
            maxTime = other.maxTime;
        }
        const auto total = count + other.count;
        mean = mean * (double(count) / double(total)) + other.mean * (double(other.count) / double(total));
        const auto combinedScale = std::max(scale, other.scale);
        if (combinedScale)
            squares = squares * std::pow(scale / combinedScale, 2) +
                      other.squares * std::pow(other.scale / combinedScale, 2);
        scale = combinedScale;
        count = total;
    }
    double rms() const noexcept {
        return count ? scale * std::sqrt(std::min(1.0, squares / double(count))) : 0;
    }
};
} // namespace comtrade::ai
