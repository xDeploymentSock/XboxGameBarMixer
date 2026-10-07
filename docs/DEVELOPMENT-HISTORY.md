# Development history

This archive preserves the README and backlog before the repository cleanup at 0.2.1.6. Version-specific installation statements, pending checks and proposed approaches describe earlier checkpoints. For current behavior and constraints, use the [README](../README.md), [roadmap](ROADMAP.md) and [validation record](VALIDATION.md).

The installed 0.2.1.6 color appearance was subsequently accepted by the user. Presentation remains inside the Xbox Game Bar UWP widget. The separate testing branch investigates host coverage; a companion desktop renderer is not the selected architecture.

## Previous README

Source framework for a transparent Xbox Game Bar HUD receiving a Sunshine/Moonlight video stream from another PC.

Version 0.2.1.4 adds [client-origin fitting, Crisp HUD scaling and reduced receiver queuing](HUD-QUALITY-LATENCY.md), plus a 1440p/HEVC/100 Mbps preset. The complete frame remains scaled above the taskbar. Receiver fixture and offscreen live checks establish local processing improvements; installed-widget alignment, text appearance and screen latency require live verification.

Version 0.2.1.5 adds [GPU resource reuse and more accurate performance diagnostics](RECEIVER-PERFORMANCE.md). Reuse reduces measured CPU draw cost; overall latency and displayed FPS require further live comparison. Details now shows latency percentiles and submission-gap bounds, and display replacements include retained render retries.

Version 0.2.1.6 adds [black removal that preserves opaque decoded colors](BLACK-CLEANUP.md). **Remove black only** disables brightness-based edge fading, restores 100% HUD opacity and keeps video independent of Game Bar's pinned opacity. Optional near-black noise removal uses a hard cutoff.

Target: **2560 x 1440 at 240 requested FPS**, Windows 11 x64, wired LAN, black, green, or magenta source background, local mouse/keyboard input. The current milestone accepts measured throughput above 200 FPS with the source display at 244 Hz; finer FPS tuning is deferred. Black transparency and click-through were confirmed. The user clarified that covering the taskbar requires manual dragging; automatic four-edge coverage has not passed.

### Current state

Version 0.2.1.3 adds [Desktop launch identity checks](DESKTOP-LAUNCH.md): Refresh retains the selected application and defaults to Desktop; Connect validates its current ID/name mapping and checks Sunshine's active application after startup. A different active app produces a named conflict instead of being resumed under the Desktop selection. Debug/Release builds and control/core suites pass, and a short live diagnostic launched Desktop and received video. The Release package is installed with Windows status OK. Source command/foreground behavior and installed-widget acceptance still need confirmation.

Version 0.2.1.2 introduced a [cleaner widget menu](WIDGET-MENU.md): Connect, HUD, Layout and Details views, persistent status/connection controls, collapsible stream options and HUD fine tuning, and navigation that adapts to narrow windows. Debug and Release builds have zero warnings/errors; existing controls, action handlers and profile defaults are preserved. It was initially prepared while 0.2.1.1 remained installed and is now included in installed 0.2.1.3. Native appearance and live navigation still need verification.

Version 0.2.1.1 on main adds **Clean black background**, an immediate preset for compression residue and bright text edges, with **Exact black** to restore nonblack dark-detail preservation. Fine sliders show their numeric values, and **Apply key settings** saves/applies changes during streaming. That version preserved saved profiles until an explicit action. Version 0.2.1.6 replaces these presets and migrates older black profiles once. See [black cleanup and live verification](BLACK-CLEANUP.md).

Version 0.2.1.0 adds **Apply video fit**: the entire feed fills the widget's visible area above an adjustable taskbar reservation, allowing vertical compression. The fit follows manual moves/resizes and preserves the negotiated Sunshine stream. It was installed with status OK; the user reported a decent live result and requested cleanup of remaining black-key pixels. Precise taskbar alignment and cross-monitor behavior still need verification. The separate testing branch continues investigating full-monitor host coverage. See [video fitting](VIDEO-FIT.md).

The previously installed 0.2.0.9 baseline had Windows package status OK. Debug and Release builds had zero warnings/errors, and the focused core layout contracts passed in both configurations. Live logs confirmed custom Apply requests reached Game Bar, but 2560x1440 was rejected. Full-screen fit was also declined, as confirmed by the user and two logged `false` responses. Manual adjustment trades the top gap for lost taskbar coverage, according to the user's follow-up; it has not solved simultaneous four-edge coverage. The top gap remains unresolved.

