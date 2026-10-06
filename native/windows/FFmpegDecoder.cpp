#include "FFmpegDecoder.h"
#include "D3D11Surface.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/pixdesc.h>
}

namespace fuser::windows {
namespace {
struct codec_deleter { void operator()(AVCodecContext* value) const noexcept { avcodec_free_context(&value); } };
struct packet_deleter { void operator()(AVPacket* value) const noexcept { av_packet_free(&value); } };
struct frame_deleter { void operator()(AVFrame* value) const noexcept { av_frame_free(&value); } };
struct buffer_deleter { void operator()(AVBufferRef* value) const noexcept { av_buffer_unref(&value); } };
using codec_owner = std::unique_ptr<AVCodecContext, codec_deleter>;
using packet_owner = std::unique_ptr<AVPacket, packet_deleter>;
using frame_owner = std::unique_ptr<AVFrame, frame_deleter>;
using buffer_owner = std::unique_ptr<AVBufferRef, buffer_deleter>;

operation_result failure(int error, const char* stage) {
    char detail[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(error, detail, sizeof(detail));
    return {operation_code::decoder_error, std::string{stage} + ": " + detail};
}

struct frame_metadata {
    std::uint64_t sequence{};
    monotonic_time received_at{};
};
static_assert(std::is_trivially_copyable_v<frame_metadata>);

class ffmpeg_frame_lease final : public decoder_frame_lease {
public:
    explicit ffmpeg_frame_lease(frame_owner frame) : frame_{std::move(frame)} {}
private:
    frame_owner frame_;
};

struct hardware_lock_state { std::shared_ptr<std::recursive_mutex> lock; };
void lock_context(void* state) { static_cast<hardware_lock_state*>(state)->lock->lock(); }
void unlock_context(void* state) { static_cast<hardware_lock_state*>(state)->lock->unlock(); }
void free_hardware_state(AVHWDeviceContext* context) {
    delete static_cast<hardware_lock_state*>(context->user_opaque);
}

AVPixelFormat hardware_format(AVCodecContext* codec, const AVPixelFormat* formats) {
    try {
        for (const auto* format = formats; *format != AV_PIX_FMT_NONE; ++format) {
            if (*format != AV_PIX_FMT_D3D11) { continue; }
            AVBufferRef* raw_frames{};
            if (avcodec_get_hw_frames_parameters(codec, codec->hw_device_ctx, *format, &raw_frames) < 0) {
                return AV_PIX_FMT_NONE;
            }
            buffer_owner frames{raw_frames};
            auto* frame_context = reinterpret_cast<AVHWFramesContext*>(frames->data);
            if (frame_context->sw_format != AV_PIX_FMT_NV12) { return AV_PIX_FMT_NONE; }
            // Four display leases can exist outside the decoder: mailbox + three
            // GPU readers. Increase the fixed array pool accordingly.
            frame_context->initial_pool_size += 4;
            auto* hardware = static_cast<AVD3D11VAFramesContext*>(frame_context->hwctx);
            hardware->BindFlags |= D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
            if (av_hwframe_ctx_init(frames.get()) < 0) { return AV_PIX_FMT_NONE; }
            av_buffer_unref(&codec->hw_frames_ctx);
            codec->hw_frames_ctx = frames.release();
            return *format;
        }
    } catch (...) {
        // C callbacks cannot propagate C++ exceptions through FFmpeg.
    }
    return AV_PIX_FMT_NONE;
}
}

struct ffmpeg_decoder::implementation {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::shared_ptr<std::recursive_mutex> context_lock;
    std::mutex submissions;
    codec_owner codec;
    video_codec selected_codec{};
    std::function<void(decoded_frame)> on_frame;
    bool flushed{};

