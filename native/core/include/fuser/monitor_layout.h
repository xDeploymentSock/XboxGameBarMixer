#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace fuser {

struct monitor_extent {
    float width;
    float height;
};

// Game Bar sizes content in view pixels; swap-chain dimensions use raw pixels.
[[nodiscard]] inline monitor_extent monitor_view_extent(
    std::uint32_t raw_width, std::uint32_t raw_height, double scale) {
    if (raw_width == 0 || raw_height == 0 || !std::isfinite(scale) || scale <= 0.0) {
        throw std::invalid_argument{"The display has no valid dimensions or DPI scale."};
    }
    return {static_cast<float>(raw_width / scale), static_cast<float>(raw_height / scale)};
}

[[nodiscard]] inline bool matches_monitor_extent(
    double width, double height, monitor_extent expected, double scale) noexcept {
    // Permit at most one physical pixel of layout rounding at fractional DPI.
    return std::isfinite(width) && std::isfinite(height) && std::isfinite(scale) && scale > 0.0
        && std::abs(width - expected.width) * scale <= 1.0
        && std::abs(height - expected.height) * scale <= 1.0;
}

} // namespace fuser
