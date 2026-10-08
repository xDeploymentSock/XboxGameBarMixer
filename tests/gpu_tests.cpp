// Hardware readback is used only by this test, never by the stream display path.
#include "D3D11Renderer.h"
#include <fuser/black_key.h>
#include <cmath>
#include <winrt/base.h>
#include <algorithm>
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

void check_array_resource_reuse() {
    fuser::windows::d3d11_renderer renderer;
    renderer.initialize(side, side);
    D3D11_TEXTURE2D_DESC description{};
    description.Width = description.Height = side;
    description.MipLevels = 1;
    description.ArraySize = 2;
    description.Format = DXGI_FORMAT_NV12;
    description.SampleDesc.Count = 1;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    std::array<std::vector<std::uint8_t>, 2> planes;
    std::array<D3D11_SUBRESOURCE_DATA, 2> initial;
    for (std::size_t slice = 0; slice < planes.size(); ++slice) {
        planes[slice].assign(side * side * 3 / 2, 128);
        std::fill_n(planes[slice].begin(), side * side, slice == 0 ? std::uint8_t{0} : std::uint8_t{255});
        initial[slice] = {planes[slice].data(), side, side * side * 3 / 2};
    }
    ComPtr<ID3D11Texture2D> texture;
    winrt::check_hresult(renderer.device()->CreateTexture2D(&description, initial.data(), texture.GetAddressOf()));
    fuser::chroma_key_settings key;
    key.color = {0.0F, 0.0F, 0.0F};
    key.tolerance = key.softness = 0.0F;
    for (std::uint32_t index = 0; index < 40; ++index) {
        const auto slice = index % 2;
        fuser::decoded_frame frame;
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, slice);
        frame.width = frame.height = side;
        frame.range = fuser::color_range::full;
        key.crisp_scaling = (index / 2) % 2 == 0;
        require(renderer.draw_frame(frame, key).succeeded(), "Alternating decoder array slices must render.");
        const auto actual = read_pixel(renderer, side / 2, side / 2);
        if (actual != (slice == 0 ? pixel{0, 0, 0, 0} : pixel{255, 255, 255, 255})) {
            std::cerr << "Array fixture slice " << slice << " crisp " << key.crisp_scaling << " BGRA "
                      << unsigned(actual[0]) << ',' << unsigned(actual[1]) << ',' << unsigned(actual[2]) << ',' << unsigned(actual[3]) << '\n';
        }
        require(actual == (slice == 0 ? pixel{0, 0, 0, 0} : pixel{255, 255, 255, 255}),
                "Reused NV12 views must select the correct slice in both scaling modes.");
    }
    const auto counts = renderer.resources_created();
    require(counts.plane_views == 4 && counts.completion_queries == 3,
            "Steady rendering must reuse one plane-view pair per pool slice and three GPU completion queries.");
    renderer.clear();
    require(read_pixel(renderer, 0, 0) == pixel{0, 0, 0, 0}, "Clearing cached video must stay transparent.");
    fuser::decoded_frame frame;
    frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 1);
    frame.width = frame.height = side;
    frame.range = fuser::color_range::full;
    require(renderer.draw_frame(frame, key).succeeded(), "Rendering must restore views after clearing video resources.");
    require(read_pixel(renderer, 0, 0) == pixel{255, 255, 255, 255}, "Restored views must preserve the selected slice.");
    require(renderer.resources_created().plane_views == 6, "Clear must release the cached decoder texture views.");
}

