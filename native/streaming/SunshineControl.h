#pragma once
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <fuser/stream_interfaces.h>

namespace fuser::streaming {

struct client_identity {
    std::string unique_id;
    std::string certificate_pem;
    std::string private_key_pem;
};

// The widget implementation protects the private key in app-local storage.
// Callers must never substitute another Moonlight client's credentials.
class credential_store {
public:
    virtual ~credential_store() = default;
    [[nodiscard]] virtual std::optional<client_identity> load_identity() = 0;
    virtual void save_identity(const client_identity&) = 0;
    [[nodiscard]] virtual std::string load_server_certificate(const host_endpoint&) = 0;
    virtual void save_server_certificate(const host_endpoint&, const std::string&) = 0;
};

struct host_information {
    std::string name;
    std::string unique_id;
    std::string app_version;
    std::string gfe_version;
    std::string state;
    std::uint16_t https_port{47984};
    std::uint32_t codec_support{};
    std::uint32_t current_application{};
    bool paired{};
};

struct launch_information {
    host_information host;
    std::string rtsp_url;
};

// Synchronous control operations run on a background thread. Cancellation is
// polled by curl, including while waiting for the user to enter Sunshine's PIN.
class sunshine_control final {
public:
    explicit sunshine_control(std::shared_ptr<credential_store> credentials);
    [[nodiscard]] operation_result inspect(const host_endpoint&, host_information&);
    [[nodiscard]] operation_result pair(const host_endpoint&, const std::string& pin);
    [[nodiscard]] operation_result list_applications(const host_endpoint&, std::vector<host_application>&);
    [[nodiscard]] operation_result start_stream(const overlay_configuration&, const host_application&,
        const std::array<unsigned char, 16>& input_key, const std::array<unsigned char, 16>& input_iv,
        launch_information&);
    void cancel() noexcept { cancelled_.store(true); }
    // Call before queuing a new explicit user action, never from a worker.
    void prepare_action() noexcept { cancelled_.store(false); }
private:
    std::shared_ptr<credential_store> credentials_;
    std::atomic<bool> cancelled_{};
};
}
