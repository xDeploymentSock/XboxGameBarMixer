// GameStream control protocol, following Moonlight's published pairing flow.
#include "SunshineControl.h"
#include <algorithm>
#include <charconv>
#include <climits>
#include <cstring>
#include <map>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <curl/curl.h>
#include <expat.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <Limelight.h>

namespace fuser::streaming {
namespace {
using bytes = std::vector<unsigned char>;
constexpr std::size_t maximum_response = 4 * 1024 * 1024;
struct control_error : std::runtime_error {
    operation_code code;
    control_error(operation_code value, std::string message) : std::runtime_error{std::move(message)}, code{value} {}
};
void check(bool valid, const char* message) {
    if (!valid) { throw control_error{operation_code::transport_error, message}; }
}
template<class Function> operation_result capture(Function&& function) {
    try { function(); return {}; }
    catch (const control_error& error) { return {error.code, error.what()}; }
    catch (const std::exception& error) { return {operation_code::transport_error, error.what()}; }
}
template<class T, auto Free> struct deleter { void operator()(T* value) const noexcept { if (value) { Free(value); } } };
using curl_owner = std::unique_ptr<CURL, deleter<CURL, curl_easy_cleanup>>;
using bio_owner = std::unique_ptr<BIO, deleter<BIO, BIO_free>>;
using cert_owner = std::unique_ptr<X509, deleter<X509, X509_free>>;
using key_owner = std::unique_ptr<EVP_PKEY, deleter<EVP_PKEY, EVP_PKEY_free>>;
using cipher_owner = std::unique_ptr<EVP_CIPHER_CTX, deleter<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>>;
using digest_owner = std::unique_ptr<EVP_MD_CTX, deleter<EVP_MD_CTX, EVP_MD_CTX_free>>;
using parser_owner = std::unique_ptr<XML_ParserStruct, deleter<XML_ParserStruct, XML_ParserFree>>;

bytes random_bytes(std::size_t size) {
    bytes result(size);
    check(RAND_bytes(result.data(), static_cast<int>(size)) == 1, "Secure random generation failed.");
    return result;
}
std::string hex(std::span<const unsigned char> data) {
    constexpr std::string_view digits{"0123456789abcdef"};
    std::string result(data.size() * 2, '0');
    for (std::size_t index = 0; index < data.size(); ++index) {
        result[index * 2] = digits[data[index] >> 4];
        result[index * 2 + 1] = digits[data[index] & 15];
    }
    return result;
}
std::string hex(std::string_view data) {
    return hex(std::span{reinterpret_cast<const unsigned char*>(data.data()), data.size()});
}
bytes unhex(std::string_view data) {
    check(data.size() % 2 == 0 && data.size() <= maximum_response, "Invalid hexadecimal pairing response.");
    const auto digit = [](char character) -> unsigned char {
        if (character >= '0' && character <= '9') { return static_cast<unsigned char>(character - '0'); }
        if (character >= 'a' && character <= 'f') { return static_cast<unsigned char>(character - 'a' + 10); }
        if (character >= 'A' && character <= 'F') { return static_cast<unsigned char>(character - 'A' + 10); }
        throw control_error{operation_code::transport_error, "Invalid hexadecimal pairing response."};
    };
    bytes result(data.size() / 2);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<unsigned char>((digit(data[index * 2]) << 4) | digit(data[index * 2 + 1]));
    }
    return result;
}
bytes concatenate(std::initializer_list<std::span<const unsigned char>> parts) {
    bytes result;
    for (const auto part : parts) { result.insert(result.end(), part.begin(), part.end()); }
    return result;
}
bytes hash(std::span<const unsigned char> data) {
    bytes result(32);
    unsigned int size{};
    check(EVP_Digest(data.data(), data.size(), result.data(), &size, EVP_sha256(), nullptr) == 1 && size == 32,
          "SHA-256 failed.");
    return result;
}
bytes crypt(std::span<const unsigned char> data, std::span<const unsigned char> key, bool encrypt) {
    check(key.size() == 16 && !data.empty() && data.size() % 16 == 0 && data.size() < INT_MAX,
          "Invalid pairing cipher input.");
    cipher_owner context{EVP_CIPHER_CTX_new()};
    check(context != nullptr, "Pairing cipher allocation failed.");
    check(EVP_CipherInit_ex(context.get(), EVP_aes_128_ecb(), nullptr, key.data(), nullptr, encrypt ? 1 : 0) == 1 &&
          EVP_CIPHER_CTX_set_padding(context.get(), 0) == 1, "Pairing cipher initialization failed.");
    bytes result(data.size() + 16);
    int written{}, final{};
    check(EVP_CipherUpdate(context.get(), result.data(), &written, data.data(), static_cast<int>(data.size())) == 1 &&
          EVP_CipherFinal_ex(context.get(), result.data() + written, &final) == 1,
          "Pairing cipher operation failed.");
    result.resize(static_cast<std::size_t>(written + final));
    return result;
}
cert_owner certificate(const std::string& pem) {
    check(!pem.empty() && pem.size() <= 16384, "Invalid certificate size.");
    bio_owner input{BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()))};
    check(input != nullptr, "Certificate buffer allocation failed.");
    cert_owner result{PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr)};
    check(result != nullptr, "Cannot parse certificate.");
    return result;
}
bytes certificate_signature(X509* value) {
    const ASN1_BIT_STRING* signature{};
    X509_get0_signature(&signature, nullptr, value);
    check(signature != nullptr && ASN1_STRING_length(signature) > 0, "Certificate has no signature.");
    const auto* data = ASN1_STRING_get0_data(signature);
    return bytes{data, data + ASN1_STRING_length(signature)};
}
std::string public_key_pin(X509* value) {
    key_owner key{X509_get_pubkey(value)};
    check(key != nullptr, "Server certificate has no public key.");
    const auto size = i2d_PUBKEY(key.get(), nullptr);
    check(size > 0, "Cannot encode server public key.");
    bytes encoded(static_cast<std::size_t>(size));
    auto* position = encoded.data();
    check(i2d_PUBKEY(key.get(), &position) == size, "Cannot encode server public key.");
    const auto digest = hash(encoded);
    std::string base64(45, '\0');
    const auto written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(base64.data()), digest.data(), 32);
    check(written == 44, "Cannot encode server certificate pin.");
    base64.resize(44);
    return "sha256//" + base64;
}
std::string pem_contents(BIO* bio) {
    char* data{};
    const auto size = BIO_get_mem_data(bio, &data);
    check(size > 0 && size <= 16384 && data, "Cannot serialize client credentials.");
    return std::string{data, static_cast<std::size_t>(size)};
}
client_identity create_identity() {
    client_identity identity;
    identity.unique_id = hex(random_bytes(8));
    key_owner key{EVP_RSA_gen(2048)};
    cert_owner cert{X509_new()};
    check(key && cert, "Client identity allocation failed.");
    check(X509_set_version(cert.get(), 2) == 1 && ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1) == 1 &&
          X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60) &&
          X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 365 * 10) &&
          X509_set_pubkey(cert.get(), key.get()) == 1, "Cannot configure client certificate.");
    auto* name = X509_get_subject_name(cert.get());
    constexpr auto common_name = "Software Fuser";
    check(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
          reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0) == 1 &&
          X509_set_issuer_name(cert.get(), name) == 1 && X509_sign(cert.get(), key.get(), EVP_sha256()) > 0,
          "Cannot sign client certificate.");
    bio_owner pem_cert{BIO_new(BIO_s_mem())}, pem_key{BIO_new(BIO_s_mem())};
    check(pem_cert && pem_key && PEM_write_bio_X509(pem_cert.get(), cert.get()) == 1 &&
          PEM_write_bio_PrivateKey(pem_key.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1,
          "Cannot serialize client identity.");
    identity.certificate_pem = pem_contents(pem_cert.get());
    identity.private_key_pem = pem_contents(pem_key.get());
    return identity;
}

