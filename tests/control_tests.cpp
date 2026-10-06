#include <SunshineControl.h>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace fuser;
using namespace fuser::streaming;
namespace {
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error{message}; } }
struct memory_credentials final : credential_store {
    std::optional<client_identity> identity;
    std::string server;
    unsigned int identity_writes{}, server_writes{};
    std::optional<client_identity> load_identity() override { return identity; }
    void save_identity(const client_identity& value) override { identity = value; ++identity_writes; }
    std::string load_server_certificate(const host_endpoint&) override { return server; }
    void save_server_certificate(const host_endpoint&, const std::string& value) override { server = value; ++server_writes; }
};
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected host/port and scenario.");
        const std::string mode{argv[2]};
        auto credentials = std::make_shared<memory_credentials>();
        sunshine_control control{credentials};
        if (mode == "inspect") {
            host_information info;
            const auto result = control.inspect({argv[1]}, info);
            require(result.succeeded(), result.detail.c_str());
            require(!credentials->identity && credentials->identity_writes == 0, "Discovery created credentials.");
            std::cout << "Read-only discovery: " << info.name << " | " << info.state << " | active app " << info.current_application << '\n';
            return 0;
        }
        const host_endpoint host{"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1]))};
        require(control.pair(host, "12x4").code == operation_code::invalid_configuration, "Invalid PIN was accepted.");
        require(credentials->identity_writes == 0, "Invalid PIN created an identity.");
        control.cancel();
        require(control.pair(host, "1234").code == operation_code::cancelled, "Pre-cancelled pairing continued.");
        require(credentials->identity_writes == 0, "Cancelled pairing created an identity.");
        control.prepare_action();
        std::vector<host_application> apps;
        require(control.list_applications(host, apps).code == operation_code::not_paired, "Unpaired apps were accepted.");
        host_information info;
        auto result = control.inspect(host, info);
        if (mode == "dtd") {
            require(!result.succeeded(), "XML DOCTYPE was accepted.");
            require(credentials->identity_writes == 0, "Malformed discovery created credentials.");
        } else {
            require(result.succeeded(), result.detail.c_str());
            const auto expected_current = mode == "desktop-launch" || mode == "stale-desktop" ? 0U :
                mode == "steam-active" ? 1093255277U : 881448767U;
            require(info.current_application == expected_current && !info.paired, "Discovery metadata was wrong.");
            result = control.pair(host, mode == "wrong-pin" ? "9876" : "1234");
            if (mode == "wrong-pin" || mode == "forged-signature") {
                require(!result.succeeded(), "Invalid pairing proof was accepted.");
                require(credentials->server_writes == 0, "Unverified server certificate was persisted.");
            } else {
                require(result.succeeded(), result.detail.c_str());
                require(credentials->identity_writes == 1 && credentials->server_writes == 1, "Pairing did not persist exactly one own identity and verified pin.");
                require(credentials->identity->unique_id.size() == 16, "Client ID length was invalid.");
                result = control.inspect(host, info);
                if (mode == "rotated-certificate") {
                    require(!result.succeeded(), "Rotated TLS public key bypassed certificate pinning.");
                } else {
                    require(result.succeeded() && info.paired, "Authenticated pairing status failed.");
                    result = control.list_applications(host, apps);
                    require(result.succeeded() && apps.size() == 2, "Authenticated app list failed.");
                    overlay_configuration config;
                    config.host = host;
                    std::array<unsigned char, 16> key{}, iv{};
                    launch_information launch;
                    if (mode == "success") {
                        result = control.start_stream(config, {"123", "Other app"}, key, iv, launch);
                        require(result.code == operation_code::unavailable, "A different active application was replaced.");
                        result = control.start_stream(config, {"881448767", "HUD"}, key, iv, launch);
                        require(result.succeeded() && !launch.rtsp_url.empty(), "Active app could not be resumed.");
                    } else {
                        result = control.start_stream(config, {"881448767", "Desktop"}, key, iv, launch);
                        if (mode == "stale-desktop") {
                            require(!result.succeeded() && result.detail.find("Refresh apps") != std::string::npos,
                                "Stale Desktop ID was allowed to launch Steam.");
                        } else if (mode == "steam-active" || mode == "resume-race") {
                            require(result.code == operation_code::unavailable && launch.rtsp_url.empty(),
                                "Desktop connected to another application's session.");
                            require(result.detail.find("Steam Big Picture") != std::string::npos,
                                "Conflict status did not identify the active Steam application.");
                        } else {
                            require(result.succeeded() && !launch.rtsp_url.empty() &&
                                launch.host.current_application == 881448767, "Desktop launch/resume was not verified.");
                        }
                    }
                }
            }
        }
        std::cout << mode << ": pairing/control assertions passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