ComPtr<ID3D11Texture2D> make_nv12(fuser::windows::d3d11_renderer& renderer,
                                 std::uint8_t y, std::uint8_t u, std::uint8_t v,
                                 bool cropped = false, bool corner_markers = false,
                                 bool thin_strokes = false) {
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
    if (corner_markers) {
        for (std::uint32_t row = 0; row < side; ++row) {
            for (std::uint32_t column = 0; column < side; ++column) {
                if ((row < 12 || row >= side - 12) && (column < 12 || column >= side - 12)) {
                    data[row * side + column] = 255;
                }
            }
        }
    }
    if (thin_strokes) {
        for (std::uint32_t row = 0; row < side; ++row) {
            for (std::uint32_t column = 0; column < side; ++column) {
                data[row * side + column] = column % 2 == 0 ? 255 : 0;
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

// Use decoded RGB with keying disabled as the reference: this isolates
// alpha/keying errors from the unavoidable YUV conversion quantization.
void check_natural_black_colors() {
    fuser::windows::d3d11_renderer renderer;
    renderer.initialize(side, side);
    const std::array<pixel, 6> colors{{
        {15, 18, 200, 255}, {45, 175, 215, 255}, {160, 110, 25, 255},
        {150, 35, 175, 255}, {100, 100, 100, 255}, {1, 1, 1, 255}}};
    for (const auto matrix : {fuser::color_matrix::bt601, fuser::color_matrix::bt709}) {
        const double kr = matrix == fuser::color_matrix::bt601 ? 0.299 : 0.2126;
        const double kb = matrix == fuser::color_matrix::bt601 ? 0.114 : 0.0722;
        for (const auto range : {fuser::color_range::full, fuser::color_range::limited}) {
            const bool limited = range == fuser::color_range::limited;
            const auto byte = [](double value) {
                return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
            };
            for (const auto color : colors) {
                const double y = kr * color[2] + (1.0 - kr - kb) * color[1] + kb * color[0];
                const double u = (color[0] - y) / (2.0 * (1.0 - kb));
                const double v = (color[2] - y) / (2.0 * (1.0 - kr));
                const auto texture = make_nv12(renderer, byte(limited ? 16.0 + y * 219.0 / 255.0 : y),
                    byte(128.0 + u * (limited ? 224.0 / 255.0 : 1.0)),
                    byte(128.0 + v * (limited ? 224.0 / 255.0 : 1.0)));
                fuser::decoded_frame frame;
                frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
                frame.width = frame.height = side;
                frame.matrix = matrix;
                frame.range = range;
                fuser::chroma_key_settings reference;
                reference.enabled = false;
                require(renderer.draw_frame(frame, reference).succeeded(), "Unkeyed color reference must render.");
                const auto original = read_pixel(renderer, 6, 22);
                require(original[3] == 255, "Unkeyed reference must be opaque.");
                for (const auto preset : {fuser::black_key_preset::exact, fuser::black_key_preset::noise_cutoff}) {
                    const auto key = fuser::black_key_settings({}, preset);
                    require(renderer.draw_frame(frame, key).succeeded(), "Natural-color black preset must render.");
                    const auto actual = read_pixel(renderer, 6, 22);
                    // Near-black is preserved by exact removal and removed only
                    // when the separate noise preset is explicitly selected.
                    if (preset == fuser::black_key_preset::noise_cutoff && color[2] == 1) {
                        require(actual == pixel{0, 0, 0, 0}, "Noise preset must remove near-black rather than partially fade it.");
                        continue;
                    }
                    if (actual != original) {
                        std::cerr << "Natural-color BGRA reference " << unsigned(original[0]) << ',' << unsigned(original[1])
                                  << ',' << unsigned(original[2]) << ',' << unsigned(original[3]) << " keyed "
                                  << unsigned(actual[0]) << ',' << unsigned(actual[1]) << ',' << unsigned(actual[2])
                                  << ',' << unsigned(actual[3]) << '\n';
                    }
                    require(actual == original, "Black presets must preserve every retained decoded RGB channel and opaque alpha.");
                    // At alpha 255, compositing over any background cannot tint the source.
                    for (const int background : {0, 110, 255}) {
                        require(actual[2] + background * (255 - actual[3]) / 255 == original[2],
                                "A solid HUD color must not absorb the receiving background.");
                    }
                }
            }
        }
    }
}

// A source texel must reconstruct to the same RGB after Crisp HUD resizing.
// Matching nearest luma with destination-position chroma otherwise varies the
// color even when repeated destination pixels select the same decoded texel.
void check_crisp_color_stability() {
    fuser::windows::d3d11_renderer renderer;
    renderer.initialize(side, side);
    for (const auto matrix : {fuser::color_matrix::bt601, fuser::color_matrix::bt709}) {
        const double kr = matrix == fuser::color_matrix::bt601 ? 0.299 : 0.2126;
        const double kb = matrix == fuser::color_matrix::bt601 ? 0.114 : 0.0722;
        for (const auto range : {fuser::color_range::full, fuser::color_range::limited}) {
            const bool limited = range == fuser::color_range::limited;
            const auto byte = [](double value) {
                return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
            };
            std::vector<std::uint8_t> data(side * side * 3 / 2);
            const std::array<pixel, 4> colors{{
                {0, 0, 255, 255}, {0, 0, 0, 255}, {255, 0, 0, 255}, {0, 0, 0, 255}}};
            for (std::uint32_t row = 0; row < side; ++row) {
                for (std::uint32_t column = 0; column < side; ++column) {
                    const auto color = colors[(row / 4 + column / 4) % colors.size()];
                    const double y = kr * color[2] + (1.0 - kr - kb) * color[1] + kb * color[0];
                    data[row * side + column] = byte(limited ? 16.0 + y * 219.0 / 255.0 : y);
                    if (((row | column) & 1U) == 0) {
                        const double u = (color[0] - y) / (2.0 * (1.0 - kb));
                        const double v = (color[2] - y) / (2.0 * (1.0 - kr));
                        const auto offset = side * side + row / 2 * side + column;
                        data[offset] = byte(128.0 + u * (limited ? 224.0 / 255.0 : 1.0));
                        data[offset + 1] = byte(128.0 + v * (limited ? 224.0 / 255.0 : 1.0));
                    }
                }
            }
            D3D11_TEXTURE2D_DESC description{};
            description.Width = description.Height = side;
            description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
            description.Format = DXGI_FORMAT_NV12;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            const D3D11_SUBRESOURCE_DATA initial{data.data(), side, 0};
            ComPtr<ID3D11Texture2D> texture;
            winrt::check_hresult(renderer.device()->CreateTexture2D(&description, &initial, texture.GetAddressOf()));
            fuser::decoded_frame frame;
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            frame.width = frame.height = side;
            frame.matrix = matrix;
            frame.range = range;
            auto key = fuser::black_key_settings({}, fuser::black_key_preset::exact);
            key.crisp_scaling = true;
            renderer.resize(side, side);
            require(renderer.draw_frame(frame, key).succeeded(), "Crisp color reference must render.");
            std::array<pixel, 16> reference{};
            for (std::uint32_t row = 0; row < reference.size(); ++row) {
                reference[row] = read_pixel(renderer, 2, row);
            }
            // Upscaling repeats decoded texels at different fractional phases;
            // vertical compression also reproduces the taskbar fitting path.
            // Use phases away from exact texel-selection boundaries, where
            // CPU double and GPU raster interpolation can choose either neighbor.
            unsigned int changed_pixels{}, largest_channel_change{};
            for (const auto destination : std::array{std::array{128U, 99U}, std::array{64U, 58U}}) {
                renderer.resize(destination[0], destination[1]);
                require(renderer.draw_frame(frame, key).succeeded(), "Scaled colored HUD must render.");
                const auto x = destination[0] == 128 ? 4U : 2U;
                for (std::uint32_t row = 0; row < destination[1]; ++row) {
                    const auto source_row = static_cast<std::uint32_t>(
                        (static_cast<double>(row) + 0.5) * side / destination[1]);
                    if (source_row >= reference.size()) { break; }
                    const auto actual = read_pixel(renderer, x, row);
                    const auto expected = reference[source_row];
                    if (actual != expected) {
                        ++changed_pixels;
                        std::cerr << "Mismatch destination " << destination[0] << 'x' << destination[1]
                                  << " row " << row << " source row " << source_row << '\n';
                    }
                    for (std::size_t channel = 0; channel < actual.size(); ++channel) {
                        largest_channel_change = std::max(largest_channel_change,
                            static_cast<unsigned int>(std::abs(static_cast<int>(actual[channel]) - static_cast<int>(expected[channel]))));
                    }
                }
            }
            std::cout << "Crisp color phase mismatch pixels " << changed_pixels
                      << " | largest BGRA change " << largest_channel_change << '\n';
            require(changed_pixels == 0,
                    "Crisp HUD must preserve reconstructed source RGB across destination sampling phases.");
        }
    }
}

int main() {
    try {
        check_natural_black_colors();
        check_crisp_color_stability();
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        fuser::windows::d3d11_renderer renderer;
        renderer.initialize(side, side);
        DXGI_SWAP_CHAIN_DESC1 chain_description{};
        winrt::check_hresult(renderer.swap_chain()->GetDesc1(&chain_description));
        require((chain_description.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0,
                "Presentation must use a waitable chain to wait before taking the newest decoded frame.");
        ComPtr<IDXGISwapChain2> paced_chain;
        winrt::check_hresult(renderer.swap_chain()->QueryInterface(IID_PPV_ARGS(paced_chain.GetAddressOf())));
        UINT maximum_latency{};
        winrt::check_hresult(paced_chain->GetMaximumFrameLatency(&maximum_latency));
        require(maximum_latency == 1, "The composition queue must be limited to one frame.");
        require(renderer.wait_to_present(100) && renderer.wait_to_present(0),
                "A ready presentation slot remains available across a deferred draw.");
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
        key.color = {0.0F, 0.0F, 0.0F};
        key.tolerance = 0.0F;
        key.softness = 0.0F;
        renderer.draw_diagnostic(key, 0);
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "Black diagnostic background must be transparent black.");
        require(read_pixel(renderer, 0, 0) == pixel{255, 255, 255, 255}, "Black key must preserve white corner markers.");
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
        key.tolerance = 0.12F;
        key.softness = 0.08F;
        require(renderer.draw_frame(frame, key).succeeded(), "Limited-range green NV12 frame must be accepted.");
        require(lease.expired(), "Completed GPU readers must release old surface leases.");
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "NV12-converted green must be removed by chroma keying.");

        // Exercise the actual NV12 shader path in both stream colour ranges.
        key.color = {0.0F, 0.0F, 0.0F};
        key.tolerance = 0.0F;
        key.softness = 0.0F;
        frame.matrix = fuser::color_matrix::bt709;
        for (const auto range : {fuser::color_range::limited, fuser::color_range::full}) {
            frame.range = range;
            const bool limited = range == fuser::color_range::limited;
            texture = make_nv12(renderer, limited ? 16 : 0, 128, 128);
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            require(renderer.draw_frame(frame, key).succeeded(), "Black NV12 frame must be accepted.");
            require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "Decoded black must produce zero premultiplied RGB and alpha.");

            texture = make_nv12(renderer, limited ? 17 : 1, 128, 128);
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            require(renderer.draw_frame(frame, key).succeeded(), "Near-black NV12 frame must be accepted.");
            const auto dark = read_pixel(renderer, 6, 22);
            require(dark[0] > 0 && dark[1] > 0 && dark[2] > 0 && dark[3] == 255,
                    "Exact black mode must preserve nonblack dark HUD pixels.");

            texture = make_nv12(renderer, limited ? 235 : 255, 128, 128);
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            require(renderer.draw_frame(frame, key).succeeded(), "White NV12 frame must be accepted.");
            require(read_pixel(renderer, 6, 22) == pixel{255, 255, 255, 255}, "Black key must preserve decoded white.");
        }
        frame.range = fuser::color_range::full;
        texture = make_nv12(renderer, 100, 128, 128);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        key.tolerance = 0.6F;
        key.softness = 0.2F;
        require(renderer.draw_frame(frame, key).succeeded(), "Soft black-key edge must be accepted.");
        const auto soft = read_pixel(renderer, 6, 22);
        require(soft[3] > 0 && soft[3] < 255 && close_to(soft[0], 100 * soft[3] / 255)
                && soft[0] == soft[1] && soft[1] == soft[2], "Soft black-key edges must remain neutral and premultiplied.");
        key.tolerance = 0.02F;
        key.softness = 0.0F;
        texture = make_nv12(renderer, 2, 128, 128);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        require(renderer.draw_frame(frame, key).succeeded(), "Black-key noise tolerance must be accepted.");
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0}, "Optional tolerance must remove near-black compression noise.");
        // White antialiased artwork over black already carries coverage in RGB.
        // Threshold-only alpha turns those grey edge samples into a dark fringe
        // over a white receiver background, even after low-level noise is cut.
        key.tolerance = 0.12F;
        key.softness = 0.08F;
        key.recover_black_edges = true;
        for (const auto range : {fuser::color_range::limited, fuser::color_range::full}) {
            frame.range = range;
            const bool limited = range == fuser::color_range::limited;
            texture = make_nv12(renderer, limited ? 26 : 10, 130, 126);
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            require(renderer.draw_frame(frame, key).succeeded(), "Noisy black with chroma deviations must be accepted.");
            require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0},
                    "Cleanup must remove near-black luma and chroma residue.");
            for (const auto level : std::array<std::uint8_t, 4>{32, 64, 128, 192}) {
                const auto luma = limited ? static_cast<std::uint8_t>(16 + level * 219 / 255) : level;
                texture = make_nv12(renderer, luma, 128, 128);
                frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
                require(renderer.draw_frame(frame, key).succeeded(), "Antialiased white edge must be accepted.");
                const auto edge = read_pixel(renderer, 6, 22);
                require(edge[3] > 0 && edge[3] < 255, "Bright HUD edges must recover fractional coverage.");
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    require(edge[channel] <= edge[3], "Cleanup output must remain premultiplied.");
                    require(edge[channel] + 255 - edge[3] >= 253,
                            "White HUD edges must not leave a dark fringe over white.");
                }
            }
            texture = make_nv12(renderer, limited ? 235 : 255, 128, 128);
            frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
            require(renderer.draw_frame(frame, key).succeeded(), "White HUD body must be accepted.");
            require(read_pixel(renderer, 6, 22) == pixel{255, 255, 255, 255},
                    "Cleanup must preserve the opaque white body of the HUD.");
        }
        frame.range = fuser::color_range::full;
        texture = make_nv12(renderer, 128, 128, 128);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        key.opacity = 0.5F;
        require(renderer.draw_frame(frame, key).succeeded(), "Recovered edges must support overall HUD opacity.");
        const auto faded_edge = read_pixel(renderer, 6, 22);
        require(close_to(faded_edge[3], 64) && faded_edge[0] == faded_edge[3] &&
                faded_edge[1] == faded_edge[3] && faded_edge[2] == faded_edge[3],
                "HUD opacity must scale recovered coverage and premultiplied colour exactly once.");
        key.opacity = 1.0F;
        key.enabled = false;
        require(renderer.draw_frame(frame, key).succeeded(), "Disabled keying must ignore edge recovery.");
        require(read_pixel(renderer, 6, 22) == pixel{128, 128, 128, 255},
                "Disabled keying must preserve opaque source colours.");
        key.enabled = true;
        key.recover_black_edges = false;
        require(renderer.draw_frame(frame, key).succeeded(), "Recovery can be disabled independently of noise tolerance.");
        require(read_pixel(renderer, 6, 22) == pixel{128, 128, 128, 255},
                "Disabling recovery must preserve solid dark panels above the cutoff.");
        texture = make_nv12(renderer, 145, 54, 34);
        frame.matrix = fuser::color_matrix::bt601;
        frame.range = fuser::color_range::limited;
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        require(renderer.draw_frame(frame, key).succeeded(), "Coloured HUD under black key must be accepted.");
        const auto green_hud = read_pixel(renderer, 6, 22);
        require(green_hud[1] >= 254 && green_hud[3] == 255, "Black key must preserve coloured HUD pixels without spill suppression.");
        key.recover_black_edges = true;
        require(renderer.draw_frame(frame, key).succeeded(), "Black edge recovery must accept coloured HUD content.");
        require(read_pixel(renderer, 6, 22) == green_hud,
                "Recovery must preserve fully bright coloured HUD pixels.");
        key.color = {0.0F, 1.0F, 0.0F};
        require(renderer.draw_frame(frame, key).succeeded(), "Green keying must accept the saved recovery option.");
        require(read_pixel(renderer, 6, 22) == pixel{0, 0, 0, 0},
                "Black edge recovery must not interfere with the green key.");
        key.color = {1.0F, 0.0F, 1.0F};
        key.tolerance = 0.0F;
        key.softness = 0.0F;
        frame.matrix = fuser::color_matrix::bt709;
        frame.range = fuser::color_range::full;
        texture = make_nv12(renderer, 128, 128, 128);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        require(renderer.draw_frame(frame, key).succeeded(), "Nonblack keying must accept neutral HUD content.");
        require(read_pixel(renderer, 6, 22) == pixel{128, 128, 128, 255},
                "Black recovery must leave the magenta-key path unchanged.");
        key.recover_black_edges = false;
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
        winrt::check_hresult(renderer.swap_chain()->GetDesc1(&chain_description));
        require((chain_description.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0,
                "Resizing must preserve the waitable presentation flag.");
        renderer.draw_diagnostic(key, 0);
        require(read_pixel(renderer, 127, 95) == pixel{255, 255, 255, 255}, "Resize must recreate the render target and viewport.");
        // Use a full source frame with four markers, then deliberately change
        // its aspect ratio to the host's usable area. Source edges must survive
        // destination scaling, while the black background stays transparent.
        texture = make_nv12(renderer, 0, 128, 128, false, true);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        frame.source_x = frame.source_y = 0;
        frame.width = frame.height = side;
        key.enabled = true;
        key.color = {0.0F, 0.0F, 0.0F};
        key.tolerance = key.softness = 0.0F;
        key.opacity = 1.0F;
        for (const auto destination : std::array{std::array{2558U, 1346U}, std::array{1000U, 700U}}) {
            renderer.resize(destination[0], destination[1]);
            require(renderer.draw_frame(frame, key).succeeded(), "Full NV12 input must scale into the usable destination.");
            for (const auto location : std::array{std::array{0U, 0U}, std::array{destination[0] - 1, 0U},
                     std::array{0U, destination[1] - 1}, std::array{destination[0] - 1, destination[1] - 1}}) {
                require(read_pixel(renderer, location[0], location[1]) == pixel{255, 255, 255, 255},
                        "All four source corner markers must remain visible after unequal-axis scaling.");
            }
            require(read_pixel(renderer, destination[0] / 2, destination[1] / 2) == pixel{0, 0, 0, 0},
                    "Video fitting must preserve transparent black inside the scaled feed.");
        }
        // Exercise cleanup after bilinear rescaling at an actual black/white
        // boundary rather than only uniform input samples.
        renderer.resize(100, 70);
        key.tolerance = 0.12F;
        key.softness = 0.08F;
        key.recover_black_edges = true;
        require(renderer.draw_frame(frame, key).succeeded(), "Scaled marker edges must support cleanup.");
        for (std::uint32_t x = 15; x < 23; ++x) {
            const auto edge = read_pixel(renderer, x, 5);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                require(edge[channel] <= edge[3] && edge[channel] + 255 - edge[3] >= 253,
                        "Rescaling bright edges must not add a dark fringe over white.");
            }
        }
        texture = make_nv12(renderer, 0, 128, 128, false, false, true);
        frame.surface = std::make_shared<fuser::windows::d3d11_surface>(texture, 0);
        key.tolerance = key.softness = 0.0F;
        key.recover_black_edges = false;
        key.crisp_scaling = true;
        require(renderer.draw_frame(frame, key).succeeded(), "Crisp HUD scaling must accept NV12.");
        for (std::uint32_t x = 0; x < 100; ++x) {
            const auto stroke = read_pixel(renderer, x, 35);
            require(stroke == pixel{0, 0, 0, 0} || stroke == pixel{255, 255, 255, 255},
                    "Crisp scaling must not add grey to one-pixel black/white HUD strokes.");
        }
        key.crisp_scaling = false;
        require(renderer.draw_frame(frame, key).succeeded(), "Smooth scaling remains available.");
        unsigned int filtered_pixels{};
        for (std::uint32_t x = 0; x < 100; ++x) {
            const auto stroke = read_pixel(renderer, x, 35);
            if (stroke[0] > 0 && stroke[0] < 255) { ++filtered_pixels; }
        }
        require(filtered_pixels > 40, "The fixture must expose the additional smoothing at unequal scaling.");
        check_array_resource_reuse();
        std::cout << "GPU key/noise/bright-edge alpha, crisp/smooth HUD scaling, NV12 array view/query reuse, conversion/cropping, resource retirement, clear and resize checks passed.\n";
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << winrt::to_string(error.message()) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
