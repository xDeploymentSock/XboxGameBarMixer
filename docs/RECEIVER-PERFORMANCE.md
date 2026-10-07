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
| Host nonzero processing average | 5.96â€“6.00 ms | 3.45â€“3.49 ms |
| Receiver callback-to-accepted-Present average | 0.247â€“0.258 ms | 0.247â€“0.256 ms |
| Receiver latency p99 upper bound | 0.50 ms | 0.50 ms |
| Accepted-Present interval p99 upper bound | 5.00â€“5.75 ms | 7.50â€“7.75 ms |
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

### Owned collector control

A subsequent 10-second Present-only capture targeted an active owned HEVC fixture benchmark. Its 12-second run decoded all 2,880 frames and accepted 2,877 Present calls (239.820/s), with three mailbox replacements, zero occupied-GPU-slot retries and passing post-shutdown alpha checks. Feed-to-Present return p95/p99 were 0.2348/0.2819 ms. The collector still lost 5,446 ETW events and produced no CSV, although both native processes exited successfully.

An elevated control run produced comparable benchmark output: 2,880 decoded frames, 2,877 Present calls (239.816/s), zero occupied-GPU-slot retries and feed-to-Present p95/p99 of 0.2298/0.2780 ms. The collector reported 5,451 lost events and no CSV. That run's PowerShell helper failed to retain its child exit codes, so it is not a verified clean-exit capture. A separate bounded Windows PowerShell check confirmed that opening the native process handle before waiting preserves the exit code; the private helper was corrected for future collection. Elevation did not resolve the observed missing-data/lost-event result.

These offscreen controls establish an active producer for the collection check, not on-screen source content or Game Bar scanout. They do not measure capture-to-screen latency or establish a version-to-version improvement. The separate Moonlight control process remained running. A valid on-screen collection check and the isolated widget/source setup are still required before reporting display pacing.

### Controlled live comparison

Release 0.2.1.8 is installed and independently verified. A controlled 0.2.1.7 live baseline was not collected before the authorized update, so the next isolated run establishes the installed version's behavior without proving a before/after improvement. Any later comparison requires a verified baseline package and the same source content, dimensions, codec, bitrate, key/scaling settings and widget placement. This protocol requires an idle test window.

