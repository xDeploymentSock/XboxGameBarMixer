#pragma once
#include "SunshineControl.h"
#include "../windows/D3D11Renderer.h"
#include "../windows/FFmpegDecoder.h"
#include <fuser/latest_frame_mailbox.h>
#include <fuser/timing_histogram.h>
#include <fuser/worker_activity.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string_view>

namespace fuser::streaming {
struct session_snapshot {
    pipeline_statistics counters;
    std::uint64_t decode_errors{};
    std::uint32_t last_frame_number{};
    std::uint64_t missing_frame_numbers{};
    std::uint32_t peak_decode_queue{};
    std::uint64_t decode_microseconds{}, max_decode_microseconds{};
    // Decoder callback arrival through accepted Present return; excludes host,
    // network arrival/assembly and Game Bar/monitor scanout.
    std::uint64_t render_latency_samples{}, render_microseconds{}, max_render_microseconds{};
    timing_distribution render_timing, present_intervals;
    // CPU wall time: includes scheduling/context-lock waits, not GPU execution
    // or display timing. Ready and timed-out capacity waits stay separate.
    timing_distribution decode_call_timing, ready_wait_timing, timeout_wait_timing, draw_call_timing, present_call_timing;
    worker_activity_snapshot decoder_activity, render_activity;
    std::uint64_t transport_queue_overflows{};
    std::uint64_t replaced_pending_frames{}, gpu_slot_retries{}, present_retries{}, presentation_wait_timeouts{};
    std::uint64_t receive_timing_samples{}, assembly_microseconds{}, queue_microseconds{}, max_queue_microseconds{};
    std::uint64_t host_latency_samples{}, host_latency_tenths_ms{}, zero_host_latency_frames{};
    std::uint16_t max_host_latency_tenths_ms{};
    stream_profile negotiated;
    // Monotonic start token distinguishes reconnects with the same profile.
    std::uint64_t connection_started_microseconds{};
    double seconds{};
    bool active{};
    bool finished{};
    std::string status;
};

// begin() and stop() run on a serialized MTA owner worker. cancel(), resize(),
// and snapshot() may be called from the UI. The render worker alone presents.
class overlay_session final {
public:
    explicit overlay_session(std::shared_ptr<sunshine_control> control,
        std::function<void(const std::string&)> logger = {});
    ~overlay_session();
    void prepare_action() noexcept;
    operation_result begin(const overlay_configuration&, const host_application&,
        std::shared_ptr<windows::d3d11_renderer>, int display_refresh_x100);
    void cancel() noexcept;
    void stop() noexcept;
    void resize(std::uint32_t width, std::uint32_t height);
    void set_key(const chroma_key_settings& key);
    session_snapshot snapshot() const;
private:
    static int setup(int format, int width, int height, int fps, void* context, int flags) noexcept;
    static int submit(void* unit) noexcept; // Bridge uses the typed C API in .cpp.
    static void stage_starting(int stage) noexcept;
    static void stage_failed(int stage, int error) noexcept;
    static void terminated(int error) noexcept;
    static void log_message(const char* format, ...) noexcept;
    void render() noexcept;
    void status(std::string_view text, bool terminal = false) noexcept;
    void trace(const char* phase, std::uint64_t number) noexcept;
    std::shared_ptr<sunshine_control> control_;
    std::function<void(const std::string&)> logger_;
    std::shared_ptr<windows::d3d11_renderer> renderer_;
    std::unique_ptr<windows::ffmpeg_decoder> decoder_;
    latest_frame_mailbox mailbox_;
    overlay_configuration configuration_;
    std::thread render_thread_;
    std::atomic<bool> cancelled_{}, render_stop_{}, connected_{}, finished_{};
    std::atomic<std::uint64_t> received_{}, decoded_{}, presented_{}, decode_errors_{};
    std::atomic<std::uint32_t> last_frame_number_{}, peak_decode_queue_{};
    std::atomic<std::uint64_t> missing_frame_numbers_{}, decode_microseconds_{}, max_decode_microseconds_{};
    std::atomic<std::uint64_t> render_latency_samples_{}, render_microseconds_{}, max_render_microseconds_{};
    timing_histogram render_timing_, present_intervals_;
    timing_histogram decode_call_timing_, ready_wait_timing_, timeout_wait_timing_, draw_call_timing_, present_call_timing_;
    worker_activity decoder_activity_, render_activity_;
    std::atomic<std::uint64_t> transport_queue_overflows_{};
    std::atomic<std::uint64_t> gpu_slot_retries_{}, present_retries_{}, presentation_wait_timeouts_{};
    std::atomic<std::uint64_t> receive_timing_samples_{}, assembly_microseconds_{}, queue_microseconds_{}, max_queue_microseconds_{};
    std::atomic<std::uint64_t> host_latency_samples_{}, host_latency_tenths_ms_{}, zero_host_latency_frames_{};
    std::atomic<std::uint16_t> max_host_latency_tenths_ms_{};
    mutable std::mutex state_mutex_;
    std::condition_variable changed_;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> requested_size_;
    stream_profile negotiated_;
    std::string status_;
    monotonic_time started_at_{};
    bool owns_core_{}; // Only accessed by serialized begin/stop owner.
};
}
