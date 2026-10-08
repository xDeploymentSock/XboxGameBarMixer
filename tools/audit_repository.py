"""Read-only checks for common private-data patterns and broken repository links."""

import argparse
import ipaddress
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

PATTERNS = {
    "personal_windows_path": re.compile(r"""(?i)\b[A-Z]:[\\/]+Users[\\/]+[^\\/\s<>"']+"""),
    "private_key_material": re.compile(r"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----"),
    "github_token": re.compile(r"(?:github_pat_[A-Za-z0-9_]{20,}|gh[pousr]_[A-Za-z0-9_]{20,})"),
    "aws_access_key": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
    "api_key": re.compile(r"\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{30,}"),
    "slack_token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}"),
}
PRIVATE_NETWORKS = [
    ipaddress.ip_network((0x0A000000, 8)),
    ipaddress.ip_network((0xAC100000, 12)),
    ipaddress.ip_network((0xC0A80000, 16)),
]
IPV4 = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")
EMAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}")
LOCAL_LINK = re.compile(r"\]\((<[^>]+>|[^)\s]+)(?:\s+[^)]*)?\)")
PRIVATE_DIRS = {".aws", ".ssh", ".codex", ".agents", ".vs"}
OUTPUT_DIRS = {"build", "out", "packages", "apppackages", "cmakefiles", "__pycache__"}
PRIVATE_SUFFIXES = {".pem", ".key", ".p12", ".pfx", ".cer", ".crt", ".der", ".jks", ".credentials", ".protected", ".pending", ".dat", ".dmp", ".mdmp", ".log", ".etl"}
OUTPUT_SUFFIXES = {".exe", ".dll", ".lib", ".obj", ".pdb", ".msix", ".appx", ".msixbundle", ".appxbundle", ".zip", ".7z"}


def git(root, *arguments):
    return subprocess.check_output(["git", "-C", str(root), *arguments], stderr=subprocess.PIPE)


def names(data):
    return {name.decode("utf-8") for name in data.split(b"\0") if name}


def forbidden_path(name):
    path = PurePosixPath(name)
    parts = {part.lower() for part in path.parts}
    base = path.name.lower()
    if parts & (PRIVATE_DIRS | OUTPUT_DIRS) or name.lower().startswith("docs/private/"):
        return True
    if path.suffix.lower() in PRIVATE_SUFFIXES | OUTPUT_SUFFIXES:
        return True
    if base == ".env" or base.startswith(".env.") or base == "local.settings.json":
        return True
    if re.fullmatch(r"(?:credentials|secrets|pairing).*\.(?:json|xml)", base):
        return True
    return path.suffix.lower() == ".csv" and name != "docs/measurement-template.csv"


def png_metadata(data):
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return True
    offset = 8
    while offset + 12 <= len(data):
        size = int.from_bytes(data[offset:offset + 4], "big")
        chunk = data[offset + 4:offset + 8]
        if chunk in {b"tEXt", b"iTXt", b"zTXt", b"eXIf"}:
            return True
        offset += size + 12
    return False


def inspect(name, data, targets):
    findings = []

    def record(kind, offset=0):
        findings.append({"path": name, "type": kind, "line": data[:offset].count(b"\n") + 1})

    if forbidden_path(name):
        record("private_or_generated_file")
    if name.lower().endswith(".png"):
        if png_metadata(data):
            record("image_metadata_requires_review")
        return findings
    try:
        text = data.decode("utf-8-sig")
    except UnicodeDecodeError:
        record("unexpected_binary_requires_review")
        return findings

    def text_record(kind, offset):
        findings.append({"path": name, "type": kind, "line": text.count("\n", 0, offset) + 1})

    for kind, pattern in PATTERNS.items():
        for match in pattern.finditer(text):
            text_record(kind, match.start())
    for match in IPV4.finditer(text):
        try:
            address = ipaddress.ip_address(match.group())
        except ValueError:
            continue
        if any(address in network for network in PRIVATE_NETWORKS):
            text_record("private_network_address", match.start())
    public_notice = name.startswith("third_party/licenses/") or name.endswith("VCPKG-LICENSE.txt")
    if not public_notice:
        for match in EMAIL.finditer(text):
            text_record("email_requires_review", match.start())
    if name.lower().endswith(".md"):
        for match in LOCAL_LINK.finditer(text):
            link = unquote(match.group(1).strip("<>"))
            url = urlsplit(link)
            if url.scheme or url.netloc or not url.path:
                continue
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), url.path))
            if target not in targets:
                text_record("missing_tracked_documentation_link", match.start())
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged", action="store_true", help="Check only added/modified staged blobs, including staged link targets.")
    args = parser.parse_args()
    root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], stderr=subprocess.PIPE).decode().strip())
    tracked = names(git(root, "ls-files", "-z"))
    paths = names(git(root, "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z")) if args.staged else tracked
    ignored = names(git(root, "ls-files", "-ci", "--exclude-standard", "-z"))
    targets = set(tracked)
    for path in tracked:
        targets.update(str(parent) for parent in PurePosixPath(path).parents)
    findings = []
    for name in sorted(paths):
        try:
            data = git(root, "show", ":" + name) if args.staged else (root / name).read_bytes()
        except (OSError, subprocess.CalledProcessError):
            findings.append({"path": name, "type": "tracked_file_unreadable"})
            continue
        file_findings = inspect(name, data, targets)
        if name in ignored and not any(item["type"] == "private_or_generated_file" for item in file_findings):
            file_findings.append({"path": name, "type": "tracked_ignored_file"})
        findings.extend(file_findings)
    print(json.dumps({"scope": "staged blobs" if args.staged else "tracked working files",
                      "checked_files": len(paths), "findings": findings}, indent=2))
    return 1 if findings else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Repository audit could not run: {type(error).__name__}", file=sys.stderr)
        sys.exit(2)
