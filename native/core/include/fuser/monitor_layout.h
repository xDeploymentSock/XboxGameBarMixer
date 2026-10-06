#pragma once

#include <cmath>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace fuser {

struct monitor_extent {
    float width;
    float height;
};

struct monitor_viewport {
    double left;
    double top;
    float width;
    float height;
};

// Return a full-size inner viewport only when the client contains the monitor.
// A clipped client must never be treated as a request to shrink the video.
[[nodiscard]] inline std::optional<monitor_viewport> contained_monitor_viewport(
    double x, double y, double width, double height, monitor_extent monitor) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
        || !std::isfinite(monitor.width) || !std::isfinite(monitor.height)
        || monitor.width <= 0.0F || monitor.height <= 0.0F || width <= 0.0 || height <= 0.0
        || x > 0.0 || y > 0.0 || x + width < monitor.width || y + height < monitor.height) {
        return std::nullopt;
    }
    return monitor_viewport{-x, -y, monitor.width, monitor.height};
}

[[nodiscard]] inline std::uint32_t parse_widget_dimension(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos) {
        text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        std::uint32_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error == std::errc{} && end == text.data() + text.size() && value > 0) { return value; }
    }
    throw std::invalid_argument{"Enter a positive whole number for each overlay dimension."};
}

// Game Bar sizes content in view pixels; swap-chain dimensions use raw pixels.
[[nodiscard]] inline monitor_extent monitor_view_extent(
    std::uint32_t raw_width, std::uint32_t raw_height, double scale) {
    if (raw_width == 0 || raw_height == 0 || !std::isfinite(scale) || scale <= 0.0) {
        throw std::invalid_argument{"The display has no valid dimensions or DPI scale."};
    }
    return {static_cast<float>(raw_width / scale), static_cast<float>(raw_height / scale)};
}

[[nodiscard]] inline monitor_extent widget_view_extent(
    std::uint32_t raw_width, std::uint32_t raw_height, double scale) {
    const auto extent = monitor_view_extent(raw_width, raw_height, scale);
    if (raw_width > 16384 || raw_height > 16384 || extent.width < 240.0F || extent.height < 240.0F
        || extent.width > 7680.0F || extent.height > 4320.0F) {
        throw std::invalid_argument{"Overlay size must be 240 x 240 to 7680 x 4320 view pixels at this display's scaling, and at most 16384 physical pixels per dimension."};
    }
    return extent;
}

struct monitor_edge_gaps {
    double left;
    double top;
    double right;
    double bottom;
};

[[nodiscard]] inline monitor_edge_gaps uncovered_monitor_edges(
    double x, double y, double width, double height, monitor_extent monitor, double scale) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height)
        || !std::isfinite(monitor.width) || !std::isfinite(monitor.height)
        || !std::isfinite(scale) || scale <= 0.0 || width <= 0.0 || height <= 0.0
        || monitor.width <= 0.0F || monitor.height <= 0.0F) {
        throw std::invalid_argument{"Cannot measure the overlay's edges from invalid bounds."};
    }
    return {std::clamp(x, 0.0, static_cast<double>(monitor.width)) * scale,
            std::clamp(y, 0.0, static_cast<double>(monitor.height)) * scale,
            std::clamp(monitor.width - x - width, 0.0, static_cast<double>(monitor.width)) * scale,
            std::clamp(monitor.height - y - height, 0.0, static_cast<double>(monitor.height)) * scale};
}

[[nodiscard]] inline bool matches_monitor_extent(
    double width, double height, monitor_extent expected, double scale) noexcept {
    // Permit at most one physical pixel of layout rounding at fractional DPI.
    return std::isfinite(width) && std::isfinite(height) && std::isfinite(scale) && scale > 0.0
        && std::abs(width - expected.width) * scale <= 1.0
        && std::abs(height - expected.height) * scale <= 1.0;
}

[[nodiscard]] inline bool matches_monitor_bounds(
    double x, double y, double width, double height, monitor_extent expected, double scale) noexcept {
    return matches_monitor_extent(width, height, expected, scale)
        && std::isfinite(x) && std::isfinite(y)
        && std::abs(x) * scale <= 1.0 && std::abs(y) * scale <= 1.0;
}

} // namespace fuser
