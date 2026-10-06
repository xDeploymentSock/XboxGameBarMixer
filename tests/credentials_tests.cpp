#include "../native/widget/AppCredentials.h"
#include <winrt/Windows.Foundation.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        if (argc != 2) { throw std::runtime_error{"Expected a test output directory."}; }
        const auto folder = std::filesystem::path{argv[1]} / std::to_string(GetCurrentProcessId());
        std::filesystem::create_directories(folder);
        fuser::widget::app_credentials store{folder};
        if (store.load_identity()) { throw std::runtime_error{"Fresh store had an identity."}; }
        const fuser::streaming::client_identity identity{"0123456789abcdef", "TEST-CERTIFICATE", "PRIVATE-TEST-MARKER-ONLY"};
        std::cout << "Saving owned test identity.\n";
        store.save_identity(identity);
        std::cout << "Loading protected identity.\n";
        const auto restored = store.load_identity();
        if (!restored || restored->unique_id != identity.unique_id || restored->private_key_pem != identity.private_key_pem ||
            restored->certificate_pem != identity.certificate_pem) { throw std::runtime_error{"Protected identity round-trip failed."}; }
        const fuser::host_endpoint host{"127.0.0.1"};
        store.save_server_certificate(host, "SERVER-CERTIFICATE");
        if (store.load_server_certificate(host) != "SERVER-CERTIFICATE") { throw std::runtime_error{"Protected server pin round-trip failed."}; }
        if (!store.load_server_certificate({"127.0.0.2"}).empty()) { throw std::runtime_error{"A different host reused the server pin."}; }
        for (const auto& file : std::filesystem::directory_iterator(folder)) {
            std::ifstream input{file.path(), std::ios::binary};
            const std::string contents{std::istreambuf_iterator<char>{input}, {}};
            if (contents.find(identity.private_key_pem) != std::string::npos || contents.find(identity.certificate_pem) != std::string::npos) {
                throw std::runtime_error{"Credentials were stored in plaintext."};
            }
        }
        std::cout << "LOCAL=user protected identity and host pin round-trips passed.\n";
        winrt::uninit_apartment();
        return 0;
    } catch (const winrt::hresult_error& error) { std::cerr << winrt::to_string(error.message()) << '\n'; return 1; }
      catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
