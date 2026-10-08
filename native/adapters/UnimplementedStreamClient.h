#pragma once

#include <fuser/stream_interfaces.h>

namespace fuser {

// An explicit integration seam, not a mock that reports a successful connection.
class unimplemented_stream_client final : public stream_client {
public:
    operation_result pair(const host_endpoint&, const std::string&) override {
        return pending();
    }
    operation_result list_applications(const host_endpoint&,
                                       std::vector<host_application>& applications) override {
        applications.clear();
        return pending();
    }
    operation_result
    connect(const overlay_configuration&, const host_application&, stream_callbacks) override {
        return pending();
    }
    void stop() noexcept override {}

private:
    static operation_result pending() {
        return {operation_code::unavailable,
                "Moonlight pairing and streaming adapter has not been integrated."};
    }
};

} // namespace fuser
