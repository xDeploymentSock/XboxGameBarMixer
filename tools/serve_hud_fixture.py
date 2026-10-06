"""Temporarily serve one owned HTML fixture to the sending PC; no directory listing."""
import argparse
import http.server
import ipaddress
from pathlib import Path
import time
from urllib.parse import urlsplit


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--minutes", type=float, default=30)
    args = parser.parse_args()
    bind = str(ipaddress.IPv4Address(args.bind))
    source = str(ipaddress.IPv4Address(args.source))
    if not 1 <= args.port <= 65535 or not 0 < args.minutes <= 120:
        parser.error("Use a valid port and a lifetime up to 120 minutes.")
    payload = (Path(__file__).resolve().parents[1] / "tests" / "source_hud.html").read_bytes()

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.client_address[0] not in {source, bind, "127.0.0.1"}:
                self.send_error(403)
                return
            if urlsplit(self.path).path not in {"/", "/hud.html"}:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(payload)

        def log_message(self, *_):
            pass

    deadline = time.monotonic() + args.minutes * 60
    with http.server.ThreadingHTTPServer((bind, args.port), Handler) as server:
        server.timeout = 0.5
        print(f"HUD fixture: http://{bind}:{args.port}/hud.html (expires in {args.minutes:g} minutes)", flush=True)
        while time.monotonic() < deadline:
            server.handle_request()


if __name__ == "__main__":
    main()
