"""Save and verify project source, package, and selected measurement evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import zipfile


ROOT_FILES = (".gitignore", "CMakeLists.txt", "README.md", "SoftwareFuser.sln",
              "THIRD-PARTY-NOTICES.md", "skills-lock.json")
SOURCE_DIRECTORIES = ("native", "tools", "tests", "docs", "third_party/licenses")
EXCLUDED_DIRECTORIES = {"Generated Files", "GeneratedFiles", "__pycache__", "x64",
                        "Debug", "Release", "obj", "ipch", ".git"}
EXCLUDED_SUFFIXES = {".user", ".suo", ".pfx", ".cer", ".pyc"}
EVIDENCE_FILES = (
    "build/prepared-widget-0.2.0.2.json",
    "build/widget-Release-0.2.0.2.log", "build/widget-Debug-0.2.0.2.log",
    "build/widget-observation-20261005-201430.csv",
    "build/widget-observation-20261005-201430-summary.json",
    "build/widget-active-session-rate-samples-20261005.csv",
    "build/widget-active-session-rate-summary-20261005.json",
    "build/fuser-presentmon-20261005-200922.csv",
    "build/fuser-presentmon-20261005-200922-summary.json",
    "build/fuser-presentmon-20261005-200922.log",
    "build/display-path-map-20261005.log",
    "build/moving-fixture-hevc-240-isolated.log",
    "build/moving-fixture-h264-240-isolated.log",
    "build/live-hevc-240-120s.log", "build/live-hevc-reconnect-3-cycles.log",
    "build/live-hevc-idle-60.log", "build/live-hevc-idle-120.log",
    "build/live-hevc-idle-240.log", "build/source-apps-20261005-2345.log",
    "build/fixtures/key-pattern.h264", "build/fixtures/key-pattern.hevc",
    "build/fixtures/inter-pattern.h264", "build/fixtures/inter-pattern.hevc",
    "build/fixtures/moving-pattern.h264", "build/fixtures/moving-pattern.hevc",
    "AppPackages/FuserWidget/FuserWidget_0.2.0.1_x64_Test/FuserWidget_0.2.0.1_x64.msix",
    "AppPackages/FuserWidget/FuserWidget_0.2.0.2_x64_Test/FuserWidget_0.2.0.2_x64.msix",
)


def source_files(root):
    selected = {root / name for name in ROOT_FILES}
    for directory in SOURCE_DIRECTORIES:
        for folder, subdirectories, filenames in os.walk(root / directory, followlinks=False):
            subdirectories[:] = [name for name in subdirectories
                                 if name not in EXCLUDED_DIRECTORIES
                                 and not (Path(folder) / name).is_symlink()]
            for name in filenames:
                path = Path(folder) / name
                if path.suffix.lower() not in EXCLUDED_SUFFIXES and not path.is_symlink():
                    selected.add(path)
    selected.update(root / name for name in EVIDENCE_FILES)
    for path in selected:
        if not path.resolve().is_relative_to(root) or not path.is_file():
            raise ValueError(f"Missing or outside-workspace checkpoint input: {path}")
    return sorted(selected, key=lambda path: path.relative_to(root).as_posix())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default=datetime.now().strftime("%Y%m%d-%H%M%S"))
    parser.add_argument("--installed-widget", help="Version from a fresh Get-AppxPackage check; omit if unknown")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9-]{1,64}", args.label):
        parser.error("Label must contain 1–64 letters, digits, or hyphens")
    if args.installed_widget is not None and not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", args.installed_widget):
        parser.error("Installed widget version must contain four numeric components")
    root = Path(__file__).resolve().parent.parent
    output_directory = root / "build/checkpoints"
    output_directory.mkdir(parents=True, exist_ok=True)
    archive_path = output_directory / f"software-fuser-{args.label}.zip"
    receipt_path = archive_path.with_suffix(".json")
    if archive_path.exists() or receipt_path.exists():
        parser.error("Checkpoint already exists; use a new label")
    files = source_files(root)
    manifest = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "installed_widget": args.installed_widget, "prepared_widget": "0.2.0.2",
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
