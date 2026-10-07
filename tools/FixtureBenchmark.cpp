// Owned compressed fixtures through the production GPU decoder and chroma renderer.
// This offscreen benchmark excludes Sunshine, network transport and Game Bar.
#include "FFmpegDecoder.h"
#include "D3D11Renderer.h"
#include <fuser/latest_frame_mailbox.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
}

namespace {
using clock_type = std::chrono::steady_clock;
using packet_list = std::vector<std::vector<std::uint8_t>>;
void require(bool condition, const char* detail) {
    if (!condition) { throw std::runtime_error{detail}; }
}
void require(const fuser::operation_result& result) {
    if (!result.succeeded()) { throw std::runtime_error{result.detail}; }
}
int integer(std::string_view text, int minimum, int maximum) {
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc{} && result.ptr == text.data() + text.size() && value >= minimum && value <= maximum,
        "Invalid numeric argument.");
    return value;
}
struct parser_deleter { void operator()(AVCodecParserContext* value) const noexcept { av_parser_close(value); } };
struct codec_deleter { void operator()(AVCodecContext* value) const noexcept { avcodec_free_context(&value); } };
struct handle_deleter { void operator()(void* value) const noexcept { CloseHandle(value); } };

packet_list read_packets(const char* path, AVCodecID codec) {
    std::ifstream input{path, std::ios::binary};
    require(input.good(), "Cannot open owned fixture.");
    const std::vector<char> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    require(!bytes.empty() && bytes.size() < 32 * 1024 * 1024, "Fixture must be smaller than 32 MiB.");
    std::vector<std::uint8_t> padded(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    std::memcpy(padded.data(), bytes.data(), bytes.size());
    std::unique_ptr<AVCodecParserContext, parser_deleter> parser{av_parser_init(codec)};
    std::unique_ptr<AVCodecContext, codec_deleter> context{avcodec_alloc_context3(avcodec_find_decoder(codec))};
    require(parser && context, "Cannot initialize fixture parser.");
    packet_list packets;
    std::size_t offset{};
    while (offset < bytes.size()) {
        std::uint8_t* output{};
        int output_size{};
        const auto consumed = av_parser_parse2(parser.get(), context.get(), &output, &output_size,
            padded.data() + offset, static_cast<int>(bytes.size() - offset), AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1);
        require(consumed >= 0 && (consumed > 0 || output_size > 0), "Fixture parser made no progress.");
        if (output_size > 0) { packets.emplace_back(output, output + output_size); }
        offset += static_cast<std::size_t>(consumed);
    }
    std::uint8_t* output{};
    int output_size{};
    require(av_parser_parse2(parser.get(), context.get(), &output, &output_size,
        nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1) >= 0, "Cannot flush fixture parser.");
    if (output_size > 0) { packets.emplace_back(output, output + output_size); }
    require(!packets.empty(), "Fixture contains no access units.");
    return packets;
}

double percentile(std::vector<double> samples, double fraction) {
    if (samples.empty()) { return 0; }
    std::sort(samples.begin(), samples.end());
    return samples[static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1))];
}
double milliseconds(clock_type::duration duration) {
    return std::chrono::duration<double, std::milli>{duration}.count();
}

void wait_until(HANDLE timer, clock_type::time_point deadline) {
    const auto remaining = deadline - clock_type::now();
    if (remaining <= clock_type::duration::zero()) { return; }
    using timer_ticks = std::chrono::duration<long long, std::ratio<1, 10000000>>;
    LARGE_INTEGER due{};
    due.QuadPart = -std::max(1LL, std::chrono::duration_cast<timer_ticks>(remaining).count());
    winrt::check_bool(SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0));
    require(WaitForSingleObject(timer, 5000) == WAIT_OBJECT_0, "Fixture pacing timer failed.");
}

