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

## Current optimization pass (0.2.1.8 candidate)

The decoder now allocates one input-packet wrapper and one receive-frame wrapper per session. Every displayed output still owns a distinct AVFrame lease. Moving references from the receive wrapper into that lease avoids cloning its hardware-buffer and timing references; a scope guard releases each input packet's data even after a callback or decode error. The shared D3D11 context lock, compressed-byte copy, ordered decoder input, color conversion, and GPU completion leases are preserved.

The benchmark's optional `decode-only` mode removes concurrent drawing and presentation from the timed input loop. It reports wrapper counts and submit-call percentiles after the first 20 inputs, alongside full-run timings. Submit elapsed time includes driver blocking and is not CPU processor time.

Three unpaced Release runs per codec and implementation decoded the same 6,000 owned inter-coded 1440p inputs. All six baseline runs and all six updated runs completed with transparent-green, opaque-white and post-stop lease checks. The table reports the median of each run-level metric:

| Metric | H.264 before | H.264 after | HEVC before | HEVC after |
| --- | ---: | ---: | ---: | ---: |
| Packet / receive wrapper allocations | 6,000 / 6,001 | 1 / 1 | 6,000 / 6,001 | 1 / 1 |
| Retained output wrappers | 6,000 | 6,000 | 6,000 | 6,000 |
| Steady submit elapsed p50 | 0.0388 ms | 0.0364 ms | 0.0588 ms | 0.0579 ms |
| Steady submit elapsed p95 | 0.1710 ms | 0.1452 ms | 0.1981 ms | 0.2020 ms |
| Steady mean submit elapsed | 0.6170 ms | 0.6164 ms | 0.6865 ms | 0.6873 ms |
| Decoded inputs/s | 1,613.8 | 1,615.4 | 1,450.3 | 1,448.9 |

The allocator reduction is established; throughput is essentially unchanged and the elapsed-time changes do not prove a user-visible latency improvement. Unpaced p99 remains approximately 15 ms under decoder saturation. Its cause needs profiling before changing timers or synchronization. These runs used the same local fixtures and preserved the other running applications; they were not a controlled gaming-load test.

One additional concurrent-present comparison per codec used 1,200 inputs paced at 240 FPS. Before/after accepted Present rates were 239.765/239.566 per second for H.264 and 239.772/239.759 for HEVC. Feed-to-accepted-Present p95 was 0.2556/0.2443 ms and 0.2564/0.2476 ms respectively, with zero GPU-slot retries. These small sequential differences do not establish a latency gain. Full-run maximum submit times near 20 ms include hardware startup; the tool separately reports post-startup samples.

Decoder regressions require session-level wrapper reuse, output-specific leases, correct sequence/timestamp propagation, failure cleanup, and successful stop/reinitialize after a throwing callback. Existing sequential and concurrent H.264/HEVC tests exercise the unchanged color/alpha path and output that survives decoder shutdown.

Reproduce a decode-only run after building the native decoder tests and preparing their runtime PATH:

```text
fuser_fixture_benchmark build/fixtures/inter-pattern.hevc hevc 0 6000 decode-only
```

Omit `decode-only` for the concurrent-present path. Use 240 instead of 0 for paced input. These tools never connect to Sunshine. FFmpeg documents refcounted input ownership and receiving into reusable frame wrappers in its [send/receive API](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html); [av_frame_move_ref](https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html) transfers references and resets the source.

### GPU draw timing

