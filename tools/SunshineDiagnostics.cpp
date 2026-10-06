#include "../native/widget/AppCredentials.h"
#include "../native/streaming/OverlaySession.h"
#include <winrt/Windows.Foundation.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>

namespace {
int bounded_integer(std::string_view value, int minimum, int maximum) {
    int result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < minimum || result > maximum) {
        throw std::invalid_argument{"Numeric argument is outside its supported range."};
    }
    return result;
}
}

// Developer CLI uses only this widget's previously paired, protected identity.
// No pairing, quit command, input events, audio playback, or screen display.
int main(int argc, char** argv) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        if (argc != 4 && argc != 7 && argc != 8 && argc != 9 && argc != 10) {
            std::cerr << "Usage: fuser_sunshine_diagnostics <own LocalState> <host> apps|stream [appid h264|hevc fps [seconds=12 [cycles=1 [bitrate_kbps=80000]]]]\n";
            return 2;
        }
        const bool enumerate = std::string_view{argv[3]} == "apps" && argc == 4;
        if (!enumerate && (std::string_view{argv[3]} != "stream" || argc < 7)) { return 2; }
        const auto duration = !enumerate && argc >= 8 ? bounded_integer(argv[7], 1, 3600) : 12;
        const auto cycles = !enumerate && argc >= 9 ? bounded_integer(argv[8], 1, 100) : 1;
        const auto bitrate = !enumerate && argc == 10 ? bounded_integer(argv[9], 1000, 200000) : 80000;
        if (duration * cycles > 3600) { throw std::invalid_argument{"Total streaming duration must not exceed one hour."}; }
        const auto fps = enumerate ? 0 : bounded_integer(argv[6], 1, 500);
        if (!enumerate && std::string_view{argv[5]} != "h264" && std::string_view{argv[5]} != "hevc") {
            std::cerr << "Select h264 or hevc.\n"; return 2;
        }
        auto control = std::make_shared<fuser::streaming::sunshine_control>(
            std::make_shared<fuser::widget::app_credentials>(std::filesystem::path{argv[1]}));
        const fuser::host_endpoint host{argv[2]};
        fuser::streaming::host_information info;
        auto result = control->inspect(host, info);
        if (!result.succeeded()) { std::cerr << result.detail << '\n'; return 1; }
        std::cout << info.name << " | " << info.state << " | active app " << info.current_application << '\n';
        std::vector<fuser::host_application> apps;
        result = control->list_applications(host, apps);
        if (!result.succeeded()) { std::cerr << result.detail << '\n'; return 1; }
        for (const auto& app : apps) { std::cout << app.id << " : " << app.name << '\n'; }
        if (enumerate) { return 0; }
        fuser::overlay_configuration config;
        config.host = host;
        config.stream.frames_per_second = static_cast<std::uint32_t>(fps);
        config.stream.bitrate_kbps = static_cast<std::uint32_t>(bitrate);
        config.stream.codec = std::string_view{argv[5]} == "hevc" ? fuser::video_codec::hevc : fuser::video_codec::h264;
        std::cout << "Requested " << config.stream.width << 'x' << config.stream.height << " / " << fps
                  << " FPS / " << bitrate << " kbps\n";
        const auto found = std::find_if(apps.begin(), apps.end(), [&](const auto& app) { return app.id == argv[4]; });
        if (found == apps.end()) { std::cerr << "Select an actual host app ID.\n"; return 2; }
        auto renderer = std::make_shared<fuser::windows::d3d11_renderer>();
        renderer->initialize(config.stream.width, config.stream.height);
        std::mutex output_mutex;
        fuser::streaming::overlay_session session{control, [&output_mutex](const std::string& text) {
            const std::lock_guard lock{output_mutex}; std::cout << "Transport: " << text << std::flush;
        }};
        // Reuse the controller, credential store and GPU renderer across cycles,
        // matching Disconnect -> Connect rather than creating a new process.
        for (int cycle = 0; cycle < cycles; ++cycle) {
            std::cout << "Cycle " << cycle + 1 << '/' << cycles << '\n' << std::flush;
            session.prepare_action();
            result = session.begin(config, *found, renderer, 0);
            if (!result.succeeded()) { std::cerr << result.detail << '\n'; return 1; }
            for (int second = 0; second < duration; ++second) {
                std::this_thread::sleep_for(std::chrono::seconds{1});
                const auto state = session.snapshot();
                { const std::lock_guard lock{output_mutex};
                    std::cout << "At " << second + 1 << "s: units " << state.counters.received_frames
                        << " decoded " << state.counters.decoded_frames << " presents " << state.counters.present_calls
                        << " decode errors " << state.decode_errors << " status " << state.status << '\n' << std::flush;
                    std::cout << "Frame index " << state.last_frame_number << " skipped before callback " << state.missing_frame_numbers
                        << " peak decode queue " << state.peak_decode_queue << " decode average ms "
                        << (state.counters.received_frames ? static_cast<double>(state.decode_microseconds) / 1000.0 / static_cast<double>(state.counters.received_frames) : 0.0)
                        << " decode maximum ms " << static_cast<double>(state.max_decode_microseconds) / 1000.0 << '\n' << std::flush;
                    std::cout << "Receiver timing samples " << state.receive_timing_samples << " frame assembly average ms "
                        << (state.receive_timing_samples ? static_cast<double>(state.assembly_microseconds) / 1000.0 / static_cast<double>(state.receive_timing_samples) : 0.0)
                        << " enqueue-to-submission average ms "
                        << (state.receive_timing_samples ? static_cast<double>(state.queue_microseconds) / 1000.0 / static_cast<double>(state.receive_timing_samples) : 0.0)
                        << " maximum ms " << static_cast<double>(state.max_queue_microseconds) / 1000.0 << '\n';
                    std::cout << "Host nonzero processing samples " << state.host_latency_samples << " average ms "
                        << (state.host_latency_samples ? static_cast<double>(state.host_latency_tenths_ms) / 10.0 / static_cast<double>(state.host_latency_samples) : 0.0)
                        << " maximum ms " << static_cast<double>(state.max_host_latency_tenths_ms) / 10.0
                        << " absent/repeated " << state.zero_host_latency_frames << '\n' << std::flush;
                }
                if (state.finished) { break; }
            }
            const auto before = std::chrono::steady_clock::now();
            session.stop();
            std::cout << "Joined stop: " << std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count() << " seconds.\n";
            const auto final = session.snapshot();
            if (final.render_latency_samples) {
                std::cout << "Local callback-to-accepted-Present average ms "
                          << static_cast<double>(final.render_microseconds) / 1000.0 / static_cast<double>(final.render_latency_samples)
                          << " maximum ms " << static_cast<double>(final.max_render_microseconds) / 1000.0
                          << ". Excludes host, network and Game Bar/monitor scanout.\n";
            }
            std::cout << "Local latency p95/p99 bound ms " << static_cast<double>(final.render_timing.p95_microseconds) / 1000.0
                      << '/' << static_cast<double>(final.render_timing.p99_microseconds) / 1000.0
                      << " | accepted-Present gaps p95/p99 bound ms " << static_cast<double>(final.present_intervals.p95_microseconds) / 1000.0
                      << '/' << static_cast<double>(final.present_intervals.p99_microseconds) / 1000.0
                      << " | maximum gap ms " << static_cast<double>(final.present_intervals.max_microseconds) / 1000.0 << '\n';
            std::cout << "Display replacements " << final.counters.replaced_display_frames
                      << " | replaced pending " << final.replaced_pending_frames
                      << " | GPU slot retries " << final.gpu_slot_retries << " | Present retries " << final.present_retries
                      << " | presentation wait timeouts " << final.presentation_wait_timeouts << '\n';
            if (!final.counters.decoded_frames || !final.counters.present_calls || final.decode_errors || final.finished) { return 1; }
            renderer->clear();
            renderer->present();
        }
        return 0;
    } catch (const winrt::hresult_error& error) { std::cerr << winrt::to_string(error.message()) << '\n'; return 1; }
      catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
