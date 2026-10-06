# Software Fuser

Source framework for a transparent Xbox Game Bar HUD receiving a Sunshine/Moonlight video stream from another PC.

Target: **2560 x 1440 at 240 requested FPS**, Windows 11 x64, wired LAN, black, green, or magenta source background, local mouse/keyboard input. The current milestone accepts measured throughput above 200 FPS with the source display at 244 Hz; finer FPS tuning is deferred. Black transparency and click-through were confirmed. The user clarified that covering the taskbar requires manual dragging; automatic four-edge coverage has not passed.

## Current state

Version 0.2.0.11 adds an explicit [pinned-only coverage probe](docs/PINNED-COVERAGE-TEST.md): arm while pinned, dismiss Game Bar, then apply full-monitor limits and one resize request in pinned-only mode. Both widget builds and focused core contracts pass; the Release test is installed with package status OK. It does not rescale the Sunshine feed or add automatic fitting on future transitions. Full-monitor coverage remains unverified for this probe.

This checkout is the `codex/fixed-startup-coverage` experiment: version 0.2.0.10 declares a fixed 2560x1440 startup client size and preserves its constraints during activation. Both widget builds and focused layout checks pass. The installed test obtains the full client size, but Game Bar places it at Y=-44, leaving a 44-pixel bottom gap; reopening repeats the offset and the pin button is off-screen. Reset restores accessible controls and pinning succeeds, but a subsequent pinned Fit request for 2560x1440 is rejected, retaining the 480x700 video area. Four-edge coverage has failed these live tests. The 0.2.0.9 baseline source/package and the main branch are preserved. Read [the startup test result](docs/FIXED-STARTUP-TEST.md) for the measured bounds and follow-up probe.

Version 0.2.0.9 was installed with Windows package status OK before the branch experiment. Debug and Release builds have zero warnings/errors, and the focused core layout contracts pass in both configurations. Live logs confirm custom Apply requests reached Game Bar, but 2560x1440 was rejected. Full-screen fit was also declined, as confirmed by the user and two logged `false` responses. Manual adjustment trades the top gap for lost taskbar coverage, according to the user's follow-up; it has not solved simultaneous four-edge coverage. The top gap remains unresolved.

Version 0.2.0.9 adds **Overlay width/height (px)** and **Apply dimensions**. Values are physical pixels, converted using the current display's DPI; they resize the widget separately from the negotiated video resolution. Saved dimensions are not applied automatically on reopening. The latest click supersedes older queued requests. Coverage reporting now shows physical-pixel gaps at all four edges. The supplied 0.2.0.8 screenshot and local geometry show a 46-pixel top gap, with content at Y=46 and size 2558x1394 on a 2560x1440 display; video-local origin is zero, so the missing strip is outside the renderer's client surface.

**Try full-screen fit** explicitly tests Windows' public [ApplicationView full-screen request](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.applicationview.tryenterfullscreenmode). It reports rejection or remaining gaps rather than treating API acceptance as coverage proof. This host declined it. The app's UWP title bar has zero height, is hidden, and is not extended; its own title-bar switch cannot reclaim the external 46-pixel strip. The failed attempt does not also center or resize the widget, preserving placement. Fit, Apply, and Reset exit full-screen mode before their normal resize; Reset restores a smaller centered window. The [Game Bar API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) documents size/centering requests but no arbitrary-position or title-bar-hiding method. A separate borderless presentation window controlled by Game Bar is the proposed next approach, pending the user's architecture choice. See the [placement test](docs/BUILD-LATER.md#taskbar-placement-test).

Version 0.2.0.8 changed **Fit my monitor** into an immediate full-display size request, available while Game Bar is open. Drag and resize controls remain enabled. Opening, closing, or pinning Game Bar no longer initiates a resize, shrinks the widget to the settings-window size, or recenters manual placement. Fit deliberately avoids host centering, which can shift or shrink the surface. Hiding or changing displays cancels stale work. The user's next screenshot exposed the remaining top gap; automatic four-edge coverage has not passed.

The geometry diagnostics first built in 0.2.0.5 are included in 0.2.0.6: widget/client/visible bounds, video-local origin, pin/visibility events, preview key settings, and layout coordinates before and after centering. Version 0.2.0.4's supplied foreground screenshot showed a full-sized surface shifted upward about 44 pixels; the user reported that taskbar auto-hide did not fix it and that the widget could not be moved. The built-in Audio widget can overlap the taskbar in the user's comparison, so this is not treated as a universal Windows restriction.

Version 0.2.0.3 introduced a saved Black background option, exact-black defaults, optional near-black tolerance, and automatic monitor fitting on activation, pinning transitions, and display/DPI changes. Its Debug/Release builds and core/hardware shader contracts passed. On this runtime, Game Bar rejected 2560x1440 and retained 2551x1389 content even when pinned; the user confirmed incomplete coverage.