struct xml_node {
    std::string name, text;
    std::map<std::string, std::string> attributes;
    std::vector<xml_node> children;
    const xml_node* child(std::string_view requested) const {
        const xml_node* result{};
        for (const auto& value : children) {
            if (value.name == requested) { check(!result, "Duplicate host response field."); result = &value; }
        }
        return result;
    }
    std::string value(std::string_view requested) const {
        const auto* found = child(requested);
        return found ? found->text : std::string{};
    }
};
struct xml_state {
    XML_Parser parser{};
    xml_node root;
    std::vector<xml_node*> stack;
    std::size_t nodes{};
    bool failed{};
    void fail() noexcept { failed = true; XML_StopParser(parser, XML_FALSE); }
};
void XMLCALL element_start(void* user, const XML_Char* name, const XML_Char** attributes) noexcept {
    auto& state = *static_cast<xml_state*>(user);
    try {
        if (state.stack.size() >= 16 || ++state.nodes > 4096) { state.fail(); return; }
        xml_node* node{};
        if (state.stack.empty()) { node = &state.root; }
        else { state.stack.back()->children.emplace_back(); node = &state.stack.back()->children.back(); }
        node->name = name;
        for (std::size_t index = 0; attributes[index]; index += 2) {
            if (index >= 64) { state.fail(); return; }
            node->attributes.emplace(attributes[index], attributes[index + 1]);
        }
        state.stack.push_back(node);
    } catch (...) { state.fail(); }
}
void XMLCALL element_end(void* user, const XML_Char*) noexcept {
    auto& state = *static_cast<xml_state*>(user);
    if (state.stack.empty()) { state.fail(); return; }
    state.stack.pop_back();
}
void XMLCALL element_text(void* user, const XML_Char* text, int size) noexcept {
    auto& state = *static_cast<xml_state*>(user);
    try {
        if (!state.stack.empty()) { state.stack.back()->text.append(text, static_cast<std::size_t>(size)); }
    } catch (...) { state.fail(); }
}
void XMLCALL reject_doctype(void* user, const XML_Char*, const XML_Char*, const XML_Char*, int) noexcept {
    static_cast<xml_state*>(user)->fail();
}
xml_node parse_xml(const std::string& input) {
    check(!input.empty() && input.size() <= maximum_response, "Host XML response size is invalid.");
    parser_owner parser{XML_ParserCreate(nullptr)};
    check(parser != nullptr, "XML parser allocation failed.");
    xml_state state;
    state.parser = parser.get();
    XML_SetUserData(parser.get(), &state);
    XML_SetElementHandler(parser.get(), element_start, element_end);
    XML_SetCharacterDataHandler(parser.get(), element_text);
    XML_SetStartDoctypeDeclHandler(parser.get(), reject_doctype);
    check(XML_Parse(parser.get(), input.data(), static_cast<int>(input.size()), XML_TRUE) != XML_STATUS_ERROR &&
          !state.failed && state.root.name == "root", "Host returned invalid XML.");
    const auto status = state.root.attributes.find("status_code");
    check(status != state.root.attributes.end(), "Host response has no status code.");
    if (status->second != "200") {
        throw control_error{status->second == "401" ? operation_code::not_paired : operation_code::transport_error,
            "Sunshine rejected the control request (status " + status->second + ")."};
    }
    return std::move(state.root);
}
std::uint32_t number(const std::string& input, std::uint32_t fallback = 0) {
    if (input.empty()) { return fallback; }
    std::uint32_t result{};
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), result);
    check(error == std::errc{} && end == input.data() + input.size(), "Invalid numeric host field.");
    return result;
}
host_information read_host(const xml_node& xml) {
    host_information host;
    host.name = xml.value("hostname");
    host.unique_id = xml.value("uniqueid");
    host.app_version = xml.value("appversion");
    host.gfe_version = xml.value("GfeVersion");
    host.state = xml.value("state");
    const auto port = number(xml.value("HttpsPort"), 47984);
    check(port > 0 && port <= 65535, "Invalid Sunshine HTTPS port.");
    host.https_port = static_cast<std::uint16_t>(port);
    host.codec_support = number(xml.value("ServerCodecModeSupport"));
    host.current_application = host.state.ends_with("_SERVER_BUSY") ? number(xml.value("currentgame")) : 0;
    host.paired = xml.value("PairStatus") == "1";
    check(!host.app_version.empty(), "Host omitted its protocol version.");
    return host;
}

