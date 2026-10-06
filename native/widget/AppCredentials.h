#pragma once
#include "../streaming/SunshineControl.h"
#include <filesystem>
#include <mutex>

namespace fuser::widget {
// All methods run on an MTA worker. Only this app's own identity is loaded.
class app_credentials final : public streaming::credential_store {
public:
    explicit app_credentials(std::filesystem::path folder) : folder_{std::filesystem::absolute(folder).make_preferred()} {}
    std::optional<streaming::client_identity> load_identity() override;
    void save_identity(const streaming::client_identity&) override;
    std::string load_server_certificate(const host_endpoint&) override;
    void save_server_certificate(const host_endpoint&, const std::string&) override;
private:
    std::string read(const std::wstring& name);
    void write(const std::wstring& name, const std::string& text);
    static std::wstring server_name(const host_endpoint&);
    std::filesystem::path folder_;
    std::mutex mutex_;
};
}
