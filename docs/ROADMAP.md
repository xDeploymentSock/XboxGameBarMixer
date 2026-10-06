# Implementation backlog

## Current milestone and next priorities

- Done: version 0.2.0.2 is installed with Windows package status OK. Its log confirms Game Bar activation, HEVC 1440p/240 setup, 231.0–234.6 received/decoded/Present calls per second, new diagnostic fields, and normal Disconnect in 267 ms. Visual confirmation and broader lifecycle checks remain future validation.
- Accept measured throughput above 200 FPS with the source display at 244 Hz for this milestone. Existing live measurements around 225–226 FPS exceed that threshold; finer FPS tuning and proof of 240 distinct displayed source frames per second are deferred.
- In progress: 0.2.0.3 adds automatic monitor-size requests on activation, pinning transitions, and display/DPI changes, plus actual layout/bounds diagnostics. Game Bar rejects 2560x1440 and retains 2551x1389 content even when pinned. Resolve this host constraint before claiming full-monitor support; verify corner placement and multiple monitors.
- Installed candidate 0.2.0.4 fixes the requested minimum/maximum to the monitor size. Fresh activation reports 2560x1440 content with y=-44; the supplied foreground screenshot corroborates an upward shift and a bottom border above the taskbar. Test taskbar auto-hide and pinning before another sizing change. Disabling fitting restores normal resize limits. Do not count a size match as four-corner acceptance.
- Prepared candidate 0.2.0.5 logs client/visible/video-local geometry and preview key settings, observes pin/visibility changes, and skips fitting while hidden. Both widget builds pass with zero warnings/errors; installation and runtime validation remain pending. Gather placement evidence before applying an offset or another size workaround.
- Implemented in 0.2.0.3: Black key selection and saved profiles, exact-black defaults, optional near-black tolerance, and premultiplied transparent output through the existing shader. Hardware tests pass for full/limited-range black, opaque white/colour, preserved dark nonblack pixels, and soft alpha edges. On-screen acceptance remains pending.
- During gaming, defer installation and tests that interrupt streams, change displays, open the widget, or add substantial GPU load. Documentation and Git checkpoints can continue.
- At each checkpoint, verify the tracked changes, check for private data and keys, commit and push, and verify the remote commit. Keep packages, local evidence, and private notes ignored.

## 1. Validate the native widget baseline

- Inventory both PCs' GPU models, Windows builds, display modes, network link speed, and local target applications.
- Done: restore pinned SDKs and build the UWP/XAML/HLSL package in Debug and Release; the latest source builds without warnings/errors, and core/GPU/decoder contracts pass in both configurations.
- Done: install Release with Windows 11 per-package unsigned-development deployment.
- User confirmed activation, pinning, test pattern, and click-through in the installed baseline. Repeat activation, idle startup, close/suspend, saved profile, edge/opacity behavior, and keyboard input still need detailed checks.
- Verify full-monitor alignment at 100%, 125%, and 150% scaling; record actual widget bounds rather than assuming resize acceptance means full coverage.
- Add DPI-change and device-loss handling where the measured baseline requires it.

## 2. Establish local presentation feasibility

- Add an explicitly started/stopped diagnostic render worker. Keep it independent of XAML layout, use a bounded render queue, and stop when hidden/closed.
- Draw changing binary frame IDs at requested 60, 120, and 240 FPS. Measure frame submission and actual display timing separately.
- Run idle and normal local-game GPU workloads. Record skipped/repeated frames, timing percentiles, and local-game performance impact.
- A 60 s PresentMon/ETW trace tracks the installed widget's sole swap chain at 225.606 display updates/s. Verify source frame IDs and physical monitor mapping; use external high-speed capture where necessary. Do not infer distinct displayed source frames from Present calls or unverified content.
- If full-screen coverage or 240 FPS cannot be achieved in Game Bar, identify the layer responsible and present evidence before discussing a platform change.

## 3. Integrate Sunshine/Moonlight transport