std::array<std::uint8_t, 4> pixel(fuser::windows::d3d11_renderer& renderer, UINT x, UINT y) {
    const std::lock_guard guard{*renderer.context_lock()};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer, staging;
    winrt::check_hresult(renderer.swap_chain()->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf())));
    D3D11_TEXTURE2D_DESC description{};
    buffer->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = description.MiscFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    winrt::check_hresult(renderer.device()->CreateTexture2D(&description, nullptr, staging.GetAddressOf()));
    renderer.context()->CopyResource(staging.Get(), buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(renderer.context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    const auto* location = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
    const std::array<std::uint8_t, 4> result{location[0], location[1], location[2], location[3]};
    renderer.context()->Unmap(staging.Get(), 0);
    return result;
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 5 || argc == 6, "Usage: fuser_fixture_benchmark <owned fixture> h264|hevc <paced fps; 0=unpaced> <frame count> [decode-only]");
        const bool decode_only = argc == 6;
        require(!decode_only || std::string_view{argv[5]} == "decode-only", "Unknown benchmark mode.");
        const std::string_view codec{argv[2]};
        require(codec == "h264" || codec == "hevc", "Select h264 or hevc.");
        const auto rate = integer(argv[3], 0, 500);
        const auto count = integer(argv[4], 1, 150000);
        const auto selected = codec == "h264" ? fuser::video_codec::h264 : fuser::video_codec::hevc;
        const auto packets = read_packets(argv[1], codec == "h264" ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        fuser::windows::d3d11_renderer renderer;
        renderer.initialize(2560, 1440);
        fuser::windows::ffmpeg_decoder decoder{renderer.device(), renderer.context(), renderer.context_lock()};
        fuser::latest_frame_mailbox mailbox;
        fuser::decoded_frame retained;
        std::uint64_t decoded{}, presented{}, unavailable{}, late_feeds{};
        std::vector<double> submit_times, draw_times, feed_to_present_times, present_intervals, input_preparation_times;
        submit_times.reserve(static_cast<std::size_t>(count));
        input_preparation_times.reserve(static_cast<std::size_t>(count));
        std::uint64_t input_bytes{};
        std::size_t maximum_input_bytes{};
        feed_to_present_times.reserve(static_cast<std::size_t>(count));
        draw_times.reserve(static_cast<std::size_t>(count));
        present_intervals.reserve(static_cast<std::size_t>(count));
        clock_type::time_point previous_present{};
        double steady_submit_total{};
        std::vector<double> steady_submit_times;
        steady_submit_times.reserve(static_cast<std::size_t>(count));
        std::mutex wake_mutex;
        std::condition_variable_any wake;
        std::atomic<bool> render_failed{};
        std::exception_ptr render_error;
        fuser::stream_profile profile;
        profile.codec = selected;
        require(decoder.initialize(profile, [&](fuser::decoded_frame frame) {
            require(frame.width == 2560 && frame.height == 1440 && frame.sequence > 0, "Unexpected fixture metadata.");
            retained = frame;
            if (!decode_only) { const std::lock_guard lock{wake_mutex};
              require(mailbox.publish(std::move(frame)), "Fixture mailbox is closed."); }
            ++decoded;
            wake.notify_one();
        }));
        std::unique_ptr<void, handle_deleter> timer{CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)};
        winrt::check_bool(static_cast<bool>(timer));
        std::jthread worker;
        if (!decode_only) { worker = std::jthread{[&](std::stop_token stop) {
            try {
                std::optional<fuser::decoded_frame> pending;
                for (;;) {
                    // Match production's predicate wake, wait-before-selection,
                    // latest-frame policy and nonblocking composition present.
                    // jthread destruction requests stop even on decoder errors.
                    // A stop-aware wait must wake without the normal-path notify.
                    { std::unique_lock lock{wake_mutex}; wake.wait(lock, stop, [&] {
                        return pending || mailbox.has_frame();
                    }); }
                    if (!pending && !mailbox.has_frame() && stop.stop_requested()) { break; }
                    if (!renderer.wait_to_present(8)) {
                        if (stop.stop_requested()) { break; }
                        continue;
                    }
                    (void)mailbox.take_latest_into(pending);
                    if (pending) {
                        const auto draw_start = clock_type::now();
                        const auto result = renderer.draw_frame(*pending, {});
                        draw_times.push_back(milliseconds(clock_type::now() - draw_start));
                        if (result.code == fuser::operation_code::unavailable) {
                            ++unavailable;
                            std::unique_lock lock{wake_mutex};
                            wake.wait_for(lock, stop, std::chrono::milliseconds{1}, [&] { return mailbox.has_frame(); });
                            continue;
                        }
                        require(result);
                        if (renderer.try_present()) {
                            const auto accepted = clock_type::now();
                            feed_to_present_times.push_back(milliseconds(accepted - pending->received_at));
                            if (previous_present != clock_type::time_point{}) {
                                present_intervals.push_back(milliseconds(accepted - previous_present));
                            }
                            previous_present = accepted;
                            ++presented;
                            pending.reset();
                        }
                    }
                }
            } catch (...) { render_error = std::current_exception(); render_failed = true; }
        }}; }
        const auto start = clock_type::now();
        for (int index = 0; index < count && !render_failed; ++index) {
            if (rate) {
                const auto due = start + std::chrono::nanoseconds{1000000000LL * index / rate};
                wait_until(timer.get(), due);
                if (clock_type::now() - due > std::chrono::nanoseconds{1000000000LL / rate}) { ++late_feeds; }
            }
            fuser::encoded_frame frame;
            frame.codec = selected;
            frame.sequence = static_cast<std::uint64_t>(index) + 1;
            // Local feed timestamp before compressed-byte copying and decoder submission.
            frame.received_at = clock_type::now();
            const auto& packet = packets[static_cast<std::size_t>(index) % packets.size()];
            frame.bytes.resize(packet.size());
            std::memcpy(frame.bytes.data(), packet.data(), packet.size());
            const auto preparation_ms = milliseconds(clock_type::now() - frame.received_at);
            const auto submitted = clock_type::now();
            require(decoder.submit(std::move(frame)));
            const auto submit_ms = milliseconds(clock_type::now() - submitted);
            input_bytes += packet.size();
            maximum_input_bytes = std::max(maximum_input_bytes, packet.size());
            if (index >= 20) { input_preparation_times.push_back(preparation_ms); }
            submit_times.push_back(submit_ms);
            // Exclude initial hardware-pool setup, but report full totals too.
            if (index >= 20) { steady_submit_times.push_back(submit_ms); steady_submit_total += submit_ms; }
        }
        require(decoder.flush());
        { const std::lock_guard lock{wake_mutex}; worker.request_stop(); }
        wake.notify_one();
        if (worker.joinable()) { worker.join(); }
        const auto seconds = std::chrono::duration<double>{clock_type::now() - start}.count();
        if (render_error) { std::rethrow_exception(render_error); }
        require(decoded == static_cast<std::uint64_t>(count) && (decode_only || presented > 0), "Fixture did not complete.");
        const auto decoder_resources = decoder.resources_created();
        decoder.stop();
        const auto resources = renderer.resources_created();
        auto final_draw = renderer.draw_frame(retained, {});
        for (int retry = 0; final_draw.code == fuser::operation_code::unavailable && retry < 100; ++retry) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            final_draw = renderer.draw_frame(retained, {});
        }
        require(final_draw);
        require(pixel(renderer, 10, 10) == std::array<std::uint8_t, 4>{0, 0, 0, 0}, "Green alpha check failed.");
        require(pixel(renderer, 100, 100)[3] == 255, "White HUD alpha check failed.");
        std::cout << codec << " 2560x1440 fixture | mode " << (decode_only ? "decode-only" : "concurrent-present") << " | requested pacing " << rate << " | seconds " << seconds
            << " | decoded " << decoded << " | present calls " << presented << '\n'
            << "Decoded/s " << static_cast<double>(decoded) / seconds << " | Present calls/s " << static_cast<double>(presented) / seconds
            << " | mailbox replacements " << mailbox.replaced_frames() << " | GPU slots unavailable " << unavailable
            << " | feed deadlines late by >1 frame " << late_feeds << '\n'
            << "Decode submit elapsed p95 ms " << percentile(submit_times, 0.95) << " | max ms " << percentile(submit_times, 1.0)
            << " | feed-to-Present return p95 ms " << percentile(feed_to_present_times, 0.95) << '\n'
            << "Draw CPU/lock p50/p95/p99 ms " << percentile(draw_times, 0.50) << '/' << percentile(draw_times, 0.95)
            << '/' << percentile(draw_times, 0.99) << " | created plane views " << resources.plane_views
            << " | created completion queries " << resources.completion_queries << '\n'
            << "Accepted-Present interval p50/p95/p99 ms " << percentile(present_intervals, 0.50) << '/'
            << percentile(present_intervals, 0.95) << '/' << percentile(present_intervals, 0.99)
            << " | feed-to-Present p99 ms " << percentile(feed_to_present_times, 0.99) << '\n'
            << "Steady decode submit elapsed p50/p95/p99 ms " << percentile(steady_submit_times, 0.50) << '/'
            << percentile(steady_submit_times, 0.95) << '/' << percentile(steady_submit_times, 0.99)
            << " | mean ms " << (steady_submit_times.empty() ? 0 : steady_submit_total / static_cast<double>(steady_submit_times.size())) << '\n'
            << "Encoded payload bytes mean/max " << static_cast<double>(input_bytes) / static_cast<double>(count)
            << '/' << maximum_input_bytes << " | steady input preparation p50/p95/p99 ms "
            << percentile(input_preparation_times, 0.50) << '/' << percentile(input_preparation_times, 0.95)
            << '/' << percentile(input_preparation_times, 0.99) << '\n'
            << "Decoder wrapper allocations packet/receive/retained " << decoder_resources.packet_wrappers << '/'
            << decoder_resources.receive_frame_wrappers << '/' << decoder_resources.retained_frame_wrappers << '\n'
            << "Green/white alpha checks passed after joined shutdown. Submit timings include driver waits. Offscreen calls do not measure Game Bar or monitor scanout.\n";
        return 0;
    } catch (const winrt::hresult_error& error) { std::cerr << winrt::to_string(error.message()) << '\n'; }
      catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    return 1;
}
