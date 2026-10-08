"""Save and verify project source, package, and selected measurement evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile


# This is a private local recovery archive, not a distributable source bundle.
# Select source from Git's index rather than walking ignored runtime folders.
PRIVATE_SUFFIXES = {".pem", ".key", ".p12", ".pfx", ".cer", ".crt", ".der", ".jks", ".protected", ".pending", ".credentials", ".dat"}

PRIVATE_DIRECTORIES = {".git", ".aws", ".ssh", ".codex", ".agents", ".vs"}

def source_files(root, extra_files=()):
    root = root.resolve()
    tracked = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"]).decode("utf-8").split("\0")
    selected = {root / name for name in tracked if name}
    for name in extra_files:
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Checkpoint evidence must use a workspace-relative path without parent traversal")
        selected.add(root / relative)
    for path in selected:
        if (not path.resolve().is_relative_to(root) or not path.is_file() or path.is_symlink()
                or path.suffix.lower() in PRIVATE_SUFFIXES
                or {part.lower() for part in path.relative_to(root).parts} & PRIVATE_DIRECTORIES
                or path.name.lower() == ".env" or path.name.lower().startswith(".env.")
                or path.name.lower() == "local.settings.json"
                or re.fullmatch(r"(?:credentials|secrets|pairing).*\.(?:json|xml)", path.name, re.IGNORECASE)):
            raise ValueError("Missing, private, linked or outside-workspace checkpoint input")
    return sorted(selected, key=lambda path: path.relative_to(root).as_posix())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default=datetime.now().strftime("%Y%m%d-%H%M%S"))
    parser.add_argument("--installed-widget", help="Version from a fresh Get-AppxPackage check; omit if unknown")
    parser.add_argument("--evidence", action="append", default=[], help="Additional workspace-relative evidence file")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9-]{1,64}", args.label):
        parser.error("Label must contain 1–64 letters, digits, or hyphens")
    if args.installed_widget is not None and not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", args.installed_widget):
        parser.error("Installed widget version must contain four numeric components")
    root = Path(__file__).resolve().parent.parent
    namespace = {"appx": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}
    identity = ET.parse(root / "native/widget/Package.appxmanifest").find("appx:Identity", namespace)
    version = identity.attrib["Version"]
    package = f"AppPackages/FuserWidget/FuserWidget_{version}_x64_Test/FuserWidget_{version}_x64.msix"
    # Read the actual built package instead of assuming a version from source.
    with zipfile.ZipFile(root / package) as built_package:
        built_identity = ET.fromstring(built_package.read("AppxManifest.xml")).find("appx:Identity", namespace)
        if built_identity.attrib["Version"] != version or built_identity.attrib["Name"] != identity.attrib["Name"]:
            raise ValueError("Prepared package identity does not match source")
    optional_evidence = [f"build/prepared-widget-{version}.json",
                         f"build/widget-Release-{version}.log", f"build/widget-Debug-{version}.log"]
    current_evidence = [package] + [name for name in optional_evidence if (root / name).is_file()]
    output_directory = root / "build/checkpoints"
    output_directory.mkdir(parents=True, exist_ok=True)
    archive_path = output_directory / f"software-fuser-{args.label}.zip"
    receipt_path = archive_path.with_suffix(".json")
    if archive_path.exists() or receipt_path.exists():
        parser.error("Checkpoint already exists; use a new label")
    files = source_files(root, current_evidence + args.evidence)
    manifest = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "installed_widget": args.installed_widget, "prepared_widget": built_identity.attrib["Version"],
        "limits": "Excludes installed app state, protected pairing credentials, build dependencies, and downloaded tools. Restore into a separate directory before selecting files to copy back.",
        "files": [],
    }
    with zipfile.ZipFile(archive_path, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            data = path.read_bytes()
            relative = path.relative_to(root).as_posix()
            manifest["files"].append({"path": relative, "bytes": len(data),
                                      "sha256": hashlib.sha256(data).hexdigest()})
            archive.writestr(relative, data)
        archive.writestr("checkpoint-manifest.json", json.dumps(manifest, indent=2) + "\n")
    with zipfile.ZipFile(archive_path) as archive:
        for entry in manifest["files"]:
            if hashlib.sha256(archive.read(entry["path"])).hexdigest() != entry["sha256"]:
                raise ValueError(f"Archive verification failed: {entry['path']}")
        if archive.testzip() is not None:
            raise ValueError("Archive CRC verification failed")
    receipt = {
        "archive": str(archive_path), "sha256": hashlib.sha256(archive_path.read_bytes()).hexdigest(),
        "bytes": archive_path.stat().st_size, "files_verified": len(files),
        "verified": True, "created_utc": manifest["created_utc"],
    }
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