- Pin a reviewed revision of moonlight-common-c and its bundled ENet dependency. Record source/license notices before adding vendor code.
- Implemented: HTTP discovery, authenticated/pinned HTTPS, PIN pairing, app-local LOCAL=user credential protection, application enumeration, and active-app-preserving launch/resume. Six controlled tests pass in Debug and Release; real widget pairing, offscreen native playback, and Game Bar HEVC video/disconnect pass. Earlier mostly idle streams measured 63–72 FPS; subsequent live widget measurements reached roughly 224–229 received units/s. A controlled single-stream changing workload remains useful for later FPS tuning.
- Separate pairing UI from Connect. Show actual negotiation and stage-specific failures. Do not reuse another client's private credentials implicitly.
- Implemented: concrete session with cancellable setup, ordered compressed decode units, termination status, and joined transport/render shutdown. It owns Moonlight codec-setup callbacks directly; the portable stream_client interface remains a design boundary. Three live native reconnect cycles in one process pass; Game Bar cancellation/reconnect remains to be validated.
- Request 2560x1440 at 240 FPS, report negotiated values, and never silently label a fallback as the requested profile. Keep audio and input forwarding disabled in the adapter.

## 4. Integrate GPU decoding and frame orchestration

- Implemented: hardware-only FFmpeg/D3D11VA on the renderer's GPU device; H.264/HEVC sequential and concurrent fixture tests pass in Debug and Release. Moving compressed fixtures process approximately 240 FPS offscreen for 60 seconds per codec; live native decode and joined stop pass, including a two-minute HEVC run. Idle packet telemetry strongly suggests repeated source frames explain most of the lower live receive rate. Isolate the control session and measure the fullscreen animated source before attributing a hardware limit. Sunshine reports a GTX 1070 with H.264/HEVC encoders; the widget UI offers those two codecs.
- Keep compressed reference ordering intact. Handle decoder backpressure and keyframe recovery explicitly; drop stale frames only after decoding.
- Supply color metadata and frame leases. Coordinate context access, handle padded textures, and retire GPU readers before decoder-pool reuse.
- Implemented: session controller joining previous callbacks before transport -> decoder -> mailbox -> renderer is reset/reconnected. Three native cycles reuse the session and renderer with zero decoder errors and joined stop under 174 ms. Validate UI recovery and reconnection under source/monitor faults.
- Implemented: separate receive/decode/present-call rates, mailbox replacements, and decoder-error counters in UI/local logs. The local timestamp begins at decode-callback delivery, not first network-packet arrival. Installed 0.2.0.2 logs frame-assembly, enqueue-to-submission, queue-depth, and host-processing diagnostics in a live stream. Its last sample contains 8,095 decoder submissions, zero skips before the callback, a peak pending queue of three, and zero absent/repeated host-timing samples. Displayed FPS and end-to-end latency still need external evidence.

## 5. End-to-end acceptance

- Establish a stock Moonlight baseline on the same hardware, profile, and wired connection.
- Compare the widget at 60/120/240 requested FPS with bitrate sweeps appropriate to the HUD; evaluate fine text, thin lines, compressed-key halos, and spill.
- A 30-minute passive recording finished, with 118 fresh observations averaging 225.719 received units/s and zero decoder errors. The stream disconnected before the last three observations; this is not a continuous 30-minute acceptance pass. Its source/local-game workloads were uncontrolled. Still run the representative local-game acceptance workload and test disconnect/reconnect, source restart, resize, DPI, monitor changes, and device removal.
- Use the accepted above-200-FPS throughput milestone for current progress. Later record whether 240 distinct displayed source frames per second and exact monitor coverage are achieved; classify unavailable measurements honestly.
- Done: 0.2.0.2 activation, live pipeline, diagnostic fields, and normal Disconnect have current-process log evidence. Further visual confirmation, reconnection, and representative workload checks remain. Full-monitor coverage and black-pixel transparency remain open features; do not describe them as implemented until their runtime checks pass.
