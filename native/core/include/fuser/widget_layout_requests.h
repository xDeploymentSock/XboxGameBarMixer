#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace fuser {

enum class widget_layout_action { fit_monitor, reset_position };

struct widget_layout_request {
    widget_layout_action action;
    std::uint64_t revision;
};

// Only explicit user actions queue a resize. Host events can cancel stale work,
// but must never undo a user's drag by creating a new layout request.
class widget_layout_requests {
public:
    void request(widget_layout_action action) noexcept {
        pending_ = widget_layout_request{action, ++revision_};
    }

    [[nodiscard]] bool has_pending() const noexcept { return pending_.has_value(); }

    [[nodiscard]] std::optional<widget_layout_request> take() noexcept {
        return std::exchange(pending_, std::nullopt);
    }

    [[nodiscard]] bool is_current(widget_layout_request request) const noexcept {
        return request.revision == revision_;
    }

    void invalidate() noexcept {
        ++revision_;
        pending_.reset();
    }

    void visibility_changed(bool visible) noexcept {
        if (!visible) { invalidate(); }
    }

private:
    std::uint64_t revision_{};
    std::optional<widget_layout_request> pending_;
};

} // namespace fuser
