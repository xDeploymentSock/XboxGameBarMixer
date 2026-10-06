# Receiver performance review

Version 0.2.1.5 reuses the renderer's NV12 shader-resource views and GPU completion queries. Previously every drawn video frame created two views and one query while holding the context lock shared with the hardware decoder. The new cache keeps one view pair per decoder-pool slice, retains only one texture allocation, and replaces it when the decoder pool changes. Clear releases cached views. Three completion queries are reused only after the corresponding GPU reads finish; decoder frame leases still protect those reads.

The shader and sampling path are unchanged. Hardware tests alternate black and white decoder-array slices in both Crisp and Smooth modes, verify alpha and slice selection, and check resource reuse and restoration after Clear. Pool views retain texture allocations, not decoder-pool frame leases.

## Measurements

One before/after Release comparison decoded the same owned 2560 x 1440 HEVC fixture, with 2,400 inputs paced at 240 FPS. The installed widget was also streaming during both runs. These offscreen measurements exclude Sunshine, transport, Game Bar composition and scanout.

| Metric | Before | After |
| --- | ---: | ---: |
| Draw CPU time including context-lock wait, p95 | 0.0377 ms | 0.0154 ms |
| Created NV12 views | 4,634 | 12 |
| Created completion queries | 2,317 | 3 |
| Fixture feed to accepted Present return, p95 | 2.8500 ms | 2.8355 ms |
| Fixture feed to accepted Present return, p99 | 3.7832 ms | 3.8529 ms |
| Accepted Present calls/s | 231.75 | 233.27 |

Resource churn and CPU draw cost decreased. Overall latency was essentially unchanged in this comparison; the small FPS difference does not establish a throughput improvement beyond run variation.

A ten-second PresentMon recording of the installed 0.2.1.4 widget saw 2,317 Present calls over approximately 9.94 seconds. Present intervals had p50/p95/p99 values of 4.24/4.46/5.83 ms. Only 677 presents had tracked display timestamps, approximately 68/s, with 10.55 ms median Present-to-display time. A second eight-second recording showed similar results. The receiver had two active displays on separate graphics adapters at different refresh rates. Widget placement, Game Bar visibility, source motion and ETW interpretation require controlled live verification before attributing this to a specific host or display limitation. Accepted Present calls do not prove distinct displayed frames.

## Better diagnostics

Display replacements now include a newer decoded frame replacing a retained render retry. Previously this replacement was invisible in the mailbox counter. Only decoded display frames are replaced; encoded reference frames remain ordered.

Details and the private runtime log include callback-to-accepted-Present p95/p99 bounds, accepted-Present interval p95/p99 bounds, GPU-read-slot retries and Present retries. The log also records maximum gaps, wait timeouts and retained-frame replacements. Bounded histograms use 250 microsecond buckets with no per-frame allocation or lock. Percentiles are upper bounds; samples above 64 ms use the observed maximum. Live snapshots are approximate and cumulative from Connect, so reconnect between comparisons. Intervals measure accepted submissions, not scanout.

The developer diagnostic accepts an optional bitrate:

```text
fuser_sunshine_diagnostics <own LocalState> <host> stream <Desktop app ID> hevc 240 8 1 100000
```

An eight-second live HEVC/100 Mbps run decoded 1,803 frames with no errors, averaging 6.37 ms of host-reported processing and 0.75 ms locally from callback to accepted Present. The corresponding H.264 run decoded 1,867 frames with no errors, averaging 5.44 ms of host processing and 0.56 ms locally. Both ran alongside the widget and a separate source-control stream, and both stopped normally. These short sequential comparisons suggest testing H.264 for latency on this source; they do not establish a quality improvement or isolate encoder capacity.

After installing the update, with the widget closed and only the source-control stream preserved, two eight-second reconnect cycles per codec gave:

| Metric | HEVC / 100 Mbps | H.264 / 100 Mbps |
| --- | ---: | ---: |
| Host nonzero processing average | 5.96–6.00 ms | 3.45–3.49 ms |
| Receiver callback-to-accepted-Present average | 0.247–0.258 ms | 0.247–0.256 ms |
| Receiver latency p99 upper bound | 0.50 ms | 0.50 ms |
| Accepted-Present interval p99 upper bound | 5.00–5.75 ms | 7.50–7.75 ms |
| Decoder errors | 0 | 0 |
| GPU-slot retries / Present retries / wait timeouts | 0 / 0 / 0 | 0 / 0 / 0 |

All four cycles completed and joined normally, including reuse of the renderer and Clear between reconnects. H.264 reduced host-reported processing by about 2.5 ms on this source, while HEVC had tighter p99 submission intervals in these runs. The receiver averages were similar. The first second includes startup; raw frame totals over eight seconds are not steady-state FPS. Source motion, visual quality and displayed frame timing were not controlled by these offscreen tests.

## Live comparison

1. Use a continuously moving owned source HUD. Place Software Fuser on the intended main monitor, pin it and close Game Bar. Reconnect before each sample.
2. Start with the 1440p HUD preset: HEVC, 240 requested FPS, 100,000 kbps and Crisp HUD. Compare an otherwise identical H.264 run by changing Codec under Stream options, then reconnecting. Judge source text, moving edges and motion as well as Details timing.
3. If source access permits, compare with the separate Moonlight control stream disconnected. Software Fuser preserves it automatically; disconnect it yourself only when you can still control the source.
4. For quality, compare HEVC at 100,000 and 120,000 kbps with all other settings unchanged. More bitrate may reduce compression artifacts but does not remove the NV12 4:2:0 limit or the rescaling needed above the taskbar. No unmeasured higher-bitrate quality improvement is claimed.
5. Record a passive, process-filtered PresentMon trace during each stable run. Compare tracked display timestamps with accepted-Present intervals; keep the other display, foreground app and GPU load consistent. End-to-end optical latency requires a separate visual measurement.

Keep Sunshine NVENC P1 initially. Higher presets increase encoding latency in exchange for compression efficiency. Quarter-resolution two-pass is the documented default; one-pass is an optional controlled experiment because bitrate overshoot can cause packet loss. These source settings are not modified by this release. See [Sunshine's NVENC configuration](https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2configuration.html#nvenc_preset).

## Timer and lock review

The normal render path wakes on decoded-frame notifications and the DXGI waitable presentation object. It does not sleep for a fixed 4 ms. The 1 ms condition timeout remains only when all GPU read slots are occupied; Windows scheduling can make that timeout longer. A global timer-resolution change cannot correct source encoding or display composition delays, and none is added here.

The live comparison recorded no GPU-slot retries, so the 1 ms fallback was not on that measured path. Additional decoder allocations and the encoded-byte copy remain candidates for later profiling, but measured receiver averages near 0.25 ms give little evidence that rewriting their ownership or removing the shared context lock would materially improve this workload.

`auto` is compile-time type deduction. Local scalar declarations do not inherently allocate memory or slow a loop. The condition-variable mutex is released while waiting; publication under that same mutex prevents lost wakeups. D3D11's shared immediate context still requires serialization with FFmpeg's hardware callbacks. A stop check after waking remains necessary because shutdown can arrive during the wait. Relevant primary references are [C++ type deduction](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp), [Windows timer resolution](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod), [D3D11 multithreading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro), and [DXGI waitable swap chains](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains).
