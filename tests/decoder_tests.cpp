// Decode owned, generated fixtures with the production FFmpeg/D3D11VA adapter.
#include "FFmpegDecoder.h"
#include "D3D11Renderer.h"
#include <winrt/base.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <thread>
#include <fuser/latest_frame_mailbox.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {
using Microsoft::WRL::ComPtr;
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error{message}; }
}
void require(const fuser::operation_result& result) {
    if (!result.succeeded()) { throw std::runtime_error{result.detail}; }
}

std::array<std::uint8_t, 4> pixel(fuser::windows::d3d11_renderer& renderer, UINT x, UINT y) {
    const std::lock_guard guard{*renderer.context_lock()};
    ComPtr<ID3D11Texture2D> back_buffer;
    winrt::check_hresult(renderer.swap_chain()->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf())));
    D3D11_TEXTURE2D_DESC description{};
    back_buffer->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    winrt::check_hresult(renderer.device()->CreateTexture2D(&description, nullptr, staging.GetAddressOf()));
    renderer.context()->CopyResource(staging.Get(), back_buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(renderer.context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    const auto* location = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
    const std::array<std::uint8_t, 4> result{location[0], location[1], location[2], location[3]};
    renderer.context()->Unmap(staging.Get(), 0);
    return result;
}

struct parser_deleter { void operator()(AVCodecParserContext* value) const noexcept { av_parser_close(value); } };
struct codec_deleter { void operator()(AVCodecContext* value) const noexcept { avcodec_free_context(&value); } };
}