Version 0.2.0.9 adds **Overlay width/height (px)** and **Apply dimensions**. Values are physical pixels, converted using the current display's DPI; they resize the widget separately from the negotiated video resolution. Saved dimensions are not applied automatically on reopening. The latest click supersedes older queued requests. Coverage reporting now shows physical-pixel gaps at all four edges. The supplied 0.2.0.8 screenshot and local geometry show a 46-pixel top gap, with content at Y=46 and size 2558x1394 on a 2560x1440 display; video-local origin is zero, so the missing strip is outside the renderer's client surface.

**Try full-screen fit** explicitly tests Windows' public [ApplicationView full-screen request](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.applicationview.tryenterfullscreenmode). It reports rejection or remaining gaps rather than treating API acceptance as coverage proof. This host declined it. The app's UWP title bar has zero height, is hidden, and is not extended; its own title-bar switch cannot reclaim the external 46-pixel strip. The failed attempt does not also center or resize the widget, preserving placement. Fit, Apply, and Reset exit full-screen mode before their normal resize; Reset restores a smaller centered window. The [Game Bar API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) documents size/centering requests but no arbitrary-position or title-bar-hiding method. A separate borderless presentation window controlled by Game Bar is the proposed next approach, pending the user's architecture choice. See the [placement test](BUILD-LATER.md#taskbar-placement-test).

Version 0.2.0.8 changed **Fit my monitor** into an immediate full-display size request, available while Game Bar is open. Drag and resize controls remain enabled. Opening, closing, or pinning Game Bar no longer initiates a resize, shrinks the widget to the settings-window size, or recenters manual placement. Fit deliberately avoids host centering, which can shift or shrink the surface. Hiding or changing displays cancels stale work. The user's next screenshot exposed the remaining top gap; automatic four-edge coverage has not passed.

The geometry diagnostics first built in 0.2.0.5 are included in 0.2.0.6: widget/client/visible bounds, video-local origin, pin/visibility events, preview key settings, and layout coordinates before and after centering. Version 0.2.0.4's supplied foreground screenshot showed a full-sized surface shifted upward about 44 pixels; the user reported that taskbar auto-hide did not fix it and that the widget could not be moved. The built-in Audio widget can overlap the taskbar in the user's comparison, so this is not treated as a universal Windows restriction.

Version 0.2.0.3 introduced a saved Black background option, exact-black defaults, optional near-black tolerance, and automatic monitor fitting on activation, pinning transitions, and display/DPI changes. Its Debug/Release builds and core/hardware shader contracts passed. On this runtime, Game Bar rejected 2560x1440 and retained 2551x1389 content even when pinned; the user confirmed incomplete coverage.

Use **Remove black only** to keep every nonblack decoded color opaque. **Remove near-black noise** also removes very dark pixels with a hard cutoff. Both presets apply immediately, restore 100% HUD opacity and disable edge blending and following Game Bar opacity for video. Optional fading controls are under Fine tuning. Fine adjustments use **Apply key settings**; **Save profile** also applies key changes during streaming. Existing saved green/magenta profiles retain their indices. Click **Fit my monitor** to request the full monitor size immediately; the previous pinned-coverage checkbox is removed and its stored preference no longer triggers layout changes. Reset preserves pairing and video/key profiles. The settings report actual video dimensions and position rather than treating monitor-sized content as proof of alignment. `tests/source_hud.html` cycles green, black, and magenta and includes corner markers and dark/coloured patches.

Version 0.2 includes Sunshine PIN pairing, protected app-local credentials, authenticated application selection, Moonlight transport, hardware FFmpeg decoding, and a render worker connected through a single-frame mailbox. Release builds without warnings or errors. Six controlled pairing/storage tests pass, including wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. The six core/GPU/decoder tests pass in Debug and Release, including concurrent H.264/HEVC decoding and presentation of inter-coded fixtures.