    operation_result drain() {
        frame_owner frame{av_frame_alloc()};
        if (!frame) { return failure(AVERROR(ENOMEM), "Allocate decoded frame"); }
        for (;;) {
            const auto result = avcodec_receive_frame(codec.get(), frame.get());
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) { return {}; }
            if (result < 0) { return failure(result, "Receive hardware frame"); }
            if (frame->format != AV_PIX_FMT_D3D11 || !frame->hw_frames_ctx || !frame->data[0]) {
                return {operation_code::unsupported_format, "Decoder did not return a D3D11 hardware surface."};
            }
            const auto* frames = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
            if (frames->sw_format != AV_PIX_FMT_NV12 || frame->width <= 0 || frame->height <= 0) {
                return {operation_code::unsupported_format, "Only 8-bit SDR NV12 decode output is supported."};
            }
            const auto width = static_cast<std::uint32_t>(frame->width);
            const auto height = static_cast<std::uint32_t>(frame->height);
            if (frame->crop_left >= width || frame->crop_right >= width - frame->crop_left ||
                frame->crop_top >= height || frame->crop_bottom >= height - frame->crop_top) {
                return {operation_code::unsupported_format, "Decoded visible rectangle is invalid."};
            }
            const auto slice = reinterpret_cast<std::uintptr_t>(frame->data[1]);
            if (slice > std::numeric_limits<std::uint32_t>::max()) {
                return {operation_code::unsupported_format, "Decoded texture slice is invalid."};
            }
            frame_owner retained{av_frame_clone(frame.get())};
            if (!retained) { return failure(AVERROR(ENOMEM), "Retain decoder frame"); }
            const auto lease = std::make_shared<ffmpeg_frame_lease>(std::move(retained));
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{reinterpret_cast<ID3D11Texture2D*>(frame->data[0])};
            decoded_frame output;
            output.surface = std::make_shared<d3d11_surface>(texture, static_cast<std::uint32_t>(slice), lease);
            output.width = width - static_cast<std::uint32_t>(frame->crop_left + frame->crop_right);
            output.height = height - static_cast<std::uint32_t>(frame->crop_top + frame->crop_bottom);
            output.source_x = static_cast<std::uint32_t>(frame->crop_left);
            output.source_y = static_cast<std::uint32_t>(frame->crop_top);
            if (frame->opaque_ref && frame->opaque_ref->size == sizeof(frame_metadata)) {
                frame_metadata metadata;
                std::memcpy(&metadata, frame->opaque_ref->data, sizeof(metadata));
                output.sequence = metadata.sequence;
                output.received_at = metadata.received_at;
            }
            output.decoded_at = std::chrono::steady_clock::now();
            switch (frame->colorspace) {
            case AVCOL_SPC_BT470BG: case AVCOL_SPC_SMPTE170M:
                output.matrix = color_matrix::bt601; break;
            case AVCOL_SPC_BT2020_NCL: case AVCOL_SPC_BT2020_CL:
                return {operation_code::unsupported_format, "BT.2020 input is not supported by the SDR renderer."};
            case AVCOL_SPC_BT709: case AVCOL_SPC_UNSPECIFIED:
                // The client requests BT.709; use that only when the bitstream
                // omits metadata, otherwise honor the actual frame metadata.
                output.matrix = color_matrix::bt709; break;
            default:
                return {operation_code::unsupported_format, "Decoded color matrix is unsupported."};
            }
            output.range = frame->color_range == AVCOL_RANGE_JPEG ? color_range::full : color_range::limited;
            on_frame(std::move(output));
            av_frame_unref(frame.get());
        }
    }
};

ffmpeg_decoder::ffmpeg_decoder(Microsoft::WRL::ComPtr<ID3D11Device> device,
                             Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                             std::shared_ptr<std::recursive_mutex> context_lock)
    : state_{std::make_unique<implementation>()} {
    if (!device || !context || !context_lock) { throw std::invalid_argument{"Decoder needs a shared D3D11 device, context and recursive lock."}; }
    state_->device = std::move(device);
    state_->context = std::move(context);
    state_->context_lock = std::move(context_lock);
}

ffmpeg_decoder::~ffmpeg_decoder() { stop(); }

operation_result ffmpeg_decoder::initialize(const stream_profile& profile,
                                           std::function<void(decoded_frame)> on_frame) {
    const std::lock_guard guard{state_->submissions};
    if (state_->codec) { return {operation_code::unavailable, "Stop the decoder before initializing another session."}; }
    if (!on_frame || profile.width == 0 || profile.height == 0 || profile.width > 8192 || profile.height > 8192) {
        return {operation_code::invalid_configuration, "Decoder needs a frame callback and valid dimensions."};
    }
    AVCodecID id{};
    switch (profile.codec) {
    case video_codec::h264: id = AV_CODEC_ID_H264; break;
    case video_codec::hevc: id = AV_CODEC_ID_HEVC; break;
    case video_codec::av1: id = AV_CODEC_ID_AV1; break;
    default: return {operation_code::unsupported_format, "Unsupported codec."};
    }
    const auto* decoder = avcodec_find_decoder(id);
    if (!decoder) { return {operation_code::unavailable, "Requested codec is absent from FFmpeg."}; }
    codec_owner codec{avcodec_alloc_context3(decoder)};
    buffer_owner hardware{av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA)};
    if (!codec || !hardware) { return failure(AVERROR(ENOMEM), "Allocate hardware decoder"); }
    auto* hardware_context = reinterpret_cast<AVHWDeviceContext*>(hardware->data);
    auto lock_state = std::make_unique<hardware_lock_state>(hardware_lock_state{state_->context_lock});
    hardware_context->user_opaque = lock_state.get();
    hardware_context->free = free_hardware_state;
    auto* d3d = static_cast<AVD3D11VADeviceContext*>(hardware_context->hwctx);
    d3d->device = state_->device.Get();
    d3d->device->AddRef(); // FFmpeg unconditionally releases user-supplied COM interfaces.
    d3d->device_context = state_->context.Get();
    d3d->device_context->AddRef();
    d3d->lock = lock_context;
    d3d->unlock = unlock_context;
    d3d->lock_ctx = lock_state.release(); // Owned by the hardware buffer, including retained frames.
    const auto initialized = av_hwdevice_ctx_init(hardware.get());
    if (initialized < 0) { return failure(initialized, "Initialize D3D11VA device"); }
    codec->hw_device_ctx = hardware.release();
    codec->get_format = hardware_format;
    codec->pix_fmt = AV_PIX_FMT_D3D11;
    codec->sw_pix_fmt = AV_PIX_FMT_NV12;
    codec->width = static_cast<int>(profile.width);
    codec->height = static_cast<int>(profile.height);
    codec->thread_count = 1;
    codec->flags |= AV_CODEC_FLAG_LOW_DELAY | AV_CODEC_FLAG_COPY_OPAQUE;
    codec->apply_cropping = 0; // Preserve GPU crop metadata instead of adjusting opaque plane pointers.
    const auto opened = avcodec_open2(codec.get(), decoder, nullptr);
    if (opened < 0) { return failure(opened, "Open hardware decoder"); }
    state_->selected_codec = profile.codec;
    state_->on_frame = std::move(on_frame);
    state_->flushed = false;
    state_->codec = std::move(codec);
    return {};
}

