"""Check or format tracked, project-owned native C++ with a pinned clang-format."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys


FORMATTER_VERSION = "18.1.8"
SOURCE_DIRECTORIES = (
    "native/adapters",
    "native/core",
    "native/streaming",
    "native/widget",
    "native/windows",
)


def run(command, root):
    return subprocess.run(command, cwd=root, capture_output=True, check=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="Report files that need formatting")
    mode.add_argument("--fix", action="store_true", help="Format tracked native C++ in place")
    parser.add_argument("--clang-format", default="clang-format", help="Formatter executable or path")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    formatter = shutil.which(args.clang_format)
    if not formatter:
        parser.error(f"Install clang-format=={FORMATTER_VERSION}, or pass --clang-format PATH")

    try:
        version = run([formatter, "--version"], root).stdout.decode("utf-8", errors="replace")
        match = re.search(r"clang-format version (\d+\.\d+\.\d+)\b", version)
        if not match or match.group(1) != FORMATTER_VERSION:
            parser.error(f"Use clang-format {FORMATTER_VERSION} for reproducible formatting")
        tracked = run(["git", "ls-files", "-z", "--", *SOURCE_DIRECTORIES], root).stdout
        files = [
            Path(name.decode("utf-8"))
            for name in tracked.split(b"\0")
            if name and Path(name.decode("utf-8")).suffix in {".cpp", ".h"}
        ]
        changed = []
        for relative in files:
            path = root / relative
            formatted = run([formatter, "--style=file", "--fallback-style=none", str(relative)], root).stdout
            if formatted == path.read_bytes():
                continue
            changed.append(relative)
            if args.fix:
                path.write_bytes(formatted)
            print(f"{'Formatted' if args.fix else 'Needs formatting'}: {relative.as_posix()}")
    except (OSError, subprocess.SubprocessError) as error:
        print(f"Formatter failed: {error}", file=sys.stderr)
        return 2

    print(f"Checked {len(files)} native C++ files; {len(changed)} {'updated' if args.fix else 'need formatting'}.")
    return 1 if changed and args.check else 0


if __name__ == "__main__":
    sys.exit(main())
