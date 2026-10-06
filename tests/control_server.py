"""Loopback-only Sunshine protocol fixture; never contacts a real source PC."""
import datetime
import hashlib
import http.server
import os
import pathlib
import ssl
import subprocess
import sys
import tempfile
import threading
import urllib.parse
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.x509.oid import NameOID


def certificate():
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Owned Sunshine fixture")])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
            .public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(minutes=1))
            .not_valid_after(now + datetime.timedelta(days=1)).sign(key, hashes.SHA256()))
    return key, cert


def aes(data, key, encrypt):
    cipher = Cipher(algorithms.AES(key), modes.ECB())
    operation = cipher.encryptor() if encrypt else cipher.decryptor()
    return operation.update(data) + operation.finalize()


class Fixture:
    def __init__(self, mode, folder):
        self.mode = mode
        self.folder = folder
        self.key, self.cert = certificate()
        self.pairs = {}
        self.paired = set()
        self.resume_count = 0
        self.errors = []
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.verify_mode = ssl.CERT_REQUIRED
        self.install_certificate(self.key, self.cert)

    def install_certificate(self, key, cert):
        cert_path = self.folder / "server.pem"
        key_path = self.folder / "server-key.pem"
        cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        key_path.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                                             serialization.PrivateFormat.PKCS8,
                                             serialization.NoEncryption()))
        self.context.load_cert_chain(cert_path, key_path)

    def respond(self, secure, path, query, peer):
        client = query.get("uniqueid", "")
        if path == "/serverinfo":
            if self.mode == "dtd":
                return '<!DOCTYPE root [<!ENTITY value "untrusted">]><root status_code="200"><hostname>&value;</hostname></root>'
            return (f'<root status_code="200"><hostname>OwnedFixture</hostname><uniqueid>fixture</uniqueid>'
                    f'<appversion>7.1.431.-1</appversion><GfeVersion>3.23.0.74</GfeVersion>'
                    f'<HttpsPort>{self.https.server_port}</HttpsPort><ServerCodecModeSupport>257</ServerCodecModeSupport>'
                    f'<state>SUNSHINE_SERVER_BUSY</state><currentgame>881448767</currentgame>'
                    f'<PairStatus>{int(client in self.paired)}</PairStatus></root>')
        if path == "/pair":
            if query.get("phrase") == "getservercert":
                client_cert = x509.load_pem_x509_certificate(bytes.fromhex(query["clientcert"]))
                self.context.load_verify_locations(cadata=client_cert.public_bytes(serialization.Encoding.PEM).decode())
                self.pairs[client] = {
                    "certificate": client_cert,
                    "key": hashlib.sha256(bytes.fromhex(query["salt"]) + b"1234").digest()[:16],
                    "secret": os.urandom(16), "challenge": os.urandom(16)}
                pem = self.cert.public_bytes(serialization.Encoding.PEM).hex()
                return f'<root status_code="200"><paired>1</paired><plaincert>{pem}</plaincert></root>'
            pair = self.pairs[client]
            if "clientchallenge" in query:
                nonce = aes(bytes.fromhex(query["clientchallenge"]), pair["key"], False)
                proof = hashlib.sha256(nonce + self.cert.signature + pair["secret"]).digest()
                response = aes(proof + pair["challenge"], pair["key"], True).hex()
                return f'<root status_code="200"><paired>1</paired><challengeresponse>{response}</challengeresponse></root>'
            if "serverchallengeresp" in query:
                pair["response"] = aes(bytes.fromhex(query["serverchallengeresp"]), pair["key"], False)
                signature = self.key.sign(pair["secret"], padding.PKCS1v15(), hashes.SHA256())
                if self.mode == "forged-signature":
                    signature = signature[:-1] + bytes([signature[-1] ^ 1])
                secret = (pair["secret"] + signature).hex()
                return f'<root status_code="200"><paired>1</paired><pairingsecret>{secret}</pairingsecret></root>'
            if "clientpairingsecret" in query:
                proof = bytes.fromhex(query["clientpairingsecret"])
                pair["certificate"].public_key().verify(proof[16:], proof[:16], padding.PKCS1v15(), hashes.SHA256())
                expected = hashlib.sha256(pair["challenge"] + pair["certificate"].signature + proof[:16]).digest()
                assert expected == pair["response"], "Client challenge proof was invalid"
                self.paired.add(client)
                return '<root status_code="200"><paired>1</paired></root>'
            if query.get("phrase") == "pairchallenge":
                assert secure and peer == pair["certificate"].public_bytes(serialization.Encoding.DER), "Client TLS identity mismatch"
                return '<root status_code="200"><paired>1</paired></root>'
        assert secure and client in self.paired, "Unauthenticated control action"
        if path == "/applist":
            return ('<root status_code="200"><App><ID>881448767</ID><AppTitle>HUD</AppTitle></App>'
                    '<App><ID>123</ID><AppTitle>Other app</AppTitle></App></root>')
        assert path == "/resume", "Client launched or terminated the active source application"
        assert query["appid"] == "881448767" and query["mode"] == "2560x1440x240"
        assert query["sops"] == "0" and query["gcmap"] == "0" and query["remoteControllersBitmap"] == "0"
        assert query["localAudioPlayMode"] == "1" and query["corever"] == "1"
        self.resume_count += 1
        return '<root status_code="200"><resume>1</resume><sessionUrl0>rtsp://127.0.0.1:48010/session</sessionUrl0></root>'


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        self.fixture.errors.append("Fixture request handler failed")


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # Query strings contain transient pairing secrets; never print them.

    def do_GET(self):
        parsed = urllib.parse.urlsplit(self.path)
        query = {key: values[0] for key, values in urllib.parse.parse_qs(parsed.query).items()}
        secure = self.server.secure
        peer = self.connection.getpeercert(binary_form=True) if secure else None
        try:
            response = self.server.fixture.respond(secure, parsed.path, query, peer).encode()
            if secure and query.get("phrase") == "pairchallenge" and self.server.fixture.mode == "rotated-certificate":
                # The current TLS handshake used the original certificate. Rotate
                # future handshakes before returning, avoiding a fixture race.
                key, cert = certificate()
                self.server.fixture.install_certificate(key, cert)
            self.send_response(200)
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.wfile.write(response)
            self.wfile.flush()
        except Exception as error:
            self.server.fixture.errors.append(type(error).__name__)  # Exclude request/secret contents.
            self.send_error(500)


def main():
    executable, mode = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="fuser-owned-control-") as directory:
        fixture = Fixture(mode, pathlib.Path(directory))
        http = Server(("127.0.0.1", 0), Handler)
        https = Server(("127.0.0.1", 0), Handler)
        fixture.https = https
        for server, secure in ((http, False), (https, True)):
            server.fixture, server.secure = fixture, secure
        https.socket = fixture.context.wrap_socket(https.socket, server_side=True)
        threads = [threading.Thread(target=server.serve_forever, daemon=True) for server in (http, https)]
        for thread in threads:
            thread.start()
        try:
            result = subprocess.run([executable, str(http.server_port), mode], timeout=20)
            if fixture.errors:
                print("Fixture errors:", fixture.errors)
                return 1
            if mode == "success" and fixture.resume_count != 1:
                print("Expected exactly one active-application resume")
                return 1
            return result.returncode
        finally:
            for server in (http, https):
                server.shutdown()
                server.server_close()
            for thread in threads:
                thread.join()


if __name__ == "__main__":
    sys.exit(main())