operation_result ffmpeg_decoder::submit(encoded_frame frame) {
    const std::lock_guard guard{state_->submissions};
    if (!state_->codec || state_->flushed) { return {operation_code::unavailable, "Decoder is stopped or flushed."}; }
    if (frame.codec != state_->selected_codec || frame.bytes.empty() || frame.bytes.size() > 32 * 1024 * 1024) {
        return {operation_code::invalid_configuration, "Compressed frame codec or size is invalid."};
    }
    try {
        packet_owner packet{av_packet_alloc()};
        if (!packet) { return failure(AVERROR(ENOMEM), "Allocate input packet"); }
        const auto allocated = av_new_packet(packet.get(), static_cast<int>(frame.bytes.size()));
        if (allocated < 0) { return failure(allocated, "Allocate packet data"); }
        std::memcpy(packet->data, frame.bytes.data(), frame.bytes.size());
        packet->opaque_ref = av_buffer_alloc(sizeof(frame_metadata));
        if (!packet->opaque_ref) { return failure(AVERROR(ENOMEM), "Allocate frame timing"); }
        const frame_metadata metadata{frame.sequence, frame.received_at};
        std::memcpy(packet->opaque_ref->data, &metadata, sizeof(metadata));
        auto sent = avcodec_send_packet(state_->codec.get(), packet.get());
        if (sent == AVERROR(EAGAIN)) {
            const auto drained = state_->drain();
            if (!drained.succeeded()) { return drained; }
            sent = avcodec_send_packet(state_->codec.get(), packet.get());
        }
        if (sent < 0) { return failure(sent, "Submit compressed frame; request a new keyframe"); }
        return state_->drain();
    } catch (const std::exception& error) {
        return {operation_code::decoder_error, error.what()};
    } catch (...) {
        return {operation_code::decoder_error, "Decoded-frame callback failed."};
    }
}

operation_result ffmpeg_decoder::flush() {
    const std::lock_guard guard{state_->submissions};
    if (!state_->codec) { return {operation_code::unavailable, "Decoder is stopped."}; }
    try {
        const auto sent = avcodec_send_packet(state_->codec.get(), nullptr);
        if (sent < 0 && sent != AVERROR_EOF) { return failure(sent, "Flush decoder"); }
        state_->flushed = true;
        return state_->drain();
    } catch (...) {
        return {operation_code::decoder_error, "Decoded-frame callback failed during flush."};
    }
}

void ffmpeg_decoder::stop() noexcept {
    const std::lock_guard guard{state_->submissions};
    state_->on_frame = {};
    state_->codec.reset();
    state_->flushed = false;
}
}
