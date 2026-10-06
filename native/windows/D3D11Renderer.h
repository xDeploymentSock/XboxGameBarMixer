#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <memory>
#include <mutex>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <fuser/configuration.h>
#include <fuser/stream_interfaces.h>
#include "D3D11Surface.h"

namespace fuser::windows {

// Renderer methods belong to one owner thread. FFmpeg's D3D11 callbacks must
// share context_lock() when using the immediate context on another thread.
class d3d11_renderer final {
public:
    d3d11_renderer() = default;
    ~d3d11_renderer() noexcept;
    d3d11_renderer(const d3d11_renderer&) = delete;
    d3d11_renderer& operator=(const d3d11_renderer&) = delete;
    d3d11_renderer(d3d11_renderer&&) = delete;
    d3d11_renderer& operator=(d3d11_renderer&&) = delete;
    void initialize(std::uint32_t width, std::uint32_t height);
    void resize(std::uint32_t width, std::uint32_t height);
    void draw_diagnostic(const chroma_key_settings& key, std::uint64_t sequence);
    [[nodiscard]] operation_result draw_frame(const decoded_frame& frame,
                                             const chroma_key_settings& key);
    void present();
    void clear();

    [[nodiscard]] ID3D11Device* device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* context() const noexcept { return context_.Get(); }
    [[nodiscard]] std::shared_ptr<std::recursive_mutex> context_lock() const noexcept { return context_lock_; }
    [[nodiscard]] IDXGISwapChain1* swap_chain() const noexcept { return swap_chain_.Get(); }
    [[nodiscard]] std::uint64_t present_calls() const noexcept { return present_calls_; }

private:
    struct alignas(16) shader_parameters {
        std::array<float, 4> key_color_tolerance{};
        std::array<float, 4> controls{};
        std::array<float, 4> dimensions_sequence{};
        std::array<float, 4> offsets{};
        std::array<float, 4> matrix_row0{};
        std::array<float, 4> matrix_row1{};
        std::array<float, 4> matrix_row2{};
        std::array<float, 4> source_rectangle{};
        std::array<float, 4> key_options{};
    };
    static_assert(sizeof(shader_parameters) == 144);

    [[nodiscard]] shader_parameters parameters(const chroma_key_settings& key,
                                               std::uint64_t sequence) const;
    void create_target();
    void draw(const shader_parameters& parameters);
    void retire_completed_frames();

    struct in_flight_frame {
        decoded_frame frame;
        Microsoft::WRL::ComPtr<ID3D11Query> completed;
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    std::shared_ptr<std::recursive_mutex> context_lock_{std::make_shared<std::recursive_mutex>()};
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::uint64_t present_calls_{};
    // Retain decoder leases until the GPU completes its reads, not just Draw().
    std::array<std::optional<in_flight_frame>, 3> in_flight_{};
};

} // namespace fuser::windows
