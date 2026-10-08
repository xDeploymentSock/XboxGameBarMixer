# Coding standards

Keep behavior easy to follow and ownership easy to verify. This project uses C++20 and the C++ Core Guidelines, adapted to C++/WinRT and D3D11.

## Readability

- Use descriptive names, small focused functions and early returns for invalid inputs.
- Keep separate operations on separate lines. Explain ordering, ownership and platform constraints in comments.
- Prefer `const`, scoped values and RAII. Use `auto` where the initializer makes the type clear.
- Preserve required include order, especially UWP precompiled headers and generated shader headers.
- Format project-owned native C++ with clang-format **18.1.8** and the checked-in `.clang-format`. Generated files and dependency ports are excluded.

```text
python -m pip install clang-format==18.1.8
python tools/format_cpp.py --check
python tools/format_cpp.py --fix
```

The check is read-only; the fix command edits tracked native `.cpp` and `.h` files. Tests, diagnostic tools and third-party sources are outside this formatting pass.

## Correctness and performance

- Express ownership with RAII and smart pointers; use raw pointers for borrowed interfaces.
- Keep compressed reference frames in decode order. Drop only decoded display frames.
- Retain GPU frame leases until GPU reads finish. Synchronize the shared D3D11 immediate context.
- Do not block the UI with networking or streaming teardown. Reject queued work for stopped views.
- Keep callbacks from leaking exceptions into C/Windows interfaces; report meaningful failure states.
- Preserve saved profiles and pairing. Keep keys, addresses, logs and captures out of published files.
- Measure representative workloads before optimizing. Distinguish CPU calls and pipeline counters from displayed frames and optical latency.

## Verification

Run the checks in [CONTRIBUTING.md](CONTRIBUTING.md), plus the relevant native builds and hardware checks when behavior changes. Treat warnings as work to investigate. Record unsupported sanitizer/platform checks and untested live behavior explicitly.
