# Validation status

The target is 2560x1440 at 240 requested FPS, composited as a transparent Xbox Game Bar widget. The current milestone accepts measured throughput above 200 FPS with the source display at 244 Hz; existing live measurements exceed that threshold. Fine FPS tuning is deferred. Actual 240 distinct displayed source frames per second, full-monitor coverage, and representative local-game impact remain unverified.

## Completed checks

- Version 0.2.0.3 built in Debug and Release with zero warnings/errors and was installed with Windows package status OK before replacement by 0.2.0.4. Both core/GPU contract suites passed. Black diagnostic and full/limited-range NV12 pixels produced zero premultiplied RGB/alpha; white, coloured, and one-code-value-above-black pixels remained opaque at exact-black settings. Optional noise tolerance and neutral premultiplied soft edges are covered.

- Debug and Release UWP/C++/XAML/HLSL builds completed with zero warnings/errors.
- Six core/GPU/decoder contracts passed in both configurations, including concurrent H.264/HEVC decoding and presentation of owned inter-coded fixtures.
- Six controlled pairing/storage tests passed, covering wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. These fixtures generate temporary keys; no production credentials are committed.
- Real Sunshine pairing, visible Game Bar video, test pattern, pinning, click-through, and normal Disconnect were confirmed in the installed baseline.
- Three native reconnect cycles reused one controller and renderer with zero decoder errors and joined stop under 174 ms.
- Version 0.2.0.2 is installed with Windows package status OK. Its log confirms Game Bar activation, pinned/click-through state, live HEVC decoding/presentation, all new diagnostic groups, and normal Disconnect. A read-only authenticated application query also verified that the widget's own saved pairing remained usable after the update.

DXGI Present and hardware decoding share the renderer's recursive immediate-context mutex. This corrected a reproduced HEVC hang/access violation caused by concurrent immediate-context/DXGI access. The test suite checks sequential and concurrent production decode/render paths.

## Measurements

| Workload | Measurement | Scope |
| --- | --- | --- |
| Moving compressed HEVC HUD, 14,400 decode inputs | 239.950 decoded and Present calls/s; zero mailbox replacements and unavailable GPU slots | Approximately 60 s, receiver only, offscreen |
| Moving compressed H.264 HUD, 14,400 decode inputs | 239.945 decoded/s and 239.928 Present calls/s; zero mailbox replacements, one unavailable GPU slot | Approximately 60 s, receiver only, offscreen |
| Installed live HEVC widget | Approximately 224–229 received units/s, zero decoder errors | Uncontrolled source workload |
| 0.2.0.2 live Game Bar HEVC, 2560x1440 at 240 requested FPS | 231.0–234.6 received/decoded/Present calls per second; sample mean 233.271; zero decoder errors | Seven current-process samples spanning 30.237 s; uncontrolled source workload |
| 0.2.0.2 normal Disconnect | 267 ms from logged stop start to Disconnected | Joined shutdown and subsequent responsive widget; no visible UI confirmation supplied |
| 60 s PresentMon trace | 225.606 ETW display updates/s; p95 time inside Present 0.0622 ms | Captured widget swap chain, Composed: Flip |
| Passive 30-minute recording | 118 fresh observations, mean receive rate 225.719/s, zero decoder errors, seven additional mailbox replacements | Last three observations followed a disconnect and were excluded |

Both moving-fixture benchmarks passed transparent-green and opaque-white pixel checks after joined shutdown. Offscreen results exclude source capture/encoding, network transport, Game Bar, monitor scanout, and local-game GPU load.

Periodic app rate samples are not complete frame counts. Present calls are not monitor scanout or capture-to-photon latency. ETW display timestamps do not prove distinct source pixels or optical timing. Display source IDs are per-adapter; a receiver with source ID 0 on two GPUs requires adapter/display correlation before associating a trace with a monitor. Resource endpoints after disconnect do not prove absence of leaks.

