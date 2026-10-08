#include "AppCredentials.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <fstream>
#include <stdexcept>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Security.Cryptography.DataProtection.h>
#include <winrt/Windows.Storage.Streams.h>

namespace fuser::widget {
using namespace winrt;
using namespace Windows::Security::Cryptography;
using namespace Windows::Security::Cryptography::DataProtection;
using namespace Windows::Storage;

std::string app_credentials::read(const std::wstring& name) {
    const auto path = folder_ / name;
    if (!std::filesystem::exists(path)) {
        return {};
    }
    if (std::filesystem::file_size(path) > 65536) {
        throw std::runtime_error{"Stored credentials exceed their size limit."};
    }
    const auto file = StorageFile::GetFileFromPathAsync(path.wstring()).get();
    const auto protected_buffer = FileIO::ReadBufferAsync(file).get();
    const auto clear = DataProtectionProvider{}.UnprotectAsync(protected_buffer).get();
    return to_string(CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8, clear));
}
void app_credentials::write(const std::wstring& name, const std::string& text) {
    if (text.empty() || text.size() > 32768) {
        throw std::runtime_error{"Invalid credential size."};
    }
    const auto clear =
        CryptographicBuffer::ConvertStringToBinary(to_hstring(text), BinaryStringEncoding::Utf8);
    const auto protected_buffer = DataProtectionProvider{L"LOCAL=user"}.ProtectAsync(clear).get();
    const auto folder = StorageFolder::GetFolderFromPathAsync(folder_.wstring()).get();
    const auto file =
        folder.CreateFileAsync(name + L".pending", CreationCollisionOption::ReplaceExisting).get();
    FileIO::WriteBufferAsync(file, protected_buffer).get();
    file.RenameAsync(name, NameCollisionOption::ReplaceExisting).get();
}
std::wstring app_credentials::server_name(const host_endpoint& host) {
    const auto input = CryptographicBuffer::ConvertStringToBinary(
        to_hstring(host.address + ":" + std::to_string(host.base_port)),
        BinaryStringEncoding::Utf8);
    const auto algorithm =
        Windows::Security::Cryptography::Core::HashAlgorithmProvider::OpenAlgorithm(L"SHA256");
    return L"sunshine-" +
           std::wstring{CryptographicBuffer::EncodeToHexString(algorithm.HashData(input))} +
           L".protected";
}
std::optional<streaming::client_identity> app_credentials::load_identity() {
    const std::lock_guard guard{mutex_};
    const auto text = read(L"client-identity.protected");
    if (text.empty()) {
        return std::nullopt;
    }
    const auto json = Windows::Data::Json::JsonObject::Parse(to_hstring(text));
    streaming::client_identity result{to_string(json.GetNamedString(L"id")),
                                      to_string(json.GetNamedString(L"certificate")),
                                      to_string(json.GetNamedString(L"key"))};
    if (result.unique_id.size() != 16 || result.certificate_pem.empty() ||
        result.private_key_pem.empty()) {
        throw std::runtime_error{"Stored client identity is invalid."};
    }
    return result;
}
void app_credentials::save_identity(const streaming::client_identity& identity) {
    const std::lock_guard guard{mutex_};
    Windows::Data::Json::JsonObject json;
    json.Insert(L"id",
                Windows::Data::Json::JsonValue::CreateStringValue(to_hstring(identity.unique_id)));
    json.Insert(
        L"certificate",
        Windows::Data::Json::JsonValue::CreateStringValue(to_hstring(identity.certificate_pem)));
    json.Insert(
        L"key",
        Windows::Data::Json::JsonValue::CreateStringValue(to_hstring(identity.private_key_pem)));
    write(L"client-identity.protected", to_string(json.Stringify()));
}
std::string app_credentials::load_server_certificate(const host_endpoint& host) {
    const std::lock_guard guard{mutex_};
    return read(server_name(host));
}
void app_credentials::save_server_certificate(const host_endpoint& host, const std::string& pem) {
    const std::lock_guard guard{mutex_};
    write(server_name(host), pem);
}
}