Version 0.2.0.1 was validated with a correction that serializes DXGI presentation with hardware decoding, plus normalized Game Bar opacity handling. Debug and Release build without warnings/errors. Real Sunshine pairing and on-screen widget video/disconnect were confirmed. Moving H.264 and HEVC fixtures process approximately 240 FPS offscreen for 60 seconds, with alpha checks passing. A subsequent live widget session logged roughly 224–229 received units/s without decoder errors; a 60-second PresentMon trace tracked its swap chain at 225.606 display updates/s. The exact source workload, visible corner/keying checks, and 240 distinct displayed source frames per second remain unverified. See [validation results and measurement limits](VALIDATION.md).

Activation, pinning, the test pattern, and click-through were confirmed in the baseline. Black transparency was later confirmed by the user. The reported full-monitor result required manual dragging, and automatic taskbar coverage remains unverified. Live stream quality and distinct displayed source-frame timing still need controlled evidence. Enter your own Sunshine hostname or IP address; no private host address is shipped as a default. Building does not install or launch the widget; startup does not connect automatically.

Version 0.2.0.2 built in Debug and Release with zero warnings/errors and was installed with Windows package status OK before later updates. Its checkpoint log confirms Game Bar activation, live HEVC 2560x1440 at 240 requested FPS, and normal Disconnect. Seven samples spanning 30.237 seconds measured 231.0–234.6 received/decoded/Present calls per second (sample mean 233.271), with zero decoder errors. The new packet, queue, submission, assembly, host-processing, associated-display, and renderer-adapter diagnostics are present. Logged shutdown completed in 267 ms. These results exceed the accepted interim throughput milestone; visual confirmation and exact distinct displayed source-frame timing remain unverified. Host timing values of zero remain classified as absent/repeated, rather than measured zero latency. See [validation results and measurement limits](VALIDATION.md) for this short check and the earlier baseline recording.

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

### Layout

- `SoftwareFuser.sln` / `native/widget`: Windows UWP project, XAML settings, Game Bar manifest.
- `native/core`: platform-neutral C++20 settings, stream contracts, and decoded-frame mailbox.
- `native/windows`: D3D11 surface ownership and renderer.
- `native/shaders`: fullscreen vertex shader and NV12/chroma-key pixel shader.
- `native/streaming`: Sunshine control, pinned Moonlight build, and session orchestration.
- `tests`: core/GPU/decoder contracts and loopback pairing/credential tests.
- `tools`: repeatable build, explicit deployment, package logos, and read-only Sunshine probe.
- `docs`: architecture, setup, research references, and feasibility evidence.

### Next work

Version 0.2.0.2 established the accepted above-200-FPS live pipeline checkpoint. Black transparency and click-through are confirmed. The user accepts manual adjustment when needed. Check custom dimensions, the full-screen host response, and retention of manual placement in 0.2.0.9 before claiming a coverage fix. Deployment closes the widget and opens a Windows elevation prompt. While gaming, defer deployment and tests that interrupt the user's session or add substantial GPU load.

- Verify repeated fitting, DPI, and monitor changes; earlier resize rejections remain recorded for comparison.
- Validate a controlled live black-background HUD; decoded full/limited-range black, nonblack preservation, and premultiplied soft edges pass GPU tests.
- Defer fine FPS tuning while the measured pipeline remains above 200 FPS.

Read [the architecture](ARCHITECTURE.md), [build and deployment instructions](BUILD-LATER.md), and [the implementation backlog](ROADMAP.md). At each checkpoint, verify and commit the important source/documentation changes, push them to the configured Git remote, and confirm the remote commit matches. Keep local evidence archives and private data excluded.

Generated packages, runtime logs, checkpoints, local machine/network notes, environment files, certificates, and credential files are ignored by Git. Pairing credentials stay in protected app-local storage. Public dependency license notices are retained in `third_party/licenses`.

The installed `cpp-coding-standards` and `verification-before-completion` skills guide source work and evidence reporting. Use `systematic-debugging` when a real failure occurs. `pr-review` is scoped to its Microsoft skills repository and is not automatically applicable to this project. No additional skills are needed for the current foundation; recommend a relevant one if later integration reveals a concrete gap.

## Previous backlog

### Implementation backlog

#### Current milestone and next priorities

