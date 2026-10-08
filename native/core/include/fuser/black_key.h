#pragma once

#include <fuser/configuration.h>
#include <algorithm>

namespace fuser {

enum class black_key_preset { exact, noise_cutoff };

// Both presets remove background with a hard cutoff: retained pixels keep
// their decoded RGB and opaque alpha. Only the noise cutoff removes near-black.
[[nodiscard]] constexpr chroma_key_settings black_key_settings(chroma_key_settings current,
                                                               black_key_preset preset) noexcept {
    current.color = {0.0F, 0.0F, 0.0F};
    current.tolerance = preset == black_key_preset::exact ? 0.0F : 0.12F;
    current.softness = 0.0F;
    current.opacity = 1.0F;
    current.enabled = true;
    current.spill_suppression = 0.0F;
    current.recover_black_edges = false;
    return current;
}

[[nodiscard]] constexpr float video_visual_opacity(float requested, bool follow_game_bar) noexcept {
    // The video is independent of the settings card and host frame opacity.
    // A non-finite host value must not make the Composition visual invalid.
    if (!follow_game_bar || requested != requested) {
        return 1.0F;
    }
    return std::clamp(requested, 0.0F, 1.0F);
}

} // namespace fuser