struct request_state { std::string response; const std::atomic<bool>* cancelled{}; };
std::size_t receive(char* data, std::size_t size, std::size_t count, void* user) noexcept {
    auto& state = *static_cast<request_state*>(user);
    if (count && size > maximum_response / count) { return 0; }
    const auto length = size * count;
    if (length > maximum_response - state.response.size()) { return 0; }
    try { state.response.append(data, length); return length; }
    catch (...) { return 0; }
}
int progress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept {
    return static_cast<request_state*>(user)->cancelled->load() ? 1 : 0;
}
std::string address(const host_endpoint& host) {
    if (host.address.empty() || host.address.size() > 253 || host.base_port == 0) {
        throw control_error{operation_code::invalid_configuration, "Invalid source address."};
    }
    for (const auto character : host.address) {
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '.' || character == '-' || character == ':')) {
            throw control_error{operation_code::invalid_configuration,
                  "Source address must be an IP address or hostname, without a URL or path."}; }
    }
    return host.address.find(':') != std::string::npos ? "[" + host.address + "]" : host.address;
}
xml_node request(const host_endpoint& host, std::uint16_t https_port, const char* verb,
                 const std::vector<std::pair<std::string, std::string>>& query,
                 const client_identity* identity, const std::string& server_pem,
                 const std::atomic<bool>& cancelled, long timeout = 10000) {
    if (cancelled.load()) { throw control_error{operation_code::cancelled, "Control action cancelled."}; }
    static std::once_flag initialized;
    std::call_once(initialized, [] { check(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK, "HTTP initialization failed."); });
    curl_owner curl{curl_easy_init()};
    check(curl != nullptr, "HTTP request allocation failed.");
    const bool secure = https_port != 0;
    std::string url = (secure ? "https://" : "http://") + address(host) + ":" +
        std::to_string(secure ? https_port : host.base_port) + "/" + verb;
    auto parameters = query;
    parameters.emplace_back("uniqueid", identity ? identity->unique_id : "SoftwareFuserReadOnly");
    parameters.emplace_back("uuid", hex(random_bytes(16)));
    bool first = true;
    for (const auto& [name, value] : parameters) {
        std::unique_ptr<char, deleter<char, curl_free>> encoded{curl_easy_escape(curl.get(), value.c_str(), static_cast<int>(value.size()))};
        check(encoded != nullptr, "URL encoding failed.");
        url += (first ? "?" : "&") + name + "=" + encoded.get();
        first = false;
    }
    request_state state{{}, &cancelled};
    const auto option = [&curl](CURLoption name, auto value) {
        check(curl_easy_setopt(curl.get(), name, value) == CURLE_OK, "HTTP option setup failed.");
    };
    option(CURLOPT_URL, url.c_str());
    option(CURLOPT_PROXY, "");
    option(CURLOPT_NOSIGNAL, 1L);
    option(CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    option(CURLOPT_TIMEOUT_MS, timeout);
    option(CURLOPT_FOLLOWLOCATION, 0L);
    option(CURLOPT_WRITEFUNCTION, receive);
    option(CURLOPT_WRITEDATA, &state);
    option(CURLOPT_NOPROGRESS, 0L);
    option(CURLOPT_XFERINFOFUNCTION, progress);
    option(CURLOPT_XFERINFODATA, &state);
    std::string pin;
    curl_blob cert_blob{}, key_blob{};
    if (secure) {
        if (!identity || server_pem.empty()) { throw control_error{operation_code::not_paired, "Pair this client before using Sunshine HTTPS control."}; }
        const auto cert = certificate(server_pem);
        pin = public_key_pin(cert.get());
        // Sunshine certificates are self-signed. Authenticate the paired
        // public key explicitly, including in the final pairing challenge.
        option(CURLOPT_SSL_VERIFYHOST, 0L);
        option(CURLOPT_SSL_VERIFYPEER, 0L);
        option(CURLOPT_PINNEDPUBLICKEY, pin.c_str());
        option(CURLOPT_SSLCERTTYPE, "PEM");
        option(CURLOPT_SSLKEYTYPE, "PEM");
        cert_blob = {const_cast<char*>(identity->certificate_pem.data()), identity->certificate_pem.size(), CURL_BLOB_COPY};
        key_blob = {const_cast<char*>(identity->private_key_pem.data()), identity->private_key_pem.size(), CURL_BLOB_COPY};
        option(CURLOPT_SSLCERT_BLOB, &cert_blob);
        option(CURLOPT_SSLKEY_BLOB, &key_blob);
    }
    const auto result = curl_easy_perform(curl.get());
    if (result != CURLE_OK) {
        throw control_error{cancelled.load() ? operation_code::cancelled : operation_code::transport_error,
            std::string{"Sunshine control: "} + curl_easy_strerror(result)};
    }
    long status{};
    check(curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status) == CURLE_OK, "Cannot read HTTP status.");
    if (status != 200) { throw control_error{status == 401 ? operation_code::not_paired : operation_code::transport_error,
        "Sunshine HTTP status " + std::to_string(status) + "."}; }
    return parse_xml(state.response);
}
}