- Done: version 0.2.0.2 established the live pipeline checkpoint with Windows package status OK. Its log confirms Game Bar activation, HEVC 1440p/240 setup, 231.0–234.6 received/decoded/Present calls per second, new diagnostic fields, and normal Disconnect in 267 ms. Controlled live source content and broader lifecycle checks remain future validation.
- Accept measured throughput above 200 FPS with the source display at 244 Hz for this milestone. Existing live measurements around 225–226 FPS exceed that threshold; finer FPS tuning and proof of 240 distinct displayed source frames per second are deferred.
- Earlier 0.2.0.3 monitor-size requests were rejected, retaining 2551x1389 content even when pinned. Preserve this evidence when validating repeated fitting, DPI, and monitor changes.
- User confirmed Reset, black transparency, and click-through in 0.2.0.6. Their reported full coverage was later clarified to require manual dragging; automatic taskbar coverage has not passed. Reopening/pinning in 0.2.0.7 repeatedly shrank the widget to 480x700 and retried rejected full-size requests. Previous 0.2.0.4 foreground placement was shifted and immovable; taskbar auto-hide failed in the user's test.
- Previous 0.2.0.7 corrected the coverage-button label to Enable monitor coverage, without layout/rendering changes. Its builds had zero warnings/errors and its installed package status was OK.
- Installed 0.2.0.8 replaces that behavior with an immediate Fit my monitor action, leaves drag/resize enabled, and removes automatic resizing/centering on host transitions. Only Reset centers. Both builds have zero warnings/errors, core layout contracts pass in Debug/Release, and the independent package query reports status OK. Verify actual taskbar coverage and preservation of manual placement. The user accepts manual adjustment when necessary.
- Installed 0.2.0.9 adds typed physical overlay dimensions and Apply, separate from video settings, plus four measured edge gaps. Both builds and focused Debug/Release layout contracts pass. Live 2560x1440 Apply requests and Windows full-screen requests were rejected, with the full-screen failure also confirmed by the user. Content remains at Y=46; the app's own UWP title bar is hidden with zero height, locating the strip in the external Game Bar frame. The user's manual test could cover the top or bottom but not both. A companion borderless renderer controlled by Game Bar is the proposed next approach, pending the user's architecture choice. The top gap remains unresolved.
- Geometry diagnostics first built in 0.2.0.5 are included in 0.2.0.6, with coordinates before and after centering. The user identified the built-in Audio widget as able to overlap the taskbar. Compare its behavior with the custom widget's pinned layout rather than declaring a universal restriction.
- Implemented in 0.2.0.3: Black key selection and saved profiles, exact-black defaults, optional near-black tolerance, and premultiplied transparent output through the existing shader. Hardware tests pass for full/limited-range black, opaque white/colour, preserved dark nonblack pixels, and soft alpha edges. The user accepted the on-screen black-key preview in 0.2.0.6; a controlled live black-background HUD remains to be checked.
- During gaming, defer installation and tests that interrupt streams, change displays, open the widget, or add substantial GPU load. Documentation and Git checkpoints can continue.
- At each checkpoint, verify the tracked changes, check for private data and keys, commit and push, and verify the remote commit. Keep packages, local evidence, and private notes ignored.

#### 1. Validate the native widget baseline

- Inventory both PCs' GPU models, Windows builds, display modes, network link speed, and local target applications.
- Done: restore pinned SDKs and build the UWP/XAML/HLSL package in Debug and Release; the latest source builds without warnings/errors, and core/GPU/decoder contracts pass in both configurations.
- Done: install Release with Windows 11 per-package unsigned-development deployment.
- User confirmed activation, pinning, test pattern, and click-through in the installed baseline. Repeat activation, idle startup, close/suspend, saved profile, edge/opacity behavior, and keyboard input still need detailed checks.
- Verify full-monitor alignment at 100%, 125%, and 150% scaling; record actual widget bounds rather than assuming resize acceptance means full coverage.
- Add DPI-change and device-loss handling where the measured baseline requires it.

#### 2. Establish local presentation feasibility

- Add an explicitly started/stopped diagnostic render worker. Keep it independent of XAML layout, use a bounded render queue, and stop when hidden/closed.
- Draw changing binary frame IDs at requested 60, 120, and 240 FPS. Measure frame submission and actual display timing separately.
- Run idle and normal local-game GPU workloads. Record skipped/repeated frames, timing percentiles, and local-game performance impact.
- A 60 s PresentMon/ETW trace tracks the installed widget's sole swap chain at 225.606 display updates/s. Verify source frame IDs and physical monitor mapping; use external high-speed capture where necessary. Do not infer distinct displayed source frames from Present calls or unverified content.
- If full-screen coverage or 240 FPS cannot be achieved in Game Bar, identify the layer responsible and present evidence before discussing a platform change.

