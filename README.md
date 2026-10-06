# Software Fuser

Source framework for a transparent Xbox Game Bar HUD receiving a Sunshine/Moonlight video stream from another PC.

Target: **2560 x 1440 at 240 requested FPS**, Windows 11 x64, wired LAN, black, green, or magenta source background, local mouse/keyboard input. The current milestone accepts measured throughput above 200 FPS with the source display at 244 Hz; finer FPS tuning is deferred. Full-monitor coverage remains under investigation.

## Current state

The installed candidate is 0.2.0.4, built in both configurations with zero warnings/errors. It sets the monitor-sized minimum and maximum while **Cover this monitor** is enabled, and restores resizable limits when disabled. A fresh activation reports 2560x1440 video content and widget bounds at x=0, y=-44. The supplied foreground screenshot confirms the test surface is shifted upward by about 44 pixels and its lower white border stops above the visible taskbar. Pinned placement and visible black-key acceptance are still pending; matching dimensions alone do not establish coverage. See the [taskbar test](docs/BUILD-LATER.md#taskbar-placement-test).

Version 0.2.0.5 is prepared but not installed. Both widget configurations build with zero warnings/errors. It logs widget bounds, client and visible bounds, and the video surface's local origin to distinguish a content offset from Game Bar's frame. It also observes pin/visibility changes and skips monitor-fit requests while the widget is hidden. The installed 0.2.0.4 visual test remains pending.

Version 0.2.0.3 introduced a saved Black background option, exact-black defaults, optional near-black tolerance, and automatic monitor fitting on activation, pinning transitions, and display/DPI changes. Its Debug/Release builds and core/hardware shader contracts passed. On this runtime, Game Bar rejected 2560x1440 and retained 2551x1389 content even when pinned; the user confirmed incomplete coverage.

Choose **Black**, use zero tolerance and softness to preserve nonblack HUD pixels, and **Save profile** to apply the key to a live stream. Raise tolerance only as needed for compressed near-black noise. Existing saved green/magenta profiles retain their indices. **Cover this monitor** persists independently; **Fit monitor now** requests another fit. The settings show actual video dimensions and report host constraints rather than claiming full coverage. `tests/source_hud.html` cycles green, black, and magenta and includes corner markers and dark/coloured patches.

Version 0.2 includes Sunshine PIN pairing, protected app-local credentials, authenticated application selection, Moonlight transport, hardware FFmpeg decoding, and a render worker connected through a single-frame mailbox. Release builds without warnings or errors. Six controlled pairing/storage tests pass, including wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. The six core/GPU/decoder tests pass in Debug and Release, including concurrent H.264/HEVC decoding and presentation of inter-coded fixtures.

Version 0.2.0.1 was validated with a correction that serializes DXGI presentation with hardware decoding, plus normalized Game Bar opacity handling. Debug and Release build without warnings/errors. Real Sunshine pairing and on-screen widget video/disconnect were confirmed. Moving H.264 and HEVC fixtures process approximately 240 FPS offscreen for 60 seconds, with alpha checks passing. A subsequent live widget session logged roughly 224–229 received units/s without decoder errors; a 60-second PresentMon trace tracked its swap chain at 225.606 display updates/s. The exact source workload, visible corner/keying checks, and 240 distinct displayed source frames per second remain unverified. See [validation results and measurement limits](docs/VALIDATION.md).

Activation, pinning, the test pattern, and click-through were confirmed in the baseline. The widget reports a 2560x1440 video surface, but monitor alignment, stream quality, throughput, and displayed FPS still need evidence. Enter your own Sunshine hostname or IP address; no private host address is shipped as a default. Building does not install or launch the widget; startup does not connect automatically.

Version 0.2.0.2 is built in Debug and Release with zero warnings/errors and is installed with Windows package status OK. Its current-process log confirms Game Bar activation, live HEVC 2560x1440 at 240 requested FPS, and normal Disconnect. Seven samples spanning 30.237 seconds measured 231.0–234.6 received/decoded/Present calls per second (sample mean 233.271), with zero decoder errors. The new packet, queue, submission, assembly, host-processing, associated-display, and renderer-adapter diagnostics are present. Logged shutdown completed in 267 ms. These results exceed the accepted interim throughput milestone; visual confirmation and exact distinct displayed source-frame timing remain unverified. Host timing values of zero remain classified as absent/repeated, rather than measured zero latency. See [validation results and measurement limits](docs/VALIDATION.md) for this short check and the earlier baseline recording.

| Component | Source provided | Validation remaining |
| --- | --- | --- |
| UWP C++/WinRT shell | Game Bar activation, retained widget lifetime, normal settings launch, transparency/click-through state | Release built and installed; lifecycle and visible behavior pending |
| Profile | 1440p240 defaults, input validation, explicit save to app-local settings | UI and persistence checks |
| GPU renderer | D3D11 composition swap chain, premultiplied alpha, diagnostic frame, SDR NV12 conversion, bounded GPU frame leases | Offscreen GPU tests pass on RTX 5070 Ti; Game Bar composition pending |
| Transport | PIN pairing, certificate-pinned HTTPS, application selection, view-only Moonlight core | Real pairing, offscreen streaming, and Game Bar HEVC video/disconnect pass |
| Decoder | Hardware-only FFmpeg D3D11VA, crop/color/timing metadata, retained pool leases | Moving H.264/HEVC fixtures process about 240 FPS offscreen for 60 s; live moving-source throughput pending |
| Frame handoff | Ordered decode, latest-frame mailbox, joined transport/render shutdown | Three native reconnect cycles reuse the session and renderer successfully; Game Bar recovery pending |
| Measurement | Separate received/decoded/present counters, packet timing diagnostics, paced fixture benchmark, PresentMon trace | 0.2.0.2 logs about 233 receive/decode/Present calls per second; its new diagnostics are verified. Baseline ETW tracks about 226 display updates/s; source content and end-to-end latency remain unverified |

Choose **Pair with Sunshine**, enter the displayed PIN on the source PC's Sunshine PIN page, select an application, and click **Connect**. Existing pairing survives package updates; use **Refresh apps** to load applications without pairing again. The active source app is selected when present; switching away from another active app is refused. Disconnect stops this client's stream without sending a host quit command; Sunshine's application lifecycle settings still apply. Save profile applies key settings to an active stream; video changes apply on the next connection. Audio playback is muted, and no user input is forwarded. The upstream startup mouse wake-up is removed in the isolated build copy. GameStream audio packets may still be received.

The preview draws **one local test frame per request**. Stream setup FPS and measured receive/decode/present-call rates are displayed separately. Present calls do not prove monitor scanout or end-to-end latency.

## Layout

- `SoftwareFuser.sln` / `native/widget`: Windows UWP project, XAML settings, Game Bar manifest.
- `native/core`: platform-neutral C++20 settings, stream contracts, and decoded-frame mailbox.
- `native/windows`: D3D11 surface ownership and renderer.
- `native/shaders`: fullscreen vertex shader and NV12/chroma-key pixel shader.
- `native/streaming`: Sunshine control, pinned Moonlight build, and session orchestration.
- `tests`: core/GPU/decoder contracts and loopback pairing/credential tests.
- `tools`: repeatable build, explicit deployment, package logos, and read-only Sunshine probe.
- `docs`: architecture, setup, research references, and feasibility evidence.

## Next work

The installed version is 0.2.0.4. Version 0.2.0.2 established the accepted above-200-FPS live pipeline checkpoint. Full-monitor placement and on-screen black-key acceptance remain pending. Deployment closes the widget and opens a Windows elevation prompt. While gaming, defer deployment and tests that interrupt the user's session or add substantial GPU load.

- Resolve the reproduced Game Bar size constraint and verify four-corner coverage, DPI, and monitor changes.
- Confirm on-screen black transparency and click-through; decoded full/limited-range black, nonblack preservation, and premultiplied soft edges pass GPU tests.
- Defer fine FPS tuning while the measured pipeline remains above 200 FPS.

Read [the architecture](docs/ARCHITECTURE.md), [build and deployment instructions](docs/BUILD-LATER.md), and [the implementation backlog](docs/ROADMAP.md). At each checkpoint, verify and commit the important source/documentation changes, push them to the configured Git remote, and confirm the remote commit matches. Keep local evidence archives and private data excluded.

Generated packages, runtime logs, checkpoints, local machine/network notes, environment files, certificates, and credential files are ignored by Git. Pairing credentials stay in protected app-local storage. Public dependency license notices are retained in `third_party/licenses`.

The installed `cpp-coding-standards` and `verification-before-completion` skills guide source work and evidence reporting. Use `systematic-debugging` when a real failure occurs. `pr-review` is scoped to its Microsoft skills repository and is not automatically applicable to this project. No additional skills are needed for the current foundation; recommend a relevant one if later integration reveals a concrete gap.
