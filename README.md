# Software Fuser

Source framework for a transparent Xbox Game Bar HUD receiving a Sunshine/Moonlight video stream from another PC.

Target: **2560 x 1440 at 240 requested FPS**, Windows 11 x64, wired LAN, reserved green or magenta source background, local mouse/keyboard input. Actual displayed frame rate and full-monitor coverage remain feasibility gates.

## Current state

Version 0.2 includes Sunshine PIN pairing, protected app-local credentials, authenticated application selection, Moonlight transport, hardware FFmpeg decoding, and a render worker connected through a single-frame mailbox. Release builds without warnings or errors. Six controlled pairing/storage tests pass, including wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. The six core/GPU/decoder tests pass in Debug and Release, including concurrent H.264/HEVC decoding and presentation of inter-coded fixtures.

Version 0.2.0.1 was validated with a correction that serializes DXGI presentation with hardware decoding, plus normalized Game Bar opacity handling. Debug and Release build without warnings/errors. Real Sunshine pairing and on-screen widget video/disconnect were confirmed. Moving H.264 and HEVC fixtures process approximately 240 FPS offscreen for 60 seconds, with alpha checks passing. A subsequent live widget session logged roughly 224–229 received units/s without decoder errors; a 60-second PresentMon trace tracked its swap chain at 225.606 display updates/s. The exact source workload, visible corner/keying checks, and 240 distinct displayed source frames per second remain unverified. See [validation results and measurement limits](docs/VALIDATION.md).

Activation, pinning, the test pattern, and click-through were confirmed in the baseline. The widget reports a 2560x1440 video surface, but monitor alignment, stream quality, throughput, and displayed FPS still need evidence. Enter your own Sunshine hostname or IP address; no private host address is shipped as a default. Building does not install or launch the widget; startup does not connect automatically.

Version 0.2.0.2 is built in Debug and Release with zero warnings/errors, but is not installed. It adds cumulative packet-index, queue-depth, decoder-submission, frame-assembly, host-processing, associated-display size, and renderer-adapter diagnostics to the bounded local log. The passive 30-minute recording on installed 0.2.0.1 has finished: 118 fresh observations average 225.719 received units/s, with zero decoder errors; its final three observations followed a disconnect and are excluded. Host timing values of zero remain classified as absent/repeated; they are not reported as measured zero latency.

| Component | Source provided | Validation remaining |
| --- | --- | --- |
| UWP C++/WinRT shell | Game Bar activation, retained widget lifetime, normal settings launch, transparency/click-through state | Release built and installed; lifecycle and visible behavior pending |
| Profile | 1440p240 defaults, input validation, explicit save to app-local settings | UI and persistence checks |
| GPU renderer | D3D11 composition swap chain, premultiplied alpha, diagnostic frame, SDR NV12 conversion, bounded GPU frame leases | Offscreen GPU tests pass on RTX 5070 Ti; Game Bar composition pending |
| Transport | PIN pairing, certificate-pinned HTTPS, application selection, view-only Moonlight core | Real pairing, offscreen streaming, and Game Bar HEVC video/disconnect pass |
| Decoder | Hardware-only FFmpeg D3D11VA, crop/color/timing metadata, retained pool leases | Moving H.264/HEVC fixtures process about 240 FPS offscreen for 60 s; live moving-source throughput pending |
| Frame handoff | Ordered decode, latest-frame mailbox, joined transport/render shutdown | Three native reconnect cycles reuse the session and renderer successfully; Game Bar recovery pending |
| Measurement | Separate received/decoded/present counters, packet timing diagnostics, paced fixture benchmark, PresentMon trace | Current live swap chain tracks about 226 display updates/s; exact source content and end-to-end latency remain unverified. Latest packet telemetry is not installed |

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

Read [the architecture](docs/ARCHITECTURE.md), [build and deployment instructions](docs/BUILD-LATER.md), and [the implementation backlog](docs/ROADMAP.md). The user's earlier gaming-session restriction was lifted before building and installing.

Generated packages, runtime logs, checkpoints, local machine/network notes, environment files, certificates, and credential files are ignored by Git. Pairing credentials stay in protected app-local storage. Public dependency license notices are retained in `third_party/licenses`.

The installed `cpp-coding-standards` and `verification-before-completion` skills guide source work and evidence reporting. Use `systematic-debugging` when a real failure occurs. `pr-review` is scoped to its Microsoft skills repository and is not automatically applicable to this project. No additional skills are needed for the current foundation; recommend a relevant one if later integration reveals a concrete gap.