sunshine_control::sunshine_control(std::shared_ptr<credential_store> credentials) : credentials_{std::move(credentials)} {
    if (!credentials_) { throw std::invalid_argument{"A private credential store is required."}; }
}

operation_result sunshine_control::inspect(const host_endpoint& host, host_information& output) {
    return capture([&] {
        const auto identity = credentials_->load_identity();
        const auto server_cert = credentials_->load_server_certificate(host);
        auto result = read_host(request(host, 0, "serverinfo", {}, identity ? &*identity : nullptr, {}, cancelled_));
        if (identity && !server_cert.empty()) {
            result = read_host(request(host, result.https_port, "serverinfo", {}, &*identity, server_cert, cancelled_));
        }
        output = std::move(result);
    });
}

operation_result sunshine_control::pair(const host_endpoint& host, const std::string& pin) {
    return capture([&] {
        if (cancelled_.load()) { throw control_error{operation_code::cancelled, "Pairing cancelled."}; }
        (void)address(host);
        if (pin.size() != 4 || !std::all_of(pin.begin(), pin.end(), [](char value) { return value >= '0' && value <= '9'; })) {
            throw control_error{operation_code::invalid_configuration, "Pairing requires a four-digit PIN."};
        }
        auto identity = credentials_->load_identity();
        if (!identity) { identity = create_identity(); credentials_->save_identity(*identity); }
        const auto info = read_host(request(host, 0, "serverinfo", {}, &*identity, {}, cancelled_));
        const auto existing_certificate = credentials_->load_server_certificate(host);
        if (!existing_certificate.empty()) {
            const auto authenticated = read_host(request(host, info.https_port, "serverinfo", {},
                &*identity, existing_certificate, cancelled_));
            if (authenticated.paired) { return; }
        }
        check(number(info.app_version.substr(0, info.app_version.find('.'))) >= 7,
              "This client supports generation-7 Sunshine pairing.");
        const auto salt = random_bytes(16);
        bytes salted_pin{salt};
        salted_pin.insert(salted_pin.end(), pin.begin(), pin.end());
        auto key = hash(salted_pin);
        key.resize(16);
        auto response = request(host, 0, "pair", {{"devicename", "Software Fuser"}, {"updateState", "1"},
            {"phrase", "getservercert"}, {"salt", hex(salt)}, {"clientcert", hex(identity->certificate_pem)}},
            &*identity, {}, cancelled_, 120000);
        check(response.value("paired") == "1", "Sunshine rejected the initial PIN pairing request.");
        const auto server_bytes = unhex(response.value("plaincert"));
        const std::string server_pem{server_bytes.begin(), server_bytes.end()};
        const auto server_certificate = certificate(server_pem);
        const auto client_certificate = certificate(identity->certificate_pem);
        const auto challenge = random_bytes(16);
        response = request(host, 0, "pair", {{"devicename", "Software Fuser"}, {"updateState", "1"},
            {"clientchallenge", hex(crypt(challenge, key, true))}}, &*identity, {}, cancelled_);
        check(response.value("paired") == "1", "Sunshine rejected the client challenge.");
        const auto challenge_response = crypt(unhex(response.value("challengeresponse")), key, false);
        check(challenge_response.size() >= 48, "Sunshine returned a short pairing challenge.");
        const bytes server_hash{challenge_response.begin(), challenge_response.begin() + 32};
        const auto client_secret = random_bytes(16);
        const auto client_signature = certificate_signature(client_certificate.get());
        const auto response_hash = hash(concatenate({std::span{challenge_response}.subspan(32, 16), client_signature, client_secret}));
        response = request(host, 0, "pair", {{"devicename", "Software Fuser"}, {"updateState", "1"},
            {"serverchallengeresp", hex(crypt(response_hash, key, true))}}, &*identity, {}, cancelled_);
        check(response.value("paired") == "1", "Sunshine rejected the challenge response.");
        const auto pairing_secret = unhex(response.value("pairingsecret"));
        check(pairing_secret.size() > 16, "Sunshine returned an invalid pairing secret.");
        key_owner server_key{X509_get_pubkey(server_certificate.get())};
        digest_owner verify{EVP_MD_CTX_new()};
        check(server_key && verify && EVP_DigestVerifyInit(verify.get(), nullptr, EVP_sha256(), nullptr, server_key.get()) == 1 &&
              EVP_DigestVerifyUpdate(verify.get(), pairing_secret.data(), 16) == 1 &&
              EVP_DigestVerifyFinal(verify.get(), pairing_secret.data() + 16, pairing_secret.size() - 16) == 1,
              "Sunshine pairing signature verification failed.");
        const auto server_signature = certificate_signature(server_certificate.get());
        const auto expected = hash(concatenate({challenge, server_signature, std::span{pairing_secret}.first(16)}));
        check(CRYPTO_memcmp(expected.data(), server_hash.data(), 32) == 0, "The Sunshine PIN did not match.");
        bio_owner private_input{BIO_new_mem_buf(identity->private_key_pem.data(), static_cast<int>(identity->private_key_pem.size()))};
        check(private_input != nullptr, "Cannot load client signing key.");
        key_owner private_key{PEM_read_bio_PrivateKey(private_input.get(), nullptr, nullptr, nullptr)};
        digest_owner signer{EVP_MD_CTX_new()};
        check(private_key && signer && EVP_DigestSignInit(signer.get(), nullptr, EVP_sha256(), nullptr, private_key.get()) == 1 &&
              EVP_DigestSignUpdate(signer.get(), client_secret.data(), client_secret.size()) == 1, "Cannot sign client pairing secret.");
        std::size_t signature_size{};
        check(EVP_DigestSignFinal(signer.get(), nullptr, &signature_size) == 1 && signature_size <= 16384,
              "Cannot size client pairing signature.");
        bytes signature(signature_size);
        check(EVP_DigestSignFinal(signer.get(), signature.data(), &signature_size) == 1, "Cannot sign client pairing secret.");
        signature.resize(signature_size);
        response = request(host, 0, "pair", {{"devicename", "Software Fuser"}, {"updateState", "1"},
            {"clientpairingsecret", hex(concatenate({client_secret, signature}))}}, &*identity, {}, cancelled_);
        check(response.value("paired") == "1", "Sunshine rejected the signed client secret.");
        response = request(host, info.https_port, "pair", {{"devicename", "Software Fuser"}, {"updateState", "1"},
            {"phrase", "pairchallenge"}}, &*identity, server_pem, cancelled_);
        check(response.value("paired") == "1", "Sunshine rejected the authenticated pairing challenge.");
        credentials_->save_server_certificate(host, server_pem);
    });
}

