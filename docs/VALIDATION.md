# Validation status

The target is 2560x1440 at 240 requested FPS, composited as a transparent Xbox Game Bar widget. The current milestone accepts measured throughput above 200 FPS with the source display at 244 Hz; existing live measurements exceed that threshold. Fine FPS tuning is deferred. Actual 240 distinct displayed source frames per second, full-monitor coverage, and representative local-game impact remain unverified.

## Completed checks

- Debug and Release UWP/C++/XAML/HLSL builds completed with zero warnings/errors.
- Six core/GPU/decoder contracts passed in both configurations, including concurrent H.264/HEVC decoding and presentation of owned inter-coded fixtures.
- Six controlled pairing/storage tests passed, covering wrong PINs, forged signatures, TLS public-key changes, forbidden XML DOCTYPEs, and encrypted credential round-trips. These fixtures generate temporary keys; no production credentials are committed.
- Real Sunshine pairing, visible Game Bar video, test pattern, pinning, click-through, and normal Disconnect were confirmed in the installed baseline.
- Three native reconnect cycles reused one controller and renderer with zero decoder errors and joined stop under 174 ms.

DXGI Present and hardware decoding share the renderer's recursive immediate-context mutex. This corrected a reproduced HEVC hang/access violation caused by concurrent immediate-context/DXGI access. The test suite checks sequential and concurrent production decode/render paths.

## Measurements

| Workload | Measurement | Scope |
| --- | --- | --- |
| Moving compressed HEVC HUD, 14,400 decode inputs | 239.950 decoded and Present calls/s; zero mailbox replacements and unavailable GPU slots | Approximately 60 s, receiver only, offscreen |
| Moving compressed H.264 HUD, 14,400 decode inputs | 239.945 decoded/s and 239.928 Present calls/s; zero mailbox replacements, one unavailable GPU slot | Approximately 60 s, receiver only, offscreen |
| Installed live HEVC widget | Approximately 224–229 received units/s, zero decoder errors | Uncontrolled source workload |
| 60 s PresentMon trace | 225.606 ETW display updates/s; p95 time inside Present 0.0622 ms | Captured widget swap chain, Composed: Flip |
| Passive 30-minute recording | 118 fresh observations, mean receive rate 225.719/s, zero decoder errors, seven additional mailbox replacements | Last three observations followed a disconnect and were excluded |

Both moving-fixture benchmarks passed transparent-green and opaque-white pixel checks after joined shutdown. Offscreen results exclude source capture/encoding, network transport, Game Bar, monitor scanout, and local-game GPU load.

Periodic app rate samples are not complete frame counts. Present calls are not monitor scanout or capture-to-photon latency. ETW display timestamps do not prove distinct source pixels or optical timing. Display source IDs are per-adapter; a receiver with source ID 0 on two GPUs requires adapter/display correlation before associating a trace with a monitor. Resource endpoints after disconnect do not prove absence of leaks.

Earlier mostly idle source streams had near-total zero host-processing fields. Moonlight defines those values as absent data or repeated frames, and the inspected Sunshine revision omits captured-frame timestamps for duplicates. This suggests idle repetitions contributed to the low observed rate; it does not establish a hardware ceiling or measured zero encoder latency.

## Remaining acceptance

Version 0.2.0.2 is installed with Windows package status OK. Its x64 package publisher matches the previous installation, and its Windows minimum version and VCLibs runtime requirement were verified before deployment. Confirm Game Bar activation, visible video, normal disconnect, and its new diagnostic fields during an idle interval. Defer deployment and disruptive runtime/performance tests during gaming.

Full-monitor overlay support and black-pixel transparency are now explicit backlog items. Current shader key modes are green and magenta; transparent-black behavior has not been implemented or validated.

For later FPS tuning, use the fullscreen owned source HUD to establish animation rate and changing frame IDs, isolate one streaming client, inspect keying/corner alignment, and record the instrumented widget's receive/decode/Present, packet-gap, queue, and timing counters. Compare stock Moonlight on the same profile and PCs. Then test representative local-game load, Game Bar lifecycle/reconnect, source restart, DPI/monitor changes, and device removal.

Raw runtime logs, private machine/network notes, and checkpoint archives remain local and are ignored by Git. Build and diagnostic tools reproduce the checks described in BUILD-LATER.md.