1. Put the [owned HUD fixture](BUILD-LATER.md#source-animation-fixture) fullscreen on the source's captured display, select its black background and leave motion running. Confirm the canvas is 2560x1440 and the source animation counter is near the display refresh rate. Its browser counter is not received or displayed FPS.
2. Disconnect the separate Moonlight control stream after the HUD is ready. Connect only Software Fuser using the 1440p HUD preset: HEVC, 240 requested FPS and 100,000 kbps. Use Crisp HUD and keep the chosen black-removal preset unchanged between samples; the installed baseline below uses Remove near-black noise. Pin it, enable click-through and close Game Bar.
3. Observe for at least 60 seconds after startup. Save begin/end receiver statistics and record changing frame IDs, receive/decode/Present rates, display replacements, decode errors, source processing, assembly/queue timing, callback-to-Present percentiles and pacing gaps. Discard a trace with lost events. Accepted Present calls alone do not establish distinct displayed frames or capture-to-screen latency.
4. Repeat the installed-version run under representative local-game load, recording game FPS/frame-time impact and HUD color/edge appearance. If a verified baseline becomes available, compare versions sequentially under the same conditions. Other streaming sessions confound source and receiver load.
5. Verify Disconnect, reconnect and application selection, then restore the source-control session. Keep logs and captures local; publish only reviewed aggregate results.

### Installed live HUD baseline (0.2.1.8)

The user started the owned HUD test and preferred **Remove near-black noise**, reporting that exact-black removal still left visible dark pixels. This baseline uses that explicit preference rather than requiring exact-black removal. Private runtime settings confirm tolerance 0.12, softness 0, HUD opacity 1, Crisp scaling and independent video opacity 1 while pinned with click-through. Retained colors remain opaque; the cutoff also removes intentionally near-black artwork. This qualitative preference is not a pixel-exact source-color comparison.

A passive observation captured a 75.606-second interval after startup from the installed HEVC widget at 2560x1440 and 240 requested FPS. Differences between beginning/ending cumulative counters give:

| Metric | Measured result |
| --- | ---: |
| Received units / accepted-Present samples | 17,238 / 17,238 |
| Received units / accepted-Present samples per second | 227.998 / 227.998 |
| Host-reported nonzero processing average | 6.473 ms |
| Decoder submit average | 0.114 ms |
| Callback-to-accepted-Present average | 0.172 ms |
| Assembly / enqueue-to-submission average | 0.0048 / 0.0077 ms |
| Added decode errors / skipped frame indexes | 0 / 0 |
| Added display / retained-retry replacements | 0 / 0 |
| Added GPU-slot / Present retries / wait timeouts | 0 / 0 / 0 |
| Added absent/repeated host timing samples | 0 |

The session's ending histograms report callback-to-accepted-Present p95/p99 upper bounds of 0.50/0.50 ms and accepted-Present gap bounds of 4.75/4.75 ms, with a maximum gap of 5.456 ms. Those histograms include the session before this observation window; they are not interval-only percentiles. All six observer samples were fresh and connected, with private memory approximately 141 MiB. This short observation does not establish long-term leak behavior.

The user confirmed 244 FPS on the source animation counter and that the separate Moonlight control stream was disconnected. The run used the instructed 100 Mbps HUD preset. The browser counter is separate from the receiver and display rates. No valid display trace or optical measurement was collected, so these numbers do not prove 240 distinct displayed frames, capture-to-screen latency or a version-to-version improvement. Representative local-game load and reconnect checks remain outstanding. Raw logs and observer/aggregate artifacts remain ignored; only reviewed aggregate results are published.

### Installed H.264 comparison (0.2.1.8)

The user reconnected with H.264 using the same 2560x1440, 240 requested FPS, 100 Mbps, near-black cleanup, Crisp scaling and fitted placement, and reported no perceptible text/edge quality difference from HEVC. Six passive observer samples over 75 seconds were fresh and connected. Beginning/ending counters cover a longer 151.174-second post-startup interval; rates and means below are normalized to each codec's own window.

| Metric | HEVC baseline | H.264 comparison |
| --- | ---: | ---: |
| Counter window | 75.606 s | 151.174 s |
| Received units / accepted-Present samples | 17,238 / 17,238 | 36,155 / 36,155 |
| Received units / accepted-Present samples per second | 227.998 / 227.998 | 239.161 / 239.161 |
| Host-reported nonzero processing average | 6.473 ms | 3.539 ms |
| Decoder submit average | 0.114 ms | 0.109 ms |
| Callback-to-accepted-Present average | 0.172 ms | 0.167 ms |
| Assembly / enqueue-to-submission average | 0.0048 / 0.0077 ms | 0.0029 / 0.0080 ms |
| Added decode errors / skipped frame indexes | 0 / 0 | 0 / 0 |
| Added display replacements / GPU-slot retries / Present retries / wait timeouts | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |

H.264's ending cumulative callback-to-Present p95/p99 upper bounds were 0.25/0.50 ms; accepted-Present gap bounds were 4.75/5.00 ms, with a cumulative maximum gap of 7.581 ms. Two display replacements and a peak decode queue of two occurred before the saved beginning counters; no additional replacements occurred within the measured interval. Private memory stayed approximately 137 MiB during the short observer window. These cumulative tails and differently sized windows are not a controlled interval-only comparison of rare stalls or long-term memory behavior.

On this source/receiver setup, H.264 is the preferred next profile for load testing: source-reported processing was about 2.934 ms lower, receive/submission rate about 4.9% higher, receiver time essentially unchanged, and the user did not perceive a quality loss. This sequential comparison does not establish an end-to-end latency reduction of 2.934 ms or a universal codec recommendation. It changes only the streaming profile, not the installed package or renderer. Keep the same settings for a representative local-game test and save the profile if the result is accepted.

No valid display trace or optical measurement was collected. Actual displayed source-frame timing, gaming impact and broader reconnect behavior remain outstanding. Logs and private calculations remain ignored; only these reviewed aggregates are published.

### Current gameplay monitoring (0.2.1.8)

The user reported that the game was running and everything looked good, then requested monitoring of current use. The moving owned source fixture and source-control disconnection were not reconfirmed for this later run. The widget had restarted since the idle comparison; its executable hash still matched the installed 0.2.1.8 checkpoint. The active stream remained H.264, 2560x1440 and 240 requested FPS. This is a current-use observation, not a controlled same-workload measurement of the game's cost or a repeat of the idle fixture.

Six passive observations across 75 seconds stayed fresh and connected. Beginning/ending cumulative counters cover a 176.871-second interval:

| Metric | Current gameplay observation |
| --- | ---: |
| Received units / accepted-Present samples | 41,958 / 21,613 |
| Received units / accepted-Present samples per second | 237.224 / 122.196 |
| Host-reported nonzero processing average | 3.495 ms |
| Decoder submit average | 0.139 ms |
| Callback-to-accepted-Present average | 2.029 ms |
| Assembly / enqueue-to-submission average | 0.0321 / 0.0128 ms |
| Added decode errors / skipped frame indexes | 0 / 0 |
| Added display replacements | 20,345 |
| Added GPU-slot / Present retries | 0 / 0 |
| Added bounded presentation wait timeouts | 1,534 |
| Added absent/repeated host timing samples | 13 |

Received units equal accepted-Present samples plus display replacements within this interval. The renderer waits for DXGI presentation capacity before taking the newest mailbox frame; an eight-millisecond bounded wait can time out while older decoded display frames are replaced. Encoded decoder inputs remain ordered. These counters locate pressure at presentation readiness; they do not establish whether composition, display mode, GPU scheduling or another factor caused it.

The ending cumulative histograms report callback-to-Present p95/p99 upper bounds of 4.25/5.00 ms and accepted-Present gap bounds of 15.00/18.00 ms. Maximum accepted-Present gap was 193.860 ms and peak pending decode queue was 14, both cumulative across the session rather than isolated to this interval. Private memory ranged from about 135 to 141 MiB during the six observations; that short range is not a leak test. Driver-reported desktop modes differed from the original target, but do not establish game scanout timing.

The current-use presentation rate is materially below the idle H.264 run's 239.161 submissions/s, while receive rate and source processing remain close. The user's visual acceptance is recorded without treating accepted submissions as actual displayed FPS. No game FPS/frame-time baseline, valid display trace or optical latency measurement was collected, and source/display conditions were not held identical. Maximum performance and absence of game impact remain unproven. No renderer, timer, priority, package, game or display setting was changed during monitoring. Raw process/resource details and logs remain private.

### Follow-up resource and end-of-session observations

Two passive GPU-engine samples one second apart attributed the game and widget to the same adapter. The game's busiest reported graphics engine averaged 95.5% utilization (96.3% maximum); the widget's reported 3D and video-decode engines averaged 30.2% and 14.2%. Engine percentages are separate utilization readings and must not be added together. This is consistent with GPU contention, but two samples cannot establish the cause of the presentation waits or the game's FPS cost.

The widget and game subsequently exited. The retained tail log contains 197 transport decode-queue overflow messages across approximately 20.8 seconds before app suspension; its last rate snapshot reported zero receive/decode/Present activity. These transport failures occurred after the saved gameplay window and are distinct from its zero added decoder errors. No timing trace identifies the initiating stall or lifecycle transition. Do not treat the earlier healthy decoder counters as proof that the entire session stayed healthy. Reproduce and trace this end-of-session behavior before changing shutdown or decoder synchronization.

A [narrow passive API collector](PASSIVE-TRACE.md) is now available to investigate Present timing without a rendering probe or broad GPU/display providers. Its owned ETW transport test passes in Debug and Release with 100 complete ordered event pairs and zero loss. An existing-process sample produced no DXGI events and was explicitly rejected as incomplete. Later captures against the restarted active game and widget also returned no usable DXGI events; the file-backed control below rules out treating this solely as a real-time consumer failure. Displayed-frame validation remains pending. No installed widget, running app, timer, priority or display setting was changed by this follow-up.

### Restarted HEVC gameplay observation

The game and installed 0.2.1.8 widget restarted. The active profile was HEVC, 2560x1440 at 240 requested FPS. Neither source fixture conditions nor the separate control stream's connection state were established. Sixty-one rate blocks retain that profile across a 303.010-second cumulative-counter interval:

| Metric | Restarted current-use observation |
| --- | ---: |
| Received units / accepted-Present samples | 70,594 / 42,626 |
| Received units / accepted-Present samples per second | 232.976 / 140.675 |
| Host-reported nonzero processing average | 6.166 ms |
| Decoder submit / callback-to-accepted-Present average | 0.151 / 1.902 ms |
| Assembly / enqueue-to-submission average | 0.0041 / 0.0139 ms |
| Added decode errors / skipped frame indexes | 0 / 0 |
| Added display replacements / presentation wait timeouts | 27,968 / 1,606 |
| Added GPU-slot / Present retries | 0 / 0 |
| Added absent/repeated host samples | 10 |

Received units equal accepted samples plus display replacements. No transport decode-queue overflow was recorded in this saved interval. The ending cumulative callback p95/p99 bounds were 4.25/4.75 ms; accepted-Present gap bounds were 13.00/19.00 ms, with a cumulative maximum gap of 260.025 ms and peak decode queue of seven. Six fresh, connected process observations across 75 seconds reported zero decoder errors and private memory of 137.9-143.4 MiB. These resource observations are shorter than the counter interval and do not establish absence of leaks.

This reinforces the presentation-readiness concern under current game load, without establishing its cause, actual displayed FPS, game cost or a codec gain. Source/load conditions were uncontrolled and short passive trace probes ran during monitoring. No installation, display, timer, priority or application-setting change was made.

### Further passive gameplay window

A later 202.147-second window of the unchanged installed 0.2.1.8 process retained HEVC/2560x1440/240 in all 41 sampled rate blocks. It recorded 46,947 received units and 24,306 accepted-Present samples, averaging 232.242 and 120.239 per second. The remaining 22,641 units were replaced before display submission, with 1,323 added presentation wait timeouts and zero added decoder errors, GPU-slot retries or Present retries. The widget was responsive at the end.

Unlike the earlier saved window, this interval includes one allowlisted transport decode-unit queue overflow and 18 skipped frame indexes. The interval duration comes from log wall-clock timestamps; the installed baseline does not have the candidate's monotonic session fields. Process identity/start time, package version and stream profile were checked for continuity. Source activity and the separate control-stream connection state were unknown. This is passive current-use evidence, not a display-FPS or game-impact measurement. Offline compilation ran during the observation, so it is not an isolated gaming benchmark. Logs and numeric summaries remain private under ignored `build/` storage. No runtime, display, timer or application settings were changed.

### In-process timing candidate

The prepared 0.2.1.9 candidate adds private-log CPU timing for completed decoder submissions, successful and timed-out presentation-capacity waits, draw calls and Present API calls. Each distribution retains sample count, total, maximum and p95/p99 upper bounds. The existing 250-microsecond bounded histogram is reused; there is no allocation or mutex per histogram sample. Successful waits include immediately ready returns; timed-out waits are a separate population. Draw timing includes context-lock/scheduling delays and command submission. Present timing includes successful calls and retry returns. These are CPU wall times, not GPU execution, hardware-decoder completion or display timestamps.

A worker reports its stage and time since entry using one lock-free atomic value containing both fields. Readers cannot combine one stage with another stage's timestamp and do not spin or acquire that worker's mutex. Decoder stages distinguish input preparation, submission and output publication; render stages distinguish frame waiting, resizing, presentation readiness, frame acquisition, drawing, Present, backoff and frame release. A long stage age includes legitimate waiting and scheduling delays; it does not itself prove a deadlock. An unobserved worker is explicitly labeled. Scope cleanup restores decoder stages on exceptions and marks the render worker stopped when it returns.

The first transport decode-queue overflow per connection also writes those worker stages and ages directly from the receiver callback, without acquiring state, immediate-context or decoder mutexes. Subsequent messages increment a separate transport-overflow counter without repeating the extra activity report. This preserves evidence if UI statistics stop updating and distinguishes transport overflow from decoder errors. Logging remains best effort; neither an age nor a timeout selects a root cause automatically.

Portable core contracts pass in Debug and Release, including concurrent stage/timestamp snapshots, clock-order clamping, nested callback exceptions and reconnect reset after joining the writer. UWP session libraries, desktop diagnostic integration and Debug/Release widget builds compile successfully; widget builds report zero warnings/errors. A CPU-only synthetic exercise of phase transitions and four histogram updates completed without hot-path allocations. The production timing path has not yet been measured live. No GPU workload test or deployment was performed while the user was gaming. Unsigned Debug/Release 0.2.1.9 archives are prepared and pass integrity, manifest and executable checks, with unchanged packaged dependency DLLs. The installed widget remains 0.2.1.8; this candidate is uninstalled and is not a verified performance improvement.

### Summarizing private timing checkpoints

For the 0.2.1.9 format, save two copies of the private runtime log at least 60 seconds apart during one uninterrupted widget process and stream. Keep the source content and load consistent. Each rate block records a steady-clock session-start token and elapsed microseconds. The elapsed difference supplies the interval duration without relying on the log's wall-clock timestamp; the token guards against reconnects with the same profile. It is a local session marker, not an identity for comparisons across machines or reboots.

```text
python tools/summarize_widget_timing.py build/begin.log build/end.log --output build/timing-summary.json
```

The tool reads the newest rate block from each file and exports known numeric counters and worker-stage names only. It rejects different connections, changed profiles, resets, lifecycle changes after a rate block, inactive intervals and incomplete newest blocks; capture fresh copies when rejection indicates a partial write. Inputs are bounded to 64 MiB. Existing 0.2.1.8 logs are rejected because they lack the required session timing fields.

The JSON reports interval receive/accepted-Present rates, display replacements, wait timeouts, overflow counts and averages for completed CPU calls. Maxima and percentile upper bounds are explicitly the ending cumulative distributions; subtracting percentile values would not produce interval percentiles. Snapshots can straddle in-flight work, so a balance residual is reported without forcing equality. These results exclude GPU execution, display timing and optical latency. Keep checkpoint logs and summaries under ignored `build/` storage.

### Remaining performance verification

Maximum performance remains unproven. Continue the review against these gates instead of treating reduced allocation counts as completion:

| Area | Required evidence |
| --- | --- |
| Compressed input handling | Owned 100 Mbps and synthetic size probes are recorded above. Still verify the actual HUD access-unit distribution and any proposed ownership change against isolated pipeline timings. |
| Shader and GPU reads | Isolated fitted-size timestamp/alpha checks are recorded above. Still compare representative HUD textures and gaming GPU load while preserving color/alpha/crop regressions. |
| Presentation pacing | Controlled Game Bar trace with changing source frame IDs; distinguish accepted Present calls from display updates and source content. |
| Source and transport | Idle moving-HUD HEVC/H.264 processing, queue and assembly results are recorded above. Still measure actual access-unit sizes and behavior under representative load before proposing transport ownership changes. |
| Gaming impact and stability | Current gameplay monitoring above exposes lower presentation throughput without decoder errors. Still collect a controlled game FPS/frame-time comparison, display trace, reconnect and shutdown results. |

## Timer and lock review

The normal render path wakes on decoded-frame notifications and the DXGI waitable presentation object. It does not sleep for a fixed 4 ms. The 1 ms condition timeout remains only when all GPU read slots are occupied; Windows scheduling can make that timeout longer. A global timer-resolution change cannot correct source encoding or display composition delays, and none is added here.

The live comparison recorded no GPU-slot retries, so the 1 ms fallback was not on that measured path. Remaining output-lease allocations and the encoded-byte copy remain candidates for later profiling, but measured receiver averages near 0.25 ms give little evidence that rewriting their ownership or removing the shared context lock would materially improve this workload.

`auto` is compile-time type deduction. Local scalar declarations do not inherently allocate memory or slow a loop. The condition-variable mutex is released while waiting; publication under that same mutex prevents lost wakeups. D3D11's shared immediate context still requires serialization with FFmpeg's hardware callbacks. A stop check after waking remains necessary because shutdown can arrive during the wait. Relevant primary references are [C++ type deduction](https://learn.microsoft.com/en-us/cpp/cpp/auto-cpp), [Windows timer resolution](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod), [D3D11 multithreading](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro), and [DXGI waitable swap chains](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains).
