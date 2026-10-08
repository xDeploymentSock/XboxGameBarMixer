#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace fuser {
struct timing_distribution {
    std::uint64_t samples{}, total_microseconds{}, max_microseconds{};
    // Upper bounds with 250 us buckets; overflow uses the observed maximum.
    std::uint64_t p95_microseconds{}, p99_microseconds{};
};

// One recording thread, concurrent readers. No allocation or locks per sample.
// Reset only after joining the recorder. Live snapshots are approximate;
// snapshots after a joined stop contain the complete distribution.
class timing_histogram final {
public:
    void record(std::uint64_t microseconds) noexcept {
        total_.fetch_add(microseconds, std::memory_order_relaxed);
        if (microseconds > maximum_.load(std::memory_order_relaxed)) {
            maximum_.store(microseconds, std::memory_order_relaxed);
        }
        const auto index =
            static_cast<std::size_t>(std::min(microseconds / bucket_width, bucket_count));
        buckets_[index].fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] timing_distribution snapshot() const noexcept {
        std::array<std::uint64_t, bucket_count + 1> counts{};
        timing_distribution result;
        for (std::size_t index = 0; index < counts.size(); ++index) {
            counts[index] = buckets_[index].load(std::memory_order_acquire);
            result.samples += counts[index];
        }
        result.total_microseconds = total_.load(std::memory_order_relaxed);
        result.max_microseconds = maximum_.load(std::memory_order_relaxed);
        if (!result.samples) {
            return result;
        }
        const auto percentile = [&](std::uint64_t rank) {
            std::uint64_t cumulative{};
            for (std::size_t index = 0; index < counts.size(); ++index) {
                cumulative += counts[index];
                if (cumulative >= rank) {
                    return index < bucket_count ? (index + 1) * bucket_width
                                                : result.max_microseconds;
                }
            }
            return result.max_microseconds;
        };
        result.p95_microseconds = percentile(result.samples - result.samples / 20);
        result.p99_microseconds = percentile(result.samples - result.samples / 100);
        return result;
    }

    void reset() noexcept {
        for (auto& count : buckets_) {
            count.store(0, std::memory_order_relaxed);
        }
        total_.store(0, std::memory_order_relaxed);
        maximum_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::uint64_t bucket_width = 250, bucket_count = 256;
    std::array<std::atomic<std::uint64_t>, bucket_count + 1> buckets_{};
    std::atomic<std::uint64_t> total_{}, maximum_{};
};
}
