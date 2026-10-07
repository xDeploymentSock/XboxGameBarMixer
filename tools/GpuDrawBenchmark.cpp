// Owned NV12 through the production shader, measured with D3D11 timestamps.
// Synchronous query/readback is confined to this developer benchmark.
#include "D3D11Renderer.h"
#include <fuser/black_key.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
using clock_type = std::chrono::steady_clock;
constexpr UINT input_width = 2560;
constexpr UINT input_height = 1440;
constexpr int warmup_samples = 50;

void require(bool condition, const char* detail) {
    if (!condition) { throw std::runtime_error{detail}; }
}

int integer(std::string_view text, int minimum, int maximum) {
    int value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
        value >= minimum && value <= maximum, "Invalid numeric benchmark argument.");
    return value;
}

ComPtr<ID3D11Query> create_query(ID3D11Device* device, D3D11_QUERY type) {
    const D3D11_QUERY_DESC description{type, 0};
    ComPtr<ID3D11Query> result;
    winrt::check_hresult(device->CreateQuery(&description, result.GetAddressOf()));
    return result;
}

template<class T>
T read_query(ID3D11DeviceContext* context, ID3D11Query* query) {
    T result{};
    const auto deadline = clock_type::now() + std::chrono::seconds{5};
    for (;;) {
        const auto status = context->GetData(query, &result, sizeof(T), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        winrt::check_hresult(status);
        if (status == S_OK) { return result; }
        require(clock_type::now() < deadline, "GPU query timeout.");
        std::this_thread::yield();
    }
}

std::array<std::uint8_t, 4> read_pixel(fuser::windows::d3d11_renderer& renderer, UINT x, UINT y) {
    ComPtr<ID3D11Texture2D> back_buffer;
    ComPtr<ID3D11Texture2D> staging;
    winrt::check_hresult(renderer.swap_chain()->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf())));
    D3D11_TEXTURE2D_DESC description{};
    back_buffer->GetDesc(&description);
    require(x < description.Width && y < description.Height, "Readback coordinates out of bounds.");
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.MiscFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    winrt::check_hresult(renderer.device()->CreateTexture2D(&description, nullptr, staging.GetAddressOf()));
    renderer.context()->CopyResource(staging.Get(), back_buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(renderer.context()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    const auto* location = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch + x * 4;
    const std::array<std::uint8_t, 4> result{location[0], location[1], location[2], location[3]};
    renderer.context()->Unmap(staging.Get(), 0);
    return result;
}

fuser::decoded_frame make_owned_frame(ID3D11Device* device) {
    // Repeated 64-pixel bands: exact black, white, red, and near black.
    std::vector<std::uint8_t> pixels(input_width * input_height * 3 / 2, 128);
    constexpr std::array<std::uint8_t, 4> luma{16, 235, 81, 20};
    for (UINT row = 0; row < input_height; ++row) {
        for (UINT column = 0; column < input_width; ++column) {
            const auto band = (column / 64) % luma.size();
            pixels[row * input_width + column] = luma[band];
            if ((row & 1) == 0 && (column & 1) == 0) {
                const auto uv = input_width * input_height + (row / 2) * input_width + column;
                pixels[uv] = band == 2 ? 90 : 128;
                pixels[uv + 1] = band == 2 ? 240 : 128;
            }
        }
    }
    D3D11_TEXTURE2D_DESC description{};
    description.Width = input_width;
    description.Height = input_height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_NV12;
    description.SampleDesc.Count = 1;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA initial{pixels.data(), input_width, 0};
    ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(device->CreateTexture2D(&description, &initial, texture.GetAddressOf()));
    fuser::decoded_frame frame;
    frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
    frame.width = input_width;
    frame.height = input_height;
    frame.matrix = fuser::color_matrix::bt601;
    return frame;
}

double percentile(const std::vector<double>& sorted, double fraction) {
    return sorted[static_cast<std::size_t>(static_cast<double>(sorted.size() - 1) * fraction)];
}

void verify_pixels(fuser::windows::d3d11_renderer& renderer, UINT width, UINT height, int mode) {
    const auto black = read_pixel(renderer, width * 32 / input_width, height / 2);
    const auto white = read_pixel(renderer, width * 96 / input_width, height / 2);
    const auto dark = read_pixel(renderer, width * 224 / input_width, height / 2);
    require(black[3] == (mode == 2 ? 255 : 0), "Black alpha check failed.");
    require(white == std::array<std::uint8_t, 4>{255, 255, 255, 255}, "Opaque white check failed.");
    require(dark[3] == (mode == 1 ? 0 : 255), "Near-black cutoff check failed.");
}

void measure(fuser::windows::d3d11_renderer& renderer, const fuser::decoded_frame& frame,
    UINT width, UINT height, int sample_count, int mode) {
    auto key = fuser::black_key_settings({}, mode == 1 ?
        fuser::black_key_preset::noise_cutoff : fuser::black_key_preset::exact);
    if (mode == 2) { key = {}; }
    key.crisp_scaling = true;
    const auto start = create_query(renderer.device(), D3D11_QUERY_TIMESTAMP);
    const auto end = create_query(renderer.device(), D3D11_QUERY_TIMESTAMP);
    const auto disjoint = create_query(renderer.device(), D3D11_QUERY_TIMESTAMP_DISJOINT);
    std::vector<UINT64> ticks;
    ticks.reserve(static_cast<std::size_t>(sample_count));
    renderer.context()->Begin(disjoint.Get());
    for (int sample = 0; sample < sample_count + warmup_samples; ++sample) {
        renderer.context()->End(start.Get());
        require(renderer.draw_frame(frame, key).succeeded(), "GPU draw failed.");
        renderer.context()->End(end.Get());
        renderer.context()->Flush();
        const auto end_tick = read_query<UINT64>(renderer.context(), end.Get());
        const auto begin_tick = read_query<UINT64>(renderer.context(), start.Get());
        require(end_tick >= begin_tick, "Timestamp reversed.");
        if (sample >= warmup_samples) { ticks.push_back(end_tick - begin_tick); }
    }
    renderer.context()->End(disjoint.Get());
    renderer.context()->Flush();
    const auto frequency = read_query<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT>(renderer.context(), disjoint.Get());
    require(!frequency.Disjoint && frequency.Frequency != 0,
        "GPU timing is disjoint; repeat under stable GPU power/load.");
    verify_pixels(renderer, width, height, mode);
    std::vector<double> times;
    times.reserve(ticks.size());
    double sum{};
    for (const auto tick : ticks) {
        const auto ms = 1000.0 * static_cast<double>(tick) / static_cast<double>(frequency.Frequency);
        times.push_back(ms);
        sum += ms;
    }
    std::sort(times.begin(), times.end());
    constexpr std::array<std::string_view, 3> labels{"exact black", "near-black cutoff", "default green"};
    std::cout << labels[mode] << " GPU milliseconds p50/p95/p99 " << percentile(times, .5) << "/"
        << percentile(times, .95) << "/" << percentile(times, .99) << " mean "
        << sum / static_cast<double>(times.size()) << " samples " << times.size()
        << " disjoint false; alpha checks passed.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Usage: fuser_gpu_draw_benchmark <output width 256..8192> "
            "<output height 64..8192> <samples 1..10000>");
        const auto output_width = static_cast<UINT>(integer(argv[1], 256, 8192));
        const auto output_height = static_cast<UINT>(integer(argv[2], 64, 8192));
        const auto sample_count = integer(argv[3], 1, 10000);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        fuser::windows::d3d11_renderer renderer;
        renderer.initialize(output_width, output_height);
        const auto frame = make_owned_frame(renderer.device());
        for (int mode = 0; mode < 3; ++mode) {
            measure(renderer, frame, output_width, output_height, sample_count, mode);
        }
        std::cout << "Owned NV12 2560x1440 -> " << output_width << "x" << output_height
            << ". GPU draw commands only; excludes decode, Present, Game Bar and scanout.\n";
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << winrt::to_string(error.message()) << "\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
    }
    return 1;
}
