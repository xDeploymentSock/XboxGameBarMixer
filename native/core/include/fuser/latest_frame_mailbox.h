#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

#include <fuser/video_frame.h>

namespace fuser {

// This boundary drops decoded display frames, never compressed reference frames.
// A decoder must process the compressed stream in its required order.
class latest_frame_mailbox final {
public:
    [[nodiscard]] bool publish(decoded_frame frame) {
        std::optional<decoded_frame> displaced;
        {
            const std::scoped_lock lock{mutex_};
            if (closed_ || !frame.surface) {
                return false;
            }
            displaced.swap(frame_);
            if (displaced) {
                ++replaced_frames_;
            }
            frame_.emplace(std::move(frame));
        }
        // The displaced GPU surface is released after the lock is gone.
        return true;
    }

    [[nodiscard]] std::optional<decoded_frame> take_latest() {
        std::optional<decoded_frame> result;
        const std::scoped_lock lock{mutex_};
        result.swap(frame_);
        return result;
    }

    // A renderer may retain a frame while GPU submission is busy. Count its
    // replacement too, so display drops are not hidden once a frame is taken.
    [[nodiscard]] bool take_latest_into(std::optional<decoded_frame>& pending) {
        std::optional<decoded_frame> displaced;
        {
            const std::scoped_lock lock{mutex_};
            if (!frame_) { return false; }
            displaced.swap(pending);
            pending.swap(frame_);
            if (displaced) { ++replaced_frames_; ++replaced_pending_frames_; }
        }
        return true;
    }

    [[nodiscard]] bool has_frame() const {
        const std::scoped_lock lock{mutex_};
        return frame_.has_value();
    }

    void close() {
        std::optional<decoded_frame> discarded;
        {
            const std::scoped_lock lock{mutex_};
            closed_ = true;
            discarded.swap(frame_);
        }
    }

    // Reopen only after stopping and joining the previous producer/consumer.
    void reset() {
        std::optional<decoded_frame> discarded;
        {
            const std::scoped_lock lock{mutex_};
            discarded.swap(frame_);
            closed_ = false;
            replaced_frames_ = 0;
            replaced_pending_frames_ = 0;
        }
    }

    [[nodiscard]] std::uint64_t replaced_frames() const {
        const std::scoped_lock lock{mutex_};
        return replaced_frames_;
    }

    [[nodiscard]] std::uint64_t replaced_pending_frames() const {
        const std::scoped_lock lock{mutex_};
        return replaced_pending_frames_;
    }

private:
    mutable std::mutex mutex_;
    std::optional<decoded_frame> frame_;
    std::uint64_t replaced_frames_{};
    std::uint64_t replaced_pending_frames_{};
    bool closed_{};
};

} // namespace fuser
