#pragma once

#include <functional>
#include <string>
#include <vector>

#include <fuser/configuration.h>
#include <fuser/video_frame.h>

namespace fuser {

enum class operation_code {
    success,
    invalid_configuration,
    not_paired,
    unavailable,
    unsupported_format,
    transport_error,
    decoder_error,
    cancelled
};

struct operation_result {
    operation_code code{operation_code::success};
    std::string detail;
    [[nodiscard]] bool succeeded() const noexcept {
        return code == operation_code::success;
    }
};

struct host_application {
    std::string id;
    std::string name;
};

struct stream_callbacks {
    std::function<void(encoded_frame)> on_encoded_frame;
    std::function<void(operation_result)> on_terminated;
};

class stream_client {
public:
    virtual ~stream_client() = default;
    stream_client(const stream_client&) = delete;
    stream_client& operator=(const stream_client&) = delete;
    stream_client(stream_client&&) = delete;
    stream_client& operator=(stream_client&&) = delete;
    [[nodiscard]] virtual operation_result pair(const host_endpoint& host,
                                                const std::string& pin) = 0;
    [[nodiscard]] virtual operation_result
    list_applications(const host_endpoint& host, std::vector<host_application>& applications) = 0;
    // Pairing/launch and stream setup are distinct. No call is made at app startup.
    [[nodiscard]] virtual operation_result connect(const overlay_configuration& configuration,
                                                   const host_application& application,
                                                   stream_callbacks callbacks) = 0;
    // Stop is idempotent; it joins workers and guarantees no callbacks after return.
    // Call from the owner thread, never from this client's callback thread.
    virtual void stop() noexcept = 0;

protected:
    stream_client() = default;
};

class video_decoder {
public:
    virtual ~video_decoder() = default;
    video_decoder(const video_decoder&) = delete;
    video_decoder& operator=(const video_decoder&) = delete;
    video_decoder(video_decoder&&) = delete;
    video_decoder& operator=(video_decoder&&) = delete;
    [[nodiscard]] virtual operation_result
    initialize(const stream_profile& profile, std::function<void(decoded_frame)> on_frame) = 0;
    // All required reference frames reach the decoder. Backpressure and recovery
    // must be explicit; do not silently drop arbitrary compressed frames.
    [[nodiscard]] virtual operation_result submit(encoded_frame frame) = 0;
    virtual void stop() noexcept = 0;

protected:
    video_decoder() = default;
};

struct pipeline_statistics {
    std::uint64_t received_frames{};
    std::uint64_t decoded_frames{};
    std::uint64_t replaced_display_frames{};
    std::uint64_t present_calls{};
    // Displayed FPS and end-to-end latency are intentionally not inferred here.
};

} // namespace fuser
