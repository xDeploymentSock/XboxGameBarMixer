#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>

namespace fuser {
enum class worker_stage : std::uint8_t {
    idle,
    preparing_decode,
    submitting_decode,
    publishing_frame,
    waiting_frame,
    resizing,
    waiting_present,
    acquiring_frame,
    drawing,
    presenting,
    gpu_backoff,
    releasing_frame,
    stopped
};

[[nodiscard]] constexpr const char* worker_stage_name(worker_stage stage) noexcept {
    switch (stage) {
    case worker_stage::idle:
        return "idle";
    case worker_stage::preparing_decode:
        return "preparing-decode";
    case worker_stage::submitting_decode:
        return "submitting-decode";
    case worker_stage::publishing_frame:
        return "publishing-frame";
    case worker_stage::waiting_frame:
        return "waiting-frame";
    case worker_stage::resizing:
        return "resizing";
    case worker_stage::waiting_present:
        return "waiting-present";
    case worker_stage::acquiring_frame:
        return "acquiring-frame";
    case worker_stage::drawing:
        return "drawing";
    case worker_stage::presenting:
        return "presenting";
    case worker_stage::gpu_backoff:
        return "gpu-backoff";
    case worker_stage::releasing_frame:
        return "releasing-frame";
    case worker_stage::stopped:
        return "stopped";
    }
    return "unknown";
}

struct worker_activity_snapshot {
    worker_stage stage{worker_stage::idle};
    std::uint64_t age_microseconds{};
    bool observed{};
};

// One worker writes; readers never take that worker's mutex or spin. Packing
// stage and entry time into one atomic prevents mixed-stage age readings.
// Age includes scheduling/legitimate waits; it is not a blocked-thread verdict.
class worker_activity final {
public:
    using clock = std::chrono::steady_clock;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

    void enter(worker_stage stage, clock::time_point now = clock::now()) noexcept {
        record_.store((tick(now) << stage_bits) | static_cast<std::uint8_t>(stage),
                      std::memory_order_relaxed);
    }

    [[nodiscard]] worker_activity_snapshot
    snapshot(clock::time_point now = clock::now()) const noexcept {
        const auto record = record_.load(std::memory_order_relaxed);
        if (!record) {
            return {};
        }
        const auto entered = record >> stage_bits;
        const auto current = tick(now);
        return {static_cast<worker_stage>(record & stage_mask),
                current >= entered ? current - entered : 0,
                true};
    }

    // Only after joining the writer, before starting a new session.
    void reset() noexcept {
        record_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr unsigned stage_bits = 8;
    static constexpr std::uint64_t stage_mask = (1ULL << stage_bits) - 1;
    static constexpr std::uint64_t max_tick =
        std::numeric_limits<std::uint64_t>::max() >> stage_bits;
    static std::uint64_t tick(clock::time_point now) noexcept {
        const auto value =
            std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return value > 0 ? std::min(static_cast<std::uint64_t>(value), max_tick) : 0;
    }
    std::atomic<std::uint64_t> record_{};
};

// Restore the outer worker stage on early return or exception as well.
class worker_activity_scope final {
public:
    worker_activity_scope(worker_activity& activity,
                          worker_stage enter,
                          worker_stage leave) noexcept
        : activity_{activity}, leave_{leave} {
        activity_.enter(enter);
    }
    ~worker_activity_scope() {
        activity_.enter(leave_);
    }
    worker_activity_scope(const worker_activity_scope&) = delete;
    worker_activity_scope& operator=(const worker_activity_scope&) = delete;

private:
    worker_activity& activity_;
    worker_stage leave_;
};
}