The developer `fuser_gpu_draw_benchmark` uses an owned 2560x1440 NV12 pattern, the production renderer, Crisp scaling and three key presets. D3D11 timestamps bracket draw commands after 50 warmups; query waits, flushes and alpha readback belong only to this diagnostic. Results are accepted only when the timestamp-disjoint flag is false and the frequency is nonzero, as required by [Microsoft's timestamp query documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_query_data_timestamp_disjoint).

One Release run at the currently logged fitted dimensions, 2556x1396, used 2,000 samples per preset:

| Preset | GPU draw p50 | p95 | p99 | Mean |
| --- | ---: | ---: | ---: | ---: |
| Exact black | 0.022624 ms | 0.022688 ms | 0.022816 ms | 0.022622 ms |
| Near-black cutoff | 0.022720 ms | 0.023424 ms | 0.023456 ms | 0.022977 ms |
| Default green | 0.023392 ms | 0.023456 ms | 0.023520 ms | 0.023383 ms |

A native 2560x1440 output run measured means of 0.023241, 0.023802 and 0.023989 ms respectively. Exact-black alpha, opaque white and near-black cutoff checks passed at both sizes. GPU draw commands cost roughly 0.023 ms on this receiver in isolation; these samples exclude decoding, CPU/context-lock waits, Present, Game Bar composition, scanout and gaming load. The repeated simple pattern is not a comprehensive shader workload.

An isolated shader experiment replaced the hard-cutoff Euclidean distance with a squared-distance comparison while retaining the soft-key path. Three paired 2560x1394 runs measured exact-black median means of 0.022620 ms before and 0.023329 ms after; the other two presets also became about 0.0007 ms slower. The candidate was rejected and the production shader is unchanged. Removing source-level operations does not establish a GPU improvement without measurement.

Reproduce after the GPU build:

```text
fuser_gpu_draw_benchmark 2556 1396 2000
```

Output dimensions and sample count are explicit arguments. Hardware GPU CTest includes a short timestamp/alpha smoke run; comprehensive color, scaling and lease checks remain in the existing shader contracts.

### Compressed-input preparation

The fixture benchmark now reports access-unit size and input allocation/copy percentiles after 20 startup units, separately from decoder submit time. Optional `GenerateDecoderFixtures.ps1 -IncludePayloadStress` creates owned high-motion H.264/HEVC input at 2560x1440, 240 FPS and 100 Mbps CBR, with green/white alpha anchors. This is a packet-size stress workload, not a source-HUD visual-quality comparison.

Three unpaced HEVC runs of 1,200 units averaged 52,239 bytes per unit (maximum 86,647). Input preparation p95 was 0.0044, 0.0044 and 0.0048 ms; p99 was 0.0058, 0.0059 and 0.0071 ms. One H.264 run averaged 52,241 bytes (maximum 82,327), with preparation p95/p99 of 0.0044/0.0070 ms. All decoded outputs and post-stop alpha checks passed.

A separate three-run synthetic probe reproduced fragment copying in 1,392-byte chunks followed by `av_new_packet`, packet-data copying, timing allocation and unref. It rotated 64 owned payloads per size, with 20,000 samples after 100 warmups. At 52,084 bytes (the average payload budget for 100 Mbps/240 FPS), median run-level p95 was 1.3 microseconds for assembly and 1.0 microseconds for packet preparation/free. At 1 MiB, those p95 values increased to 256.2 and 252.1 microseconds. These synthetic bytes were not submitted to the decoder; results exclude its driver waits and do not establish the live HUD's size distribution. The current copy/ownership path is preserved because the tested typical-size costs are small and no safe ownership rewrite has demonstrated a pipeline gain.

One paced HEVC stress run decoded 1,200 units in 4.999 seconds (240.055/s), with input preparation p95 of 0.0069 ms. Accepted Present calls were only 118.827/s, with 605 display replacements and no occupied-GPU-slot retries. Other streams were active and the swap chain was offscreen. This is evidence that the presentation result varies under concurrent load, not an isolated widget regression or a 120 Hz display limit. It does not supersede the earlier idle-system paced results.

A constant-upload cache was also tested in an isolated renderer copy, preserving input, shader and alpha checks while using changing frame IDs. Six sequential fitted-size runs (three per implementation, 2,000 samples per key preset) gave inconsistent CPU/GPU differences while other streams ran. It was not adopted; production still uploads the current constants on every draw. An attempted 40-second passive PresentMon capture lost ETW events and produced no CSV, so it supplies no reliable display measurements.

Reproduce the owned-payload measurements after building Decoder:

```powershell
.\tools\GenerateDecoderFixtures.ps1 -IncludePayloadStress
$env:PATH = (Join-Path (Get-Location) 'build/test-runtime/Release') + ';' + $env:PATH
.\build\decoder\Release\fuser_fixture_benchmark.exe build/fixtures/payload-pattern.hevc hevc 0 1200 decode-only
.\build\decoder\Release\fuser_fixture_benchmark.exe build/fixtures/payload-pattern.h264 h264 0 1200 decode-only
```

Supply `-FfmpegPath` when FFmpeg is not on PATH. Replace 0 with 240 and omit `decode-only` for the concurrent path. Keep comparisons sequential and record other active streams.

### Live trace collection limitation

A follow-up 20-second capture disabled GPU and input tracking while retaining display tracking. It still lost 634,740 ETW events and produced no CSV. A 10-second Present-only capture, additionally disabling display tracking, lost 6,332 events and produced no CSV. A five-second Present-only check against the existing Moonlight window also lost 2,815 events and produced no CSV. All tools exited normally; exit code zero alone does not prove valid measurement data.

The executing account was not elevated, was a Performance Log Users member, and matched the widget process owner. Those checks do not establish why events were lost or that elevation will fix collection. No privilege/group settings or running sessions were changed. Further repeated captures with this setup are deferred until the collection path can be validated in the controlled test window.

[PresentMon's versioned console documentation](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md) defines the tracking switches and CSV metrics. Disabling display tracking removes display-duration/latency measurements, so a successful Present-only capture would still be insufficient to establish displayed source FPS or capture-to-screen latency. Missing CSV data from these failed captures is not evidence that the widget did not present frames; its own receive/decode/Present counters remained active.

### Controlled live comparison

The candidate package is prepared; installation and this comparison require an idle test window. Record the installed 0.2.1.7 baseline before updating it. Keep source content, dimensions, codec, bitrate, key/scaling settings and widget placement identical across the two versions.

1. Put the [owned HUD fixture](BUILD-LATER.md#source-animation-fixture) fullscreen on the source's captured display, select its black background and leave motion running. Confirm the canvas is 2560x1440 and the source animation counter is near the display refresh rate. Its browser counter is not received or displayed FPS.
2. Disconnect the separate Moonlight control stream after the HUD is ready. Connect only Software Fuser using the 1440p HUD preset: HEVC, 240 requested FPS and 100,000 kbps. Use Remove black only and Crisp HUD. Pin it, enable click-through and close Game Bar.
3. Observe for at least 60 seconds after startup. Save begin/end receiver statistics and record changing frame IDs, receive/decode/Present rates, display replacements, decode errors, source processing, assembly/queue timing, callback-to-Present percentiles and pacing gaps. Discard a trace with lost events. Accepted Present calls alone do not establish distinct displayed frames or capture-to-screen latency.
4. After the baseline, install the prepared Release candidate in the idle window and repeat the same run. Then repeat under representative local-game load, recording game FPS/frame-time impact and HUD color/edge appearance. Run comparisons sequentially; other streaming sessions confound source and receiver load.
5. Verify Disconnect, reconnect and application selection, then restore the source-control session. Keep logs and captures local; publish only reviewed aggregate results.

### Remaining performance verification

Maximum performance remains unproven. Continue the review against these gates instead of treating reduced allocation counts as completion:

| Area | Required evidence |
| --- | --- |
| Compressed input handling | Owned 100 Mbps and synthetic size probes are recorded above. Still verify the actual HUD access-unit distribution and any proposed ownership change against isolated pipeline timings. |
| Shader and GPU reads | Isolated fitted-size timestamp/alpha checks are recorded above. Still compare representative HUD textures and gaming GPU load while preserving color/alpha/crop regressions. |
| Presentation pacing | Controlled Game Bar trace with changing source frame IDs; distinguish accepted Present calls from display updates and source content. |
| Source and transport | Isolated stream with a moving HUD, encoder processing/queue/assembly counters and otherwise identical codec settings. |
| Gaming impact and stability | Representative local-game load, reconnect and shutdown measurements; compare the verified package against the previous checkpoint. |

## Timer and lock review

The normal render path wakes on decoded-frame notifications and the DXGI waitable presentation object. It does not sleep for a fixed 4 ms. The 1 ms condition timeout remains only when all GPU read slots are occupied; Windows scheduling can make that timeout longer. A global timer-resolution change cannot correct source encoding or display composition delays, and none is added here.

The live comparison recorded no GPU-slot retries, so the 1 ms fallback was not on that measured path. Remaining output-lease allocations and the encoded-byte copy remain candidates for later profiling, but measured receiver averages near 0.25 ms give little evidence that rewriting their ownership or removing the shared context lock would materially improve this workload.

`auto` is compile-time type deduction. Local scalar declarations do not inherently allocate memory or slow a loop. The condition-variable mutex is released while waiting; publication under that same mutex prevents lost wakeups. D3D11's shared immediate context still requires serialization with FFmpeg's hardware callbacks. A stop check after waking remains necessary because shutdown can arrive during the wait. Relevant primary references are [C++ type deduction](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp), [Windows timer resolution](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod), [D3D11 multithreading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro), and [DXGI waitable swap chains](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains).
