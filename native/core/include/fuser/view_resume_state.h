#pragma once

#include <cstdint>

namespace fuser {
// Only the UI thread mutates this state. An off-thread Resuming callback carries
// an immutable token back to that thread; closing/replacing a view invalidates it.
class view_resume_state final {
public:
    [[nodiscard]] std::uint64_t suspend() noexcept {
        pending_ = true;
        return ++generation_;
    }
    [[nodiscard]] bool resume(std::uint64_t token) noexcept {
        if (!pending_ || token != generation_) { return false; }
        pending_ = false;
        return true;
    }
    void invalidate() noexcept {
        ++generation_;
        pending_ = false;
    }
private:
    std::uint64_t generation_{};
    bool pending_{};
};
} // namespace fuser
