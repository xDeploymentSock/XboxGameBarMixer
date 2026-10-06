#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <fuser/stream_interfaces.h>

namespace fuser {

// Preserve an explicit selection by identity when refreshing the same host.
// A first refresh prefers Desktop; an active app never overrides this choice.
[[nodiscard]] inline std::optional<std::size_t> choose_source_application(
    std::span<const host_application> applications, const std::optional<host_application>& previous) {
    std::optional<std::size_t> selected;
    for (std::size_t index = 0; index < applications.size(); ++index) {
        const auto& app = applications[index];
        const bool matches = previous ? app.id == previous->id && app.name == previous->name : app.name == "Desktop";
        if (!matches) { continue; }
        if (selected) { return {}; } // An ambiguous Desktop entry requires a manual selection.
        selected = index;
    }
    return selected;
}

} // namespace fuser
