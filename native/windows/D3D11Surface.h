#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <fuser/video_frame.h>

namespace fuser::windows {

class d3d11_surface final : public gpu_surface {
public:
    d3d11_surface(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                  std::uint32_t array_slice,
                  std::shared_ptr<const decoder_frame_lease> pool_lease = {})
        : texture_{std::move(texture)}, array_slice_{array_slice},
          pool_lease_{std::move(pool_lease)} {}

    [[nodiscard]] ID3D11Texture2D* texture() const noexcept {
        return texture_.Get();
    }
    [[nodiscard]] std::uint32_t array_slice() const noexcept {
        return array_slice_;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    std::uint32_t array_slice_{};
    // Mandatory for decoder-pool textures; optional for independent GPU copies.
    std::shared_ptr<const decoder_frame_lease> pool_lease_;
};

} // namespace fuser::windows