#### 3. Integrate Sunshine/Moonlight transport

- Pin a reviewed revision of moonlight-common-c and its bundled ENet dependency. Record source/license notices before adding vendor code.
- Implemented: HTTP discovery, authenticated/pinned HTTPS, PIN pairing, app-local LOCAL=user credential protection, application enumeration, and active-app-preserving launch/resume. Six controlled tests pass in Debug and Release; real widget pairing, offscreen native playback, and Game Bar HEVC video/disconnect pass. Earlier mostly idle streams measured 63–72 FPS; subsequent live widget measurements reached roughly 224–229 received units/s. A controlled single-stream changing workload remains useful for later FPS tuning.
- Separate pairing UI from Connect. Show actual negotiation and stage-specific failures. Do not reuse another client's private credentials implicitly.
- Implemented: concrete session with cancellable setup, ordered compressed decode units, termination status, and joined transport/render shutdown. It owns Moonlight codec-setup callbacks directly; the portable stream_client interface remains a design boundary. Three live native reconnect cycles in one process pass; Game Bar cancellation/reconnect remains to be validated.
- Request 2560x1440 at 240 FPS, report negotiated values, and never silently label a fallback as the requested profile. Keep audio and input forwarding disabled in the adapter.

#### 4. Integrate GPU decoding and frame orchestration

- Implemented: hardware-only FFmpeg/D3D11VA on the renderer's GPU device; H.264/HEVC sequential and concurrent fixture tests pass in Debug and Release. Moving compressed fixtures process approximately 240 FPS offscreen for 60 seconds per codec; live native decode and joined stop pass, including a two-minute HEVC run. Idle packet telemetry strongly suggests repeated source frames explain most of the lower live receive rate. Isolate the control session and measure the fullscreen animated source before attributing a hardware limit. Sunshine reports a GTX 1070 with H.264/HEVC encoders; the widget UI offers those two codecs.
- Keep compressed reference ordering intact. Handle decoder backpressure and keyframe recovery explicitly; drop stale frames only after decoding.
- Supply color metadata and frame leases. Coordinate context access, handle padded textures, and retire GPU readers before decoder-pool reuse.
- Implemented: session controller joining previous callbacks before transport -> decoder -> mailbox -> renderer is reset/reconnected. Three native cycles reuse the session and renderer with zero decoder errors and joined stop under 174 ms. Validate UI recovery and reconnection under source/monitor faults.
- Implemented: separate receive/decode/present-call rates, mailbox replacements, and decoder-error counters in UI/local logs. The local timestamp begins at decode-callback delivery, not first network-packet arrival. Installed 0.2.0.2 logs frame-assembly, enqueue-to-submission, queue-depth, and host-processing diagnostics in a live stream. Its last sample contains 8,095 decoder submissions, zero skips before the callback, a peak pending queue of three, and zero absent/repeated host-timing samples. Displayed FPS and end-to-end latency still need external evidence.

#### 5. End-to-end acceptance

- Establish a stock Moonlight baseline on the same hardware, profile, and wired connection.
- Compare the widget at 60/120/240 requested FPS with bitrate sweeps appropriate to the HUD; evaluate fine text, thin lines, compressed-key halos, and spill.
- A 30-minute passive recording finished, with 118 fresh observations averaging 225.719 received units/s and zero decoder errors. The stream disconnected before the last three observations; this is not a continuous 30-minute acceptance pass. Its source/local-game workloads were uncontrolled. Still run the representative local-game acceptance workload and test disconnect/reconnect, source restart, resize, DPI, monitor changes, and device removal.
- Use the accepted above-200-FPS throughput milestone for current progress. Later record whether 240 distinct displayed source frames per second and exact monitor coverage are achieved; classify unavailable measurements honestly.
- Done: 0.2.0.2 activation, live pipeline, diagnostic fields, and normal Disconnect have current-process log evidence. Further visual confirmation, reconnection, and representative workload checks remain. Full-monitor coverage and black-pixel transparency remain open features; do not describe them as implemented until their runtime checks pass.
