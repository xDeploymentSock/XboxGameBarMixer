#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <fuser/configuration.h>

namespace fuser {

using monotonic_time = std::chrono::steady_clock::time_point;
enum class pixel_format { nv12, p010 };
enum class color_matrix { bt601, bt709, bt2020 };
enum class color_range { limited, full };

struct encoded_frame {
    std::vector<std::byte> bytes;
    video_codec codec{video_codec::hevc};
    std::uint64_t sequence{};
    monotonic_time received_at{};
    // A host timestamp is meaningful only with a documented clock conversion.
    std::optional<std::chrono::microseconds> host_presentation_time;
};

class gpu_surface {
public:
    gpu_surface() = default;
    virtual ~gpu_surface() = default;
    gpu_surface(const gpu_surface&) = delete;
    gpu_surface& operator=(const gpu_surface&) = delete;
    gpu_surface(gpu_surface&&) = delete;
    gpu_surface& operator=(gpu_surface&&) = delete;
};

// The Windows adapter will derive an FFmpeg AVFrame owner from this interface.
// Keeping a texture alive is distinct from preventing its decoder-pool reuse.
class decoder_frame_lease {
public:
    decoder_frame_lease() = default;
    virtual ~decoder_frame_lease() = default;
    decoder_frame_lease(const decoder_frame_lease&) = delete;
    decoder_frame_lease& operator=(const decoder_frame_lease&) = delete;
    decoder_frame_lease(decoder_frame_lease&&) = delete;
    decoder_frame_lease& operator=(decoder_frame_lease&&) = delete;
};

struct decoded_frame {
    std::shared_ptr<const gpu_surface> surface;
    std::uint32_t width{};
    std::uint32_t height{};
    // Visible rectangle within a potentially padded hardware allocation.
    std::uint32_t source_x{};
    std::uint32_t source_y{};
    std::uint64_t sequence{};
    pixel_format format{pixel_format::nv12};
    color_matrix matrix{color_matrix::bt709};
    color_range range{color_range::limited};
    monotonic_time received_at{};
    monotonic_time decoded_at{};
};

} // namespace fuser