Choose **Black**, use zero tolerance and softness to preserve nonblack HUD pixels, and **Save profile** to apply the key to a live stream. Raise tolerance only as needed for compressed near-black noise. Existing saved green/magenta profiles retain their indices. Click **Fit my monitor** to request the full monitor size immediately; the previous pinned-coverage checkbox is removed and its stored preference no longer triggers layout changes. Reset preserves pairing and video/key profiles. The settings report actual video dimensions and position rather than treating monitor-sized content as proof of alignment. `tests/source_hud.html` cycles green, black, and magenta and includes corner markers and dark/coloured patches.

Version 0.2 includes Sunshine PIN pairing, protected app-local credentials, authenticated application selection, Moonlight transport, hardware FFmpeg decoding, and a render worker connected through a single-frame mailbox. Release builds without warnings or errors. Six controlled pairing/storage tests pass, including wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. The six core/GPU/decoder tests pass in Debug and Release, including concurrent H.264/HEVC decoding and presentation of inter-coded fixtures.

Version 0.2.0.1 was validated with a correction that serializes DXGI presentation with hardware decoding, plus normalized Game Bar opacity handling. Debug and Release build without warnings/errors. Real Sunshine pairing and on-screen widget video/disconnect were confirmed. Moving H.264 and HEVC fixtures process approximately 240 FPS offscreen for 60 seconds, with alpha checks passing. A subsequent live widget session logged roughly 224–229 received units/s without decoder errors; a 60-second PresentMon trace tracked its swap chain at 225.606 display updates/s. The exact source workload, visible corner/keying checks, and 240 distinct displayed source frames per second remain unverified. See [validation results and measurement limits](docs/VALIDATION.md).

Activation, pinning, the test pattern, and click-through were confirmed in the baseline. Black transparency was later confirmed by the user. The reported full-monitor result required manual dragging, and automatic taskbar coverage remains unverified. Live stream quality and distinct displayed source-frame timing still need controlled evidence. Enter your own Sunshine hostname or IP address; no private host address is shipped as a default. Building does not install or launch the widget; startup does not connect automatically.

Version 0.2.0.2 built in Debug and Release with zero warnings/errors and was installed with Windows package status OK before later updates. Its checkpoint log confirms Game Bar activation, live HEVC 2560x1440 at 240 requested FPS, and normal Disconnect. Seven samples spanning 30.237 seconds measured 231.0–234.6 received/decoded/Present calls per second (sample mean 233.271), with zero decoder errors. The new packet, queue, submission, assembly, host-processing, associated-display, and renderer-adapter diagnostics are present. Logged shutdown completed in 267 ms. These results exceed the accepted interim throughput milestone; visual confirmation and exact distinct displayed source-frame timing remain unverified. Host timing values of zero remain classified as absent/repeated, rather than measured zero latency. See [validation results and measurement limits](docs/VALIDATION.md) for this short check and the earlier baseline recording.

| Component | Source provided | Validation remaining |
| --- | --- | --- |
| UWP C++/WinRT shell | Game Bar activation, retained widget lifetime, normal settings launch, transparency/click-through state | User confirmed Reset, black transparency, click-through, and manually dragged taskbar coverage; immediate fit and placement preservation need live checks |
| Profile | 1440p240 defaults, input validation, explicit save to app-local settings | UI and persistence checks |
| GPU renderer | D3D11 composition swap chain, premultiplied alpha, diagnostic frame, SDR NV12 conversion, bounded GPU frame leases | GPU contracts pass on RTX 5070 Ti; user confirmed pinned black-key preview; controlled live HUD checks remain |
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

Version 0.2.0.2 established the accepted above-200-FPS live pipeline checkpoint. Black transparency and click-through are confirmed. The user accepts manual adjustment when needed. Check custom dimensions, the full-screen host response, and retention of manual placement in 0.2.0.9 before claiming a coverage fix. Deployment closes the widget and opens a Windows elevation prompt. While gaming, defer deployment and tests that interrupt the user's session or add substantial GPU load.

- Verify repeated fitting, DPI, and monitor changes; earlier resize rejections remain recorded for comparison.
- Validate a controlled live black-background HUD; decoded full/limited-range black, nonblack preservation, and premultiplied soft edges pass GPU tests.
- Defer fine FPS tuning while the measured pipeline remains above 200 FPS.

Read [the architecture](docs/ARCHITECTURE.md), [build and deployment instructions](docs/BUILD-LATER.md), and [the implementation backlog](docs/ROADMAP.md). At each checkpoint, verify and commit the important source/documentation changes, push them to the configured Git remote, and confirm the remote commit matches. Keep local evidence archives and private data excluded.

Generated packages, runtime logs, checkpoints, local machine/network notes, environment files, certificates, and credential files are ignored by Git. Pairing credentials stay in protected app-local storage. Public dependency license notices are retained in `third_party/licenses`.

The installed `cpp-coding-standards` and `verification-before-completion` skills guide source work and evidence reporting. Use `systematic-debugging` when a real failure occurs. `pr-review` is scoped to its Microsoft skills repository and is not automatically applicable to this project. No additional skills are needed for the current foundation; recommend a relevant one if later integration reveals a concrete gap.
