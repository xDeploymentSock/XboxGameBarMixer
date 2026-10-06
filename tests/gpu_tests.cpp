// Hardware readback is used only by this test, never by the stream display path.
#include "D3D11Renderer.h"
#include <winrt/base.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
using pixel = std::array<std::uint8_t, 4>; // BGRA
constexpr std::uint32_t side = 64;

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error{message}; }
}

pixel read_pixel(fuser::windows::d3d11_renderer& renderer, std::uint32_t x, std::uint32_t y) {
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
    ComPtr<ID3D11DeviceContext> context;
    renderer.device()->GetImmediateContext(context.GetAddressOf());
    context->CopyResource(staging.Get(), back_buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    const auto* location = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
    const pixel result{location[0], location[1], location[2], location[3]};
    context->Unmap(staging.Get(), 0);
    return result;
}

bool close_to(std::uint8_t actual, int expected) {
    const auto difference = static_cast<int>(actual) - expected;
    return difference >= -2 && difference <= 2;
}

ComPtr<ID3D11Texture2D> make_nv12(fuser::windows::d3d11_renderer& renderer,
                                 std::uint8_t y, std::uint8_t u, std::uint8_t v,
                                 bool cropped = false) {
    std::vector<std::uint8_t> data(side * side * 3 / 2, y);
    for (std::size_t position = side * side; position < data.size(); position += 2) {
        data[position] = u;
        data[position + 1] = v;
    }
    if (cropped) {
        // A grey visible rectangle inside coloured padding exposes wrong UV
        // scaling and edge filtering on both NV12 planes.
        for (std::size_t row = 4; row < 36; ++row) {
            for (std::size_t column = 8; column < 40; ++column) {
                data[row * side + column] = 100;
            }
        }
        for (std::size_t row = 2; row < 18; ++row) {
            for (std::size_t column = 8; column < 40; column += 2) {
                data[side * side + row * side + column] = 128;
                data[side * side + row * side + column + 1] = 128;
            }
        }
    }
    D3D11_TEXTURE2D_DESC description{};
    description.Width = side;
    description.Height = side;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_NV12;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA initial{data.data(), side, 0};
    ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(renderer.device()->CreateTexture2D(&description, &initial, texture.GetAddressOf()));
    return texture;
}
}

int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        fuser::windows::d3d11_renderer renderer;
        renderer.initialize(side, side);
        ComPtr<IDXGIDevice> dxgi;
        winrt::check_hresult(renderer.device()->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf())));
        ComPtr<IDXGIAdapter> adapter;
        winrt::check_hresult(dxgi->GetAdapter(adapter.GetAddressOf()));
        DXGI_ADAPTER_DESC adapter_description{};
        winrt::check_hresult(adapter->GetDesc(&adapter_description));
        std::wcout << L"Hardware adapter: " << adapter_description.Description << L'\n';

        fuser::chroma_key_settings key;
        renderer.draw_diagnostic(key, 0);
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "Green diagnostic background must be transparent black.");
        require(read_pixel(renderer, 0, 0) == pixel{255, 255, 255, 255}, "Diagnostic border must remain opaque white.");
        key.color = {1.0F, 0.0F, 1.0F};
        renderer.draw_diagnostic(key, 0);
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "Magenta diagnostic background must be transparent black.");
        key.opacity = 0.5F;
        renderer.draw_diagnostic(key, 0);
        const auto half_white = read_pixel(renderer, 0, 0);
        for (const auto component : half_white) {
            require(close_to(component, 128), "Half-opacity white must use premultiplied RGB and alpha.");
        }
        renderer.clear();
        require(read_pixel(renderer, 0, 0) == pixel{0, 0, 0, 0}, "Clearing must erase opaque HUD pixels.");

        auto texture = make_nv12(renderer, 100, 128, 128);
        fuser::decoded_frame frame;
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        frame.width = side;
        frame.height = side;
        frame.range = fuser::color_range::full;
        key.enabled = false;
        key.opacity = 1.0F;
        require(renderer.draw_frame(frame, key).succeeded(), "SDR NV12 GPU frame must be accepted.");
        const auto grey = read_pixel(renderer, 6, 22);
        require(close_to(grey[0], 100) && close_to(grey[1], 100) && close_to(grey[2], 100) && grey[3] == 255,
                "Full-range neutral NV12 must convert to opaque neutral RGB.");
        const std::weak_ptr<const fuser::gpu_surface> lease{frame.surface};
        frame.surface.reset();
        require(!lease.expired(), "GPU reader must retain the submitted surface until explicit retirement.");

        texture = make_nv12(renderer, 145, 54, 34);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        frame.matrix = fuser::color_matrix::bt601;
        frame.range = fuser::color_range::limited;
        key.enabled = true;
        key.color = {0.0F, 1.0F, 0.0F};
        require(renderer.draw_frame(frame, key).succeeded(), "Limited-range green NV12 frame must be accepted.");
        require(lease.expired(), "Completed GPU readers must release old surface leases.");
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "NV12-converted green must be removed by chroma keying.");
        frame.format = fuser::pixel_format::p010;
        require(renderer.draw_frame(frame, key).code == fuser::operation_code::unsupported_format,
                "Unsupported HDR input must be reported explicitly.");
        frame.format = fuser::pixel_format::nv12;
        frame.matrix = fuser::color_matrix::bt709;
        frame.range = fuser::color_range::full;
        frame.width = 32;
        frame.height = 32;
        frame.source_x = 8;
        frame.source_y = 4;
        texture = make_nv12(renderer, 105, 212, 235, true);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        key.enabled = false;
        require(renderer.draw_frame(frame, key).succeeded(), "A cropped, padded NV12 frame must be accepted.");
        for (const auto location : std::array{std::array{0U, 0U}, std::array{63U, 63U}}) {
            const auto corner = read_pixel(renderer, location[0], location[1]);
            require(close_to(corner[0], 100) && close_to(corner[1], 100) && close_to(corner[2], 100)
                    && corner[3] == 255, "Allocation padding must not bleed into either visible edge.");
        }
        frame.source_x = 9;
        require(renderer.draw_frame(frame, key).code == fuser::operation_code::unsupported_format,
                "Chroma-misaligned source rectangles must be rejected.");
        frame.source_x = side;
        require(renderer.draw_frame(frame, key).code == fuser::operation_code::unsupported_format,
                "Out-of-bounds source rectangles must be rejected.");
        renderer.resize(128, 96);
        renderer.draw_diagnostic(key, 0);
        require(read_pixel(renderer, 127, 95) == pixel{255, 255, 255, 255}, "Resize must recreate the render target and viewport.");
        std::cout << "GPU alpha, NV12 conversion/cropping, resource retirement, clear and resize checks passed.\n";
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << winrt::to_string(error.message()) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