Earlier mostly idle source streams had near-total zero host-processing fields. Moonlight defines those values as absent data or repeated frames, and the inspected Sunshine revision omits captured-frame timestamps for duplicates. This suggests idle repetitions contributed to the low observed rate; it does not establish a hardware ceiling or measured zero encoder latency.

The last 0.2.0.2 stream sample contains 8,095 decoder submissions and receiver/host timing samples, zero skipped frame indexes before the callback, zero absent/repeated host timing samples, a peak pending decode queue of three, and four replaced display frames. Associated-display and renderer-adapter diagnostics were logged. These fields validate the new instrumentation, without establishing capture-to-photon timing or distinct displayed source pixels.

A separate three-minute passive observer finished successfully with 37 observations. Six fresh connected samples averaged 233.05 received/decoded/Present calls per second. Its other 31 observations retained an older baseline rate or followed Disconnect and were excluded. This is a short live check within a bounded observation window, not a continuous three-minute stream test.

## Remaining acceptance

The installed 0.2.0.4 candidate sets minimum and maximum content dimensions to the monitor size while fitting is enabled. Both widget builds have zero warnings/errors, and Windows package status is OK. After a fresh activation, the runtime reports 2560x1440 content and bounds x=0, y=-44. In the user's supplied 2559x1439 foreground screenshot, the horizontal white center line is at rows 676–677 and the bottom border at rows 1394–1396. The cyan test rectangle is deliberately inset to normalized coordinates 0.2–0.8; its horizontal edges appear at rows 242–247 and 1106–1111. Together these measurements are consistent with an unscaled 1440-pixel surface shifted upward about 44 pixels. The visible taskbar occupies the bottom strip. This establishes incomplete foreground placement, not its precise host cause or pinned behavior. Current logs show the widget remains unpinned. Test taskbar auto-hide and pinned placement separately before selecting another sizing change. Disabling fitting restores the previous resizable limits.

Version 0.2.0.5 is prepared, with zero warnings/errors in both widget configurations, but has no installation or runtime result. It adds deduplicated geometry logs for the widget, client, visible area, and video-local origin, plus preview key settings. Pin/visibility events are observed, and hidden widgets skip monitor-fit requests. The foreground screenshot corroborates the negative origin's effect on the test surface; the available pinned content area and clipping still require runtime geometry and the four-edge visual check.

Version 0.2.0.2's x64 package publisher matches the previous installation, and its Windows minimum version and VCLibs runtime requirement were verified before deployment. Current-process logs verify activation, the live pipeline, normal Disconnect, and the new diagnostic fields. Further visual confirmation and broader lifecycle checks remain future validation. Defer deployment and disruptive runtime/performance tests during gaming.

Version 0.2.0.3 exposes green, magenta, and black keys. GPU black-alpha contracts pass; visible black-key and click-through acceptance is pending. Automatic fitting is implemented, but current-process logs show rejected 2560x1440 resize requests with 2551x1389 content at scale 1, including two attempts in pinned mode. Full-monitor coverage is incomplete. The SDK exposes sizing and centering requests, whose documented bounds rules can resize or reposition content. An open [resize-limit report](https://github.com/microsoft/XboxGameBarSamples/issues/141) and [fullscreen feature request](https://github.com/microsoft/XboxGameBarSamples/issues/111) describe related limitations; they do not establish a workaround on this runtime.

For later FPS tuning, use the fullscreen owned source HUD to establish animation rate and changing frame IDs, isolate one streaming client, inspect keying/corner alignment, and record the instrumented widget's receive/decode/Present, packet-gap, queue, and timing counters. Compare stock Moonlight on the same profile and PCs. Then test representative local-game load, Game Bar lifecycle/reconnect, source restart, DPI/monitor changes, and device removal.

Raw runtime logs, private machine/network notes, and checkpoint archives remain local and are ignored by Git. Build and diagnostic tools reproduce the checks described in BUILD-LATER.md.
