#include <fuser/configuration.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fuser {

std::vector<validation_issue> validate(const overlay_configuration& configuration,
                                     bool require_host) {
    std::vector<validation_issue> issues;
    const auto add = [&issues](std::string field, std::string message) {
        issues.push_back({std::move(field), std::move(message)});
    };
    if (require_host && (configuration.host.address.empty() ||
        configuration.host.address.find_first_not_of(" \t\r\n") == std::string::npos)) {
        add("host.address", "Enter a host IP address or hostname.");
    }
    if (configuration.host.base_port < 1029 || configuration.host.base_port > 65514) {
        add("host.base_port", "The Sunshine base port must be between 1029 and 65514.");
    }
    if (configuration.stream.width == 0 || configuration.stream.height == 0 ||
        configuration.stream.width > 8192 || configuration.stream.height > 8192 ||
        configuration.stream.width % 2 != 0 || configuration.stream.height % 2 != 0) {
        add("stream.size", "Use positive, even dimensions up to 8192 pixels for 4:2:0 video.");
    }
    if (configuration.stream.frames_per_second == 0 ||
        configuration.stream.frames_per_second > 1000) {
        add("stream.frames_per_second", "Requested FPS must be between 1 and 1000.");
    }
    // Moonlight's STREAM_CONFIGURATION stores kilobits per second in an int.
    // Reject unsigned values that would wrap before starting host control.
    if (configuration.stream.bitrate_kbps == 0 ||
        configuration.stream.bitrate_kbps > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        add("stream.bitrate_kbps", "Bitrate must be between 1 and 2147483647 kilobits per second.");
    }
    const auto unit_interval = [](float value) {
        return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
    };
    if (!std::all_of(configuration.key.color.begin(), configuration.key.color.end(), unit_interval) ||
        !unit_interval(configuration.key.tolerance) ||
        !unit_interval(configuration.key.softness) ||
        !unit_interval(configuration.key.spill_suppression) ||
        !unit_interval(configuration.key.opacity)) {
        add("key", "Color, tolerance, softness, spill suppression, and opacity must be finite values in [0, 1].");
    }
    return issues;
}

} // namespace fuser
