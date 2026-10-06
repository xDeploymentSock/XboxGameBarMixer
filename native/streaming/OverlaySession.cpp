#include "OverlaySession.h"
#include <Limelight.h>
#include <openssl/rand.h>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <cstdarg>
#include <cstdio>
#include <string_view>

namespace fuser::streaming {
namespace {
std::mutex core_gate;
std::atomic<overlay_session*> core_owner{};
int mute_init(int, const POPUS_MULTISTREAM_CONFIGURATION, void*, int) { return 0; }
void mute_sample(char*, int) {}
}
overlay_session::overlay_session(std::shared_ptr<sunshine_control> control,
    std::function<void(const std::string&)> logger) : control_{std::move(control)}, logger_{std::move(logger)} {}
overlay_session::~overlay_session() { stop(); }
void overlay_session::prepare_action() noexcept { cancelled_ = false; control_->prepare_action(); }
void overlay_session::status(std::string text, bool terminal) noexcept {
    try { const std::lock_guard guard{state_mutex_}; status_ = std::move(text); }
    catch (...) {}
    if (terminal) { finished_ = true; }
}
void overlay_session::trace(const char* phase, std::uint64_t number) noexcept {
    if (!logger_ || number > 3) { return; }
    try { logger_(std::string{phase} + " " + std::to_string(number) +
        " thread " + std::to_string(GetCurrentThreadId()) + "\n"); } catch (...) {}
}
operation_result overlay_session::begin(const overlay_configuration& config, const host_application& application,
    std::shared_ptr<windows::d3d11_renderer> renderer, int display_refresh_x100) {
    if (cancelled_) { return {operation_code::cancelled, "Connection cancelled."}; }
    if (owns_core_ || render_thread_.joinable()) { return {operation_code::unavailable, "Disconnect the previous stream first."}; }
    const auto issues = validate(config);
    if (!issues.empty()) { return {operation_code::invalid_configuration, issues.front().message}; }
    if (!renderer) { return {operation_code::decoder_error, "A composition renderer is required."}; }
    // Never launch a source application while another widget session owns the core.
    {
        const std::lock_guard guard{core_gate};
        if (core_owner.load()) { return {operation_code::unavailable, "Another streaming session is active."}; }
        core_owner = this;
        owns_core_ = true;
    }
    configuration_ = config;
    renderer_ = std::move(renderer);
    received_ = decoded_ = presented_ = decode_errors_ = 0;
    last_frame_number_ = peak_decode_queue_ = 0;
    missing_frame_numbers_ = decode_microseconds_ = max_decode_microseconds_ = 0;
    receive_timing_samples_ = assembly_microseconds_ = queue_microseconds_ = max_queue_microseconds_ = 0;
    host_latency_samples_ = host_latency_tenths_ms_ = zero_host_latency_frames_ = max_host_latency_tenths_ms_ = 0;
    finished_ = render_stop_ = false;
    mailbox_.reset();
    {
        const std::lock_guard guard{state_mutex_};
        started_at_ = std::chrono::steady_clock::now();
        negotiated_ = {};
        negotiated_.width = negotiated_.height = negotiated_.frames_per_second = 0;
        requested_size_.reset();
    }
    std::array<unsigned char, 16> input_key{}, input_iv{};
    if (RAND_bytes(input_key.data(), 16) != 1 || RAND_bytes(input_iv.data(), 16) != 1) {
        stop(); return {operation_code::transport_error, "Cannot generate stream encryption keys."};
    }
    // GameStream uses the first four IV bytes as its key ID; the remaining bytes are zero.
    std::fill(input_iv.begin() + 4, input_iv.end(), 0);
    launch_information launch;
    auto result = control_->start_stream(config, application, input_key, input_iv, launch);
    if (!result.succeeded()) { stop(); return result; }
    if (cancelled_) { stop(); return {operation_code::cancelled, "Connection cancelled."}; }
    STREAM_CONFIGURATION stream{};
    LiInitializeStreamConfiguration(&stream);
    stream.width = static_cast<int>(config.stream.width);
    stream.height = static_cast<int>(config.stream.height);
    stream.fps = static_cast<int>(config.stream.frames_per_second);
    stream.bitrate = static_cast<int>(config.stream.bitrate_kbps);
    stream.packetSize = 1392;
    stream.streamingRemotely = STREAM_CFG_AUTO;
    stream.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    stream.colorSpace = COLORSPACE_REC_709;
    stream.colorRange = COLOR_RANGE_LIMITED;
    stream.encryptionFlags = ENCFLG_ALL;
    stream.clientRefreshRateX100 = display_refresh_x100;
    stream.supportedVideoFormats = config.stream.codec == video_codec::hevc ? VIDEO_FORMAT_H265 :
        config.stream.codec == video_codec::av1 ? VIDEO_FORMAT_AV1_MAIN8 : VIDEO_FORMAT_H264;
    std::memcpy(stream.remoteInputAesKey, input_key.data(), 16);
    std::memcpy(stream.remoteInputAesIv, input_iv.data(), 16);
    SERVER_INFORMATION server{};
    LiInitializeServerInformation(&server);
    server.address = config.host.address.c_str();
    server.serverInfoAppVersion = launch.host.app_version.c_str();
    server.serverInfoGfeVersion = launch.host.gfe_version.c_str();
    server.rtspSessionUrl = launch.rtsp_url.c_str();
    server.serverCodecModeSupport = static_cast<int>(launch.host.codec_support);
    DECODER_RENDERER_CALLBACKS video{};
    LiInitializeVideoCallbacks(&video);
    video.setup = setup;
    video.submitDecodeUnit = [](PDECODE_UNIT unit) { return submit(unit); };
    // FFmpeg decode may block: use Moonlight's ordered decoder thread, not direct submit.
    video.capabilities = CAPABILITY_SLICES_PER_FRAME(1);
    AUDIO_RENDERER_CALLBACKS audio{};
    LiInitializeAudioCallbacks(&audio);
    audio.init = mute_init;
    audio.decodeAndPlaySample = mute_sample;
    CONNECTION_LISTENER_CALLBACKS listener{};
    LiInitializeConnectionCallbacks(&listener);
    listener.stageStarting = stage_starting;
    listener.stageFailed = stage_failed;
    listener.connectionTerminated = terminated;
    listener.logMessage = log_message;
    try {
        decoder_ = std::make_unique<windows::ffmpeg_decoder>(renderer_->device(), renderer_->context(), renderer_->context_lock());
        render_thread_ = std::thread{[this] { render(); }};
        const auto error = LiStartConnection(&server, &stream, &listener, &video, &audio, this, 0, nullptr, 0);
        connected_ = error == 0;
        if (error || cancelled_ || finished_) {
            const auto reason = snapshot().status;
            const bool was_cancelled = cancelled_;
            stop();
            return {was_cancelled ? operation_code::cancelled : operation_code::transport_error,
                was_cancelled ? "Connection cancelled." : "Stream startup failed: " + reason + " (" + std::to_string(error) + ")."};
        }
        status("Streaming. Audio is muted; input remains local.");
        return {};
    } catch (const std::exception& error) {
        stop(); return {operation_code::transport_error, error.what()};
    } catch (...) {
        stop(); return {operation_code::transport_error, "Stream startup encountered a Windows error."};
    }
}
void overlay_session::cancel() noexcept {
    cancelled_ = true;
    control_->cancel();
    const std::lock_guard guard{core_gate};
    if (core_owner == this) { LiInterruptConnection(); }
}
void overlay_session::stop() noexcept {
    cancel();
    // LiStartConnection's error path already joins its workers. A successful
    // connection must be stopped before releasing decoder callbacks or surfaces.
    if (connected_.exchange(false)) { LiStopConnection(); }
    render_stop_ = true;
    changed_.notify_all();
    if (render_thread_.joinable()) { render_thread_.join(); }
    if (decoder_) { decoder_->stop(); }
    mailbox_.close();
    decoder_.reset();
    renderer_.reset();
    if (owns_core_) {
        const std::lock_guard guard{core_gate};
        core_owner = nullptr;
        owns_core_ = false;
    }
}
int overlay_session::setup(int format, int width, int height, int fps, void* context, int) noexcept {
    auto& self = *static_cast<overlay_session*>(context);
    try {
        if (self.cancelled_ || width <= 0 || height <= 0 || fps <= 0) { return -1; }
        auto actual = self.configuration_.stream;
        actual.width = static_cast<std::uint32_t>(width);
        actual.height = static_cast<std::uint32_t>(height);
        actual.frames_per_second = static_cast<std::uint32_t>(fps);
        if (format == VIDEO_FORMAT_H264) { actual.codec = video_codec::h264; }
        else if (format == VIDEO_FORMAT_H265) { actual.codec = video_codec::hevc; }
        else if (format == VIDEO_FORMAT_AV1_MAIN8) { actual.codec = video_codec::av1; }
        else { self.status("Host negotiated an unsupported pixel format.", true); return -1; }
        {
            const std::lock_guard guard{self.state_mutex_}; self.negotiated_ = actual;
        }
        const auto result = self.decoder_->initialize(actual, [&self](decoded_frame frame) {
            const auto number = ++self.decoded_;
            self.trace("Decoded; publishing", number);
            (void)self.mailbox_.publish(std::move(frame));
            self.changed_.notify_one();
            self.trace("Published", number);
        });
        if (!result.succeeded()) { self.status(result.detail, true); return -1; }
        return 0;
    } catch (...) { self.status("Decoder setup failed.", true); return -1; }
}
int overlay_session::submit(void* value) noexcept {
    auto* self = core_owner.load();
    const auto* unit = static_cast<PDECODE_UNIT>(value);
    if (!self || self->cancelled_ || self->finished_) { return DR_OK; }
    try {
        if (!unit || unit->fullLength <= 0 || unit->fullLength > 32 * 1024 * 1024) { return DR_NEED_IDR; }
        encoded_frame frame;
        frame.bytes.resize(static_cast<std::size_t>(unit->fullLength));
        frame.codec = self->negotiated_.codec; // Written before Moonlight starts its decoder thread.
        frame.sequence = static_cast<std::uint32_t>(unit->frameNumber);
        frame.received_at = std::chrono::steady_clock::now(); // Callback arrival, not first network packet.
        frame.host_presentation_time = std::chrono::microseconds{unit->presentationTimeUs};
        std::size_t offset{};
        for (auto* entry = unit->bufferList; entry; entry = entry->next) {
            if (!entry->data || entry->length <= 0 || static_cast<std::size_t>(entry->length) > frame.bytes.size() - offset) {
                return DR_NEED_IDR;
            }
            std::memcpy(frame.bytes.data() + offset, entry->data, static_cast<std::size_t>(entry->length));
            offset += static_cast<std::size_t>(entry->length);
        }
        if (offset != frame.bytes.size()) { return DR_NEED_IDR; }
        const auto previous = self->last_frame_number_.exchange(static_cast<std::uint32_t>(frame.sequence));
        const auto difference = static_cast<std::uint32_t>(frame.sequence) - previous;
        if (previous && difference > 1 && difference < 0x80000000U) {
            self->missing_frame_numbers_ += difference - 1;
        }
        // Read on the decoder callback while the core's video queue is alive;
        // snapshot() uses atomics and never queries a queue during teardown.
        const auto queued = static_cast<std::uint32_t>(std::max(0, LiGetPendingVideoFrames()));
        auto peak = self->peak_decode_queue_.load();
        while (peak < queued && !self->peak_decode_queue_.compare_exchange_weak(peak, queued)) {}
        // Moonlight's receive/enqueue times share LiGetMicroseconds()'s epoch.
        // This measures assembly and client queue delay, not one-way network latency.
        const auto callback_time = LiGetMicroseconds();
        if (unit->receiveTimeUs && unit->enqueueTimeUs >= unit->receiveTimeUs && callback_time >= unit->enqueueTimeUs) {
            ++self->receive_timing_samples_;
            self->assembly_microseconds_ += unit->enqueueTimeUs - unit->receiveTimeUs;
            const auto queue_time = callback_time - unit->enqueueTimeUs;
            self->queue_microseconds_ += queue_time;
            auto maximum_queue = self->max_queue_microseconds_.load();
            while (maximum_queue < queue_time && !self->max_queue_microseconds_.compare_exchange_weak(maximum_queue, queue_time)) {}
        }
        if (unit->frameHostProcessingLatency) {
            ++self->host_latency_samples_;
            self->host_latency_tenths_ms_ += unit->frameHostProcessingLatency;
            auto maximum_host = self->max_host_latency_tenths_ms_.load();
            while (maximum_host < unit->frameHostProcessingLatency &&
                !self->max_host_latency_tenths_ms_.compare_exchange_weak(maximum_host, unit->frameHostProcessingLatency)) {}
        } else {
            // Per the pinned API, zero means repeated frame OR omitted latency.
            ++self->zero_host_latency_frames_;
        }
        const auto number = ++self->received_;
        self->trace("Decode enter", number);
        const auto decode_start = std::chrono::steady_clock::now();
        const auto result = self->decoder_->submit(std::move(frame));
        const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - decode_start).count());
        self->decode_microseconds_ += elapsed;
        auto maximum = self->max_decode_microseconds_.load();
        while (maximum < elapsed && !self->max_decode_microseconds_.compare_exchange_weak(maximum, elapsed)) {}
        self->trace("Decode returned", number);
        if (!result.succeeded()) {
            ++self->decode_errors_;
            if (result.code == operation_code::unsupported_format) { self->status(result.detail, true); }
            else if (self->decode_errors_ == 1) { self->status(result.detail + " Requesting a keyframe."); }
            return DR_NEED_IDR;
        }
        return DR_OK;
    } catch (...) { ++self->decode_errors_; return DR_NEED_IDR; }
}
void overlay_session::stage_starting(int stage) noexcept {
    if (auto* self = core_owner.load()) {
        if (self->cancelled_) { LiInterruptConnection(); }
        else { self->status(std::string{"Connecting: "} + LiGetStageName(stage)); }
    }
}
void overlay_session::stage_failed(int stage, int error) noexcept {
    if (auto* self = core_owner.load()) {
        try {
            const auto cause = self->finished_ ? self->snapshot().status : std::string{};
            self->status(std::string{LiGetStageName(stage)} + " failed (" + std::to_string(error) + "). " + cause, true);
        } catch (...) { self->finished_ = true; }
    }
}
void overlay_session::terminated(int error) noexcept {
    if (auto* self = core_owner.load()) { self->status("Host connection ended (" + std::to_string(error) + ").", true); }
}
void overlay_session::log_message(const char* format, ...) noexcept {
    auto* self = core_owner.load();
    if (!self || !self->logger_ || !format) { return; }
    // Exact pinned-core formats: only fixed text and numeric arguments. Never
    // log general protocol text, URLs, certificates, keys, or pairing payloads.
    constexpr std::string_view allowed[]{
        "Received first video packet after %d ms\n",
        "Terminating connection due to lack of video traffic\n",
        "Terminating connection due to lack of a successful video frame\n",
        "Video Receive: recvUdpSocket() failed: %d\n",
        "Control stream connection failed: %d\n",
        "Control stream received unexpected disconnect event\n",
        "Disconnect event timeout expired\n",
        "No video traffic was ever received from the host!\n",
        "Video decode unit queue overflow\n",
        "Stopping input stream...", "Stopping audio stream...", "Stopping video stream...",
        "Stopping control stream...", "Cleaning up input stream...", "Cleaning up video stream...",
        "Cleaning up control stream...", "Cleaning up audio stream...", "Cleaning up platform...", "done\n"};
    if (std::find(std::begin(allowed), std::end(allowed), format) == std::end(allowed)) { return; }
    char message[256]{};
    va_list arguments;
    va_start(arguments, format);
    const auto written = std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(message)) { return; }
    try { self->logger_(std::string{message, static_cast<std::size_t>(written)}); } catch (...) {}
}
void overlay_session::resize(std::uint32_t width, std::uint32_t height) {
    { const std::lock_guard guard{state_mutex_}; requested_size_ = {width, height}; }
    changed_.notify_one();
}
void overlay_session::set_key(const chroma_key_settings& key) {
    const std::lock_guard guard{state_mutex_}; configuration_.key = key;
}
session_snapshot overlay_session::snapshot() const {
    session_snapshot result;
    result.counters = {received_, decoded_, mailbox_.replaced_frames(), presented_};
    result.decode_errors = decode_errors_;
    result.last_frame_number = last_frame_number_;
    result.missing_frame_numbers = missing_frame_numbers_;
    result.peak_decode_queue = peak_decode_queue_;
    result.decode_microseconds = decode_microseconds_;
    result.max_decode_microseconds = max_decode_microseconds_;
    result.receive_timing_samples = receive_timing_samples_;
    result.assembly_microseconds = assembly_microseconds_;
    result.queue_microseconds = queue_microseconds_;
    result.max_queue_microseconds = max_queue_microseconds_;
    result.host_latency_samples = host_latency_samples_;
    result.host_latency_tenths_ms = host_latency_tenths_ms_;
    result.zero_host_latency_frames = zero_host_latency_frames_;
    result.max_host_latency_tenths_ms = max_host_latency_tenths_ms_;
    result.active = connected_;
    result.finished = finished_;
    const std::lock_guard guard{state_mutex_};
    result.negotiated = negotiated_;
    result.status = status_;
    result.seconds = started_at_ == monotonic_time{} ? 0.0 :
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at_).count();
    return result;
}
void overlay_session::render() noexcept {
    try {
        while (!render_stop_) {
            std::optional<std::pair<std::uint32_t, std::uint32_t>> size;
            chroma_key_settings key;
            {
                std::unique_lock lock{state_mutex_};
                changed_.wait_for(lock, std::chrono::milliseconds{4});
                size.swap(requested_size_);
                key = configuration_.key;
            }
            if (render_stop_) { break; }
            if (size) { renderer_->resize(size->first, size->second); }
            if (auto frame = mailbox_.take_latest()) {
                const auto number = presented_.load() + 1;
                trace("Draw enter", number);
                const auto result = renderer_->draw_frame(*frame, key);
                trace("Draw returned", number);
                if (result.succeeded()) {
                    trace("Present enter", number);
                    renderer_->present(); ++presented_;
                    trace("Present returned", number);
                }
                else if (result.code != operation_code::unavailable) { status(result.detail, true); break; }
            }
        }
    } catch (...) { status("GPU presentation failed. Disconnect and reconnect to recreate the device.", true); }
}
}
