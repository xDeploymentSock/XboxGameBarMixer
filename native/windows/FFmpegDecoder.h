#pragma once

#include <memory>
#include <mutex>
#include <d3d11.h>
#include <wrl/client.h>
#include <fuser/stream_interfaces.h>

namespace fuser::windows {

// Hardware-only SDR decoding. Call stop after the transport has joined.
// Output surfaces retain FFmpeg frame-pool leases independently of this owner.
class ffmpeg_decoder final : public video_decoder {
public:
    ffmpeg_decoder(Microsoft::WRL::ComPtr<ID3D11Device> device,
                   Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                   std::shared_ptr<std::recursive_mutex> context_lock);
    ~ffmpeg_decoder() override;
    [[nodiscard]] operation_result initialize(const stream_profile& profile,
                                      std::function<void(decoded_frame)> on_frame) override;
    [[nodiscard]] operation_result submit(encoded_frame frame) override;
    [[nodiscard]] operation_result flush(); // Explicit end-of-input; never called by stop.
    void stop() noexcept override;

private:
    struct implementation;
    std::unique_ptr<implementation> state_;
};
}
