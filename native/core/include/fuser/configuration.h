#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fuser {

enum class video_codec { h264, hevc, av1 };

struct stream_profile {
    std::uint32_t width{2560};
    std::uint32_t height{1440};
    std::uint32_t frames_per_second{240};
    std::uint32_t bitrate_kbps{80000};
    video_codec codec{video_codec::hevc};
};

struct host_endpoint {
    std::string address;
    std::uint16_t base_port{47989};
};

struct chroma_key_settings {
    // RGB components are normalized. Reserve this color on the source display.
    std::array<float, 3> color{0.0F, 1.0F, 0.0F};
    float tolerance{0.12F};
    float softness{0.08F};
    float spill_suppression{0.6F};
    float opacity{1.0F};
    bool enabled{true};
    // For bright artwork drawn over black: infer edge coverage from brightness.
    // Leave disabled when dark greys are intentional opaque HUD content.
    bool recover_black_edges{false};
};

struct overlay_configuration {
    host_endpoint host;
    stream_profile stream;
    chroma_key_settings key;
    // Input forwarding and audio are deliberately absent from this view-only API.
};

struct validation_issue {
    std::string field;
    std::string message;
};

[[nodiscard]] std::vector<validation_issue> validate(
    const overlay_configuration& configuration, bool require_host = true);

} // namespace fuser