int main(int argc, char** argv) {
    try {
        require(argc == 3 || argc == 4, "Pass the fixture path, h264/hevc codec, and optional concurrent mode.");
        const bool concurrent = argc == 4 && std::string{argv[3]} == "concurrent";
        const auto selected = std::string{argv[2]} == "h264" ? fuser::video_codec::h264 : fuser::video_codec::hevc;
        const auto codec_id = selected == fuser::video_codec::h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        fuser::windows::d3d11_renderer renderer;
        renderer.initialize(2560, 1440);
        ComPtr<ID3D11Device> device{renderer.device()};
        ComPtr<ID3D11DeviceContext> context{renderer.context()};
        fuser::windows::ffmpeg_decoder decoder{device, context, renderer.context_lock()};
        fuser::stream_profile profile;
        profile.codec = selected;
        std::uint64_t decoded{};
        fuser::decoded_frame retained;
        fuser::latest_frame_mailbox mailbox;
        std::atomic<std::uint64_t> presented{};
        std::exception_ptr render_error;
        std::jthread render_worker;
        if (concurrent) {
            render_worker = std::jthread{[&](std::stop_token stop) {
                try {
                    for (;;) {
                        if (!renderer.wait_to_present(8)) {
                            if (stop.stop_requested()) { break; }
                            continue;
                        }
                        if (auto frame = mailbox.take_latest()) {
                            const auto drawn = renderer.draw_frame(*frame, {});
                            if (drawn.code == fuser::operation_code::unavailable) { continue; }
                            require(drawn);
                            if (renderer.try_present()) { ++presented; }
                        } else if (stop.stop_requested()) {
                            break;
                        } else {
                            std::this_thread::yield();
                        }
                    }
                } catch (...) { render_error = std::current_exception(); }
            }};
        }
        require(decoder.initialize(profile, [&](fuser::decoded_frame frame) {
            require(frame.width == 2560 && frame.height == 1440, "Decoded visible dimensions must match the fixture.");
            require(frame.sequence > 0 && frame.received_at != fuser::monotonic_time{}
                    && frame.decoded_at >= frame.received_at, "Frame metadata must survive hardware decoding.");
            require(frame.matrix == fuser::color_matrix::bt709 && frame.range == fuser::color_range::limited,
                    "Bitstream color metadata must reach the renderer.");
            const auto surface = std::dynamic_pointer_cast<const fuser::windows::d3d11_surface>(frame.surface);
            require(surface && surface->texture(), "Hardware decoding must return a D3D11 texture.");
            D3D11_TEXTURE2D_DESC description{};
            surface->texture()->GetDesc(&description);
            require((description.BindFlags & (D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE)) ==
                    (D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE), "Hardware frames must be directly shader-readable.");
            if (concurrent) {
                retained = frame;
                require(mailbox.publish(std::move(frame)), "Concurrent mailbox must accept decoded output.");
                ++decoded;
                return;
            }
            require(renderer.draw_frame(frame, {}));
            require(pixel(renderer, 10, 10) == std::array<std::uint8_t, 4>{0, 0, 0, 0},
                    "Compressed green must disappear after hardware decode and keying.");
            const auto white = pixel(renderer, 100, 100);
            require(white[0] >= 250 && white[1] >= 250 && white[2] >= 250 && white[3] == 255,
                    "Opaque white HUD content must survive decode and keying.");
            retained = std::move(frame);
            ++decoded;
        }));
        std::ifstream input{argv[1], std::ios::binary};
        require(input.good(), "Cannot read fixture.");
        const std::vector<char> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        require(!bytes.empty() && bytes.size() < 32 * 1024 * 1024, "Fixture size is invalid.");
        std::vector<std::uint8_t> padded(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE);
        std::memcpy(padded.data(), bytes.data(), bytes.size());
        std::unique_ptr<AVCodecParserContext, parser_deleter> parser{av_parser_init(codec_id)};
        std::unique_ptr<AVCodecContext, codec_deleter> parser_context{avcodec_alloc_context3(avcodec_find_decoder(codec_id))};
        require(parser && parser_context, "Cannot create fixture parser.");
        std::size_t offset{};
        std::uint64_t sequence{};
        fuser::encoded_frame first_unit;
        const auto submit = [&](const std::uint8_t* data, int size) {
            fuser::encoded_frame frame;
            frame.codec = selected;
            frame.sequence = ++sequence;
            frame.received_at = std::chrono::steady_clock::now();
            frame.bytes.resize(static_cast<std::size_t>(size));
            std::memcpy(frame.bytes.data(), data, static_cast<std::size_t>(size));
            if (sequence == 1) { first_unit = frame; }
            require(decoder.submit(std::move(frame)));
        };
        while (offset < bytes.size()) {
            std::uint8_t* output{};
            int output_size{};
            const auto consumed = av_parser_parse2(parser.get(), parser_context.get(), &output, &output_size,
                padded.data() + offset, static_cast<int>(bytes.size() - offset), AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1);
            require(consumed >= 0 && (consumed > 0 || output_size > 0), "Fixture parser made no progress.");
            if (output_size > 0) { submit(output, output_size); }
            offset += static_cast<std::size_t>(consumed);
        }
        std::uint8_t* output{};
        int output_size{};
        require(av_parser_parse2(parser.get(), parser_context.get(), &output, &output_size,
                nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1) >= 0, "Cannot flush fixture parser.");
        if (output_size > 0) { submit(output, output_size); }
        require(decoder.flush());
        if (concurrent) {
            render_worker.request_stop();
            render_worker.join();
            if (render_error) { std::rethrow_exception(render_error); }
            require(presented > 0, "Concurrent rendering must present decoded frames.");
        }
        const auto expected = concurrent ? 120U : 12U;
        require(decoded == expected && sequence == expected, "All fixture access units must decode.");
        const auto allocations = decoder.resources_created();
        require(allocations.packet_wrappers == 1 && allocations.receive_frame_wrappers == 1,
                "Packet and receive wrappers must be allocated once per decoder session.");
        require(allocations.retained_frame_wrappers == decoded,
                "Each output must retain its own decoder frame lease.");
        decoder.stop();
        decoder.stop();
        auto retained_draw = renderer.draw_frame(retained, {});
        // Nonblocking presentation does not imply GPU read fences are already
        // complete. Await slot retirement, then verify the retained pool lease.
        for (int retry = 0; retained_draw.code == fuser::operation_code::unavailable && retry < 100; ++retry) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            retained_draw = renderer.draw_frame(retained, {});
        }
        require(retained_draw);
        require(pixel(renderer, 10, 10) == std::array<std::uint8_t, 4>{0, 0, 0, 0},
                "Retained compressed green must remain transparent.");
        require(pixel(renderer, 100, 100)[3] == 255, "A retained frame lease must survive decoder shutdown.");
        require(decoder.submit({}).code == fuser::operation_code::unavailable,
                "Stopped decoder must reject new input.");
        // A callback failure must release transferred leases and packet data;
        // stop/reinitialize must recreate wrappers without invalidating old output.
        require(decoder.initialize(profile, [](fuser::decoded_frame) {
            throw std::runtime_error{"Owned callback failure"};
        }));
        require(decoder.submit(first_unit).code == fuser::operation_code::decoder_error,
                "Callback failure must return a decoder error without escaping.");
        decoder.stop();
        std::uint64_t restarted{};
        require(decoder.initialize(profile, [&](fuser::decoded_frame frame) {
            require(frame.sequence == first_unit.sequence && frame.received_at == first_unit.received_at,
                    "Restart must preserve this packet's own timing metadata.");
            ++restarted;
        }));
        require(decoder.submit(first_unit));
        require(decoder.flush());
        require(restarted == 1, "The decoder must restart after callback failure.");
        require(decoder.resources_created().packet_wrappers == 1 &&
                decoder.resources_created().receive_frame_wrappers == 1,
                "A new session must reset wrapper counts.");
        decoder.stop();
        std::cout << argv[2] << ": " << decoded << " hardware-decoded 1440p frames; alpha, metadata and shutdown leases passed.\n"
                  << "Concurrent present calls: " << presented << ".\n"
                  << "This is a fixture test, not a streaming or displayed-FPS measurement.\n";
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << winrt::to_string(error.message()) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