operation_result sunshine_control::list_applications(const host_endpoint& host, std::vector<host_application>& output) {
    return capture([&] {
        const auto identity = credentials_->load_identity();
        const auto server_cert = credentials_->load_server_certificate(host);
        if (!identity || server_cert.empty()) { throw control_error{operation_code::not_paired, "Pair this client first."}; }
        const auto info = read_host(request(host, 0, "serverinfo", {}, &*identity, {}, cancelled_));
        const auto list = request(host, info.https_port, "applist", {}, &*identity, server_cert, cancelled_);
        std::vector<host_application> applications;
        for (const auto& app : list.children) {
            if (app.name == "App") {
                const auto id = number(app.value("ID"));
                const auto name = app.value("AppTitle");
                check(id != 0 && !name.empty(), "Sunshine returned an invalid application.");
                applications.push_back({std::to_string(id), name});
            }
        }
        output = std::move(applications);
    });
}

operation_result sunshine_control::start_stream(const overlay_configuration& config, const host_application& application,
    const std::array<unsigned char, 16>& input_key, const std::array<unsigned char, 16>& input_iv, launch_information& output) {
    return capture([&] {
        const auto problems = validate(config, true);
        if (!problems.empty()) { throw control_error{operation_code::invalid_configuration, problems.front().message}; }
        const auto app_id = number(application.id);
        check(app_id > 0 && app_id <= INT_MAX, "Select a valid Sunshine application.");
        const auto identity = credentials_->load_identity();
        const auto server_cert = credentials_->load_server_certificate(config.host);
        if (!identity || server_cert.empty()) { throw control_error{operation_code::not_paired, "Pair this client first."}; }
        auto info = read_host(request(config.host, 0, "serverinfo", {}, &*identity, {}, cancelled_));
        info = read_host(request(config.host, info.https_port, "serverinfo", {}, &*identity, server_cert, cancelled_));
        if (info.current_application && info.current_application != app_id) {
            throw control_error{operation_code::unavailable, "Another Sunshine application is active. Select that application to resume it, or stop it on the source PC first."};
        }
        const auto key_id = (static_cast<std::uint32_t>(input_iv[0]) << 24) | (static_cast<std::uint32_t>(input_iv[1]) << 16) |
                            (static_cast<std::uint32_t>(input_iv[2]) << 8) | input_iv[3];
        const std::vector<std::pair<std::string, std::string>> query{
            {"appid", std::to_string(app_id)}, {"mode", std::to_string(config.stream.width) + "x" +
                std::to_string(config.stream.height) + "x" + std::to_string(config.stream.frames_per_second)},
            {"additionalStates", "1"}, {"sops", "0"}, {"rikey", hex(input_key)}, {"rikeyid", std::to_string(key_id)},
            {"localAudioPlayMode", "1"}, {"surroundAudioInfo", "196610"}, {"remoteControllersBitmap", "0"}, {"gcmap", "0"}};
        // Extended Sunshine negotiation flags are already URL query text from
        // Moonlight; request() encodes values, so add each named flag separately.
        auto parameters = query;
        std::string_view extensions{LiGetLaunchUrlQueryParameters()};
        while (!extensions.empty()) {
            if (extensions.front() == '&') { extensions.remove_prefix(1); }
            const auto separator = extensions.find('&');
            const auto field = extensions.substr(0, separator);
            const auto equal = field.find('=');
            if (equal != std::string_view::npos) { parameters.emplace_back(field.substr(0, equal), field.substr(equal + 1)); }
            if (separator == std::string_view::npos) { break; }
            extensions.remove_prefix(separator);
        }
        const auto response = request(config.host, info.https_port, info.current_application ? "resume" : "launch",
                                      parameters, &*identity, server_cert, cancelled_);
        check(number(response.value(info.current_application ? "resume" : "gamesession")) != 0,
              "Sunshine declined stream startup.");
        const auto rtsp = response.value("sessionUrl0");
        check(!rtsp.empty() && rtsp.size() <= 2048, "Sunshine omitted the RTSP session URL.");
        output = {std::move(info), rtsp};
    });
}
}
