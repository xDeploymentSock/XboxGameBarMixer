// Production session/control code, loopback TLS fixture and real D3D11 renderer.
// Moonlight transport alone is simulated: these are not network/FPS measurements.
#include <OverlaySession.h>
#include <Limelight.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <winrt/base.h>
#include <stdexcept>

namespace {
thread_local bool fail_next_allocation{};
std::string_view scenario;
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error{message}; } }
struct memory_credentials final : fuser::streaming::credential_store {
    std::optional<fuser::streaming::client_identity> identity;
    std::string server;
    std::optional<fuser::streaming::client_identity> load_identity() override { return identity; }
    void save_identity(const fuser::streaming::client_identity& value) override { identity = value; }
    std::string load_server_certificate(const fuser::host_endpoint&) override { return server; }
    void save_server_certificate(const fuser::host_endpoint&, const std::string& value) override { server = value; }
};
}
// Injection is confined to the calling thread and disabled immediately after
// the callback. Other renderer/control threads never see the failure flag.
void* operator new(std::size_t size) {
    if (std::exchange(fail_next_allocation, false)) { throw std::bad_alloc{}; }
    if (auto* memory = std::malloc(size ? size : 1)) { return memory; }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

extern "C" {
const char* LiGetLaunchUrlQueryParameters() { return "&corever=1"; }
void LiInitializeStreamConfiguration(PSTREAM_CONFIGURATION value) { *value = {}; }
void LiInitializeServerInformation(PSERVER_INFORMATION value) { *value = {}; }
void LiInitializeVideoCallbacks(PDECODER_RENDERER_CALLBACKS value) { *value = {}; }
void LiInitializeAudioCallbacks(PAUDIO_RENDERER_CALLBACKS value) { *value = {}; }
void LiInitializeConnectionCallbacks(PCONNECTION_LISTENER_CALLBACKS value) { *value = {}; }
void LiStopConnection() {}
void LiInterruptConnection() {}
int LiGetPendingVideoFrames() { return 0; }
uint64_t LiGetMicroseconds() { return 0; }
const char* LiGetStageName(int) {
    return "Owned long stage name for injected allocation failure: abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz";
}
int LiStartConnection(PSERVER_INFORMATION, PSTREAM_CONFIGURATION, PCONNECTION_LISTENER_CALLBACKS callbacks,
    PDECODER_RENDERER_CALLBACKS, PAUDIO_RENDERER_CALLBACKS, void*, int, void*, int) {
    if (scenario == "session-stage-oom" || scenario == "session-end-oom") {
        fail_next_allocation = true;
        if (scenario == "session-stage-oom") { callbacks->stageStarting(0); }
        else { callbacks->connectionTerminated(-7); }
        const bool attempted = !fail_next_allocation;
        fail_next_allocation = false;
        require(attempted, "Callback did not reach the injected allocation failure.");
    }
    return 0;
}
}

int main(int argc, char** argv) {
    // A pre-fix noexcept failure must fail CTest without opening a crash dialog.
    std::set_terminate([] { std::_Exit(86); });
    try {
        require(argc == 3, "Expected loopback port and scenario.");
        scenario = argv[2];
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto control = std::make_shared<fuser::streaming::sunshine_control>(std::make_shared<memory_credentials>());
        const fuser::host_endpoint host{"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1]))};
        const auto paired = control->pair(host, "1234");
        require(paired.succeeded(), paired.detail.c_str());
        auto renderer = std::make_shared<fuser::windows::d3d11_renderer>();
        renderer->initialize(64, 64);
        fuser::streaming::overlay_session session{control};
        fuser::overlay_configuration config;
        config.host = host;
        session.prepare_action();
        session.resize(80, 48); // UI transferred ownership, begin has not run yet.
        const auto result = session.begin(config, {"881448767", "HUD"}, renderer, 0);
        if (scenario == "session-end-oom") {
            require(!result.succeeded() && session.snapshot().finished,
                "Terminal callback lost completion state under allocation failure.");
        } else {
            require(result.succeeded(), result.detail.c_str());
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            bool resized = false;
            while (std::chrono::steady_clock::now() < deadline) {
                {
                    const std::lock_guard lock{*renderer->context_lock()};
                    DXGI_SWAP_CHAIN_DESC1 description{};
                    winrt::check_hresult(renderer->swap_chain()->GetDesc1(&description));
                    resized = description.Width == 80 && description.Height == 48;
                }
                if (resized) { break; }
                std::this_thread::yield();
            }
            require(resized, "Startup discarded the UI's queued surface resize.");
        }
        session.stop();
        std::cout << scenario << ": simulated-transport session assertions passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
