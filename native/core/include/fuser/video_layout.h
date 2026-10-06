#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace fuser {

struct video_rectangle {
    double x{}, y{}, width{}, height{};
};

// View-pixel rectangle inside the widget. The bottom reservation is in raw pixels.
[[nodiscard]] inline std::optional<video_rectangle> usable_video_rectangle(
    video_rectangle client, double monitor_width, double monitor_height,
    double scale, double reserved_bottom_pixels) {
    if (!std::isfinite(client.x) || !std::isfinite(client.y)
        || !std::isfinite(client.width) || !std::isfinite(client.height)
        || client.width < 0 || client.height < 0
        || !std::isfinite(monitor_width) || !std::isfinite(monitor_height)
        || monitor_width <= 0 || monitor_height <= 0
        || !std::isfinite(scale) || scale <= 0
        || !std::isfinite(reserved_bottom_pixels) || reserved_bottom_pixels < 0
        || reserved_bottom_pixels / scale >= monitor_height) {
        throw std::invalid_argument{"Invalid usable video area or taskbar height."};
    }
    const auto left = std::max(0.0, client.x);
    const auto top = std::max(0.0, client.y);
    const auto right = std::min(monitor_width, client.x + client.width);
    const auto bottom = std::min(monitor_height - reserved_bottom_pixels / scale,
                                 client.y + client.height);
    if (right <= left || bottom <= top) { return std::nullopt; }
    return video_rectangle{left - client.x, top - client.y, right - left, bottom - top};
}

} // namespace fuser
