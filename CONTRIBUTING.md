# Contributing

Keep changes scoped to the reported behavior and include enough evidence for someone else to review them. Describe the trigger, resulting behavior, verification and any remaining limits.

## Worktree and runtime boundaries

- Inspect `git status` and the active branch before editing. Use the intended worktree; preserve other active branches and uncommitted work.
- Main contains the usable-area video fit. The separate testing branch investigates host coverage. Do not merge that work implicitly.
- Video presentation stays inside the Xbox Game Bar UWP widget.
- Preserve active source-control sessions. Installation closes Software Fuser and may open Windows elevation; schedule it for an authorized idle window.
- Keep GPU-heavy benchmarks and disruptive live checks out of ongoing gameplay.

## Readable code

Follow [CODING_STANDARDS.md](CODING_STANDARDS.md). Check native C++ with clang-format 18.1.8:

```text
python tools/format_cpp.py --check
```

## Verify a checkpoint

With CMake 3.24+, a C++20 compiler and Python 3.10+:

```text
python tools/audit_repository.py
python -m unittest discover -s tests -p timing_summary_tests.py
cmake -S . -B build/core -DFUSER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
git diff --check
```

The default CMake options leave GPU, decoder, control and display-probe targets disabled. CI checks the portable core on Windows and Linux, plus native formatting and Linux AddressSanitizer/UndefinedBehaviorSanitizer contracts. It does not validate the UWP package or hardware paths. For changes to those paths, use the targeted [Windows build and validation procedures](docs/BUILD-LATER.md). Record actual output before claiming a check passed.

Stage intended files, review the staged diff, and run `python tools/audit_repository.py --staged` before committing. That mode reads the staged blobs; the default checks all tracked working files. Review newly added files carefully, then commit/push authorized checkpoints and compare the remote branch with the local commit. Do not rewrite shared history or remove active worktrees as routine cleanup.

## Publication hygiene

Do not commit private host addresses, user paths, PINs, credentials, private keys, screenshots or local runtime evidence. Keep them under ignored `build/` or `docs/private/` where appropriate. Build outputs, MSIX packages and certificates are also ignored. `.gitignore` does not remove files already tracked.

The audit flags common private-address, credential and local-path patterns, disallowed tracked files and missing local Markdown targets. It reports file names and finding types without printing matched values. It cannot identify every form of personal information or secret; inspect the diff as well. Preserve public third-party copyright and license notices.

Project code has no root distribution license selected yet. Retained dependency licenses and notices are described in [third-party notices](THIRD-PARTY-NOTICES.md); they do not supply a license for the project's own code.
