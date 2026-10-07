# Build and deployment

These steps have been run on the receiving PC after the user authorized execution. The filename is retained for existing links.

## Prerequisites

- Visual Studio 2022 with v143 C++, `Microsoft.VisualStudio.ComponentGroup.UWP.VC`, and Windows SDK 10.0.26100.0.
- Windows 11 x64, Xbox Game Bar, CMake, and Git.
- Python 3.10+ for repository and benchmark failure checks; the Control target also needs Python cryptography.

The installed VS 2022 Community instance is 17.14.37301.10. Game Bar is 7.326.8061.0. NuGet pins remain `Microsoft.Gaming.XboxGameBar` 7.2.240903001 and `Microsoft.Windows.CppWinRT` 2.0.200203.5, matching the inspected Microsoft sample. MSBuild restores `packages.config`; a separate NuGet CLI is not needed.

From the workspace root:

```powershell
.\tools\Build.ps1 -Target Core -Configuration Debug
.\tools\Build.ps1 -Target GPU -Configuration Release
.\tools\Build.ps1 -Target Widget -Configuration Release
.\tools\Build.ps1 -Target Decoder -Configuration Release
.\tools\Build.ps1 -Target Control -Configuration Release
.\tools\BuildStreamingLibraries.ps1 -Configuration Release
.\tools\Deploy.ps1 -Configuration Release
```

`Build.ps1` also accepts `All` and either configuration. Core/GPU targets run CTest. The GPU target requires a hardware D3D11 device and tests shader output using test-only CPU readback. Production rendering does not read pixels back to the CPU. The Control target uses an existing Python runtime with `cryptography` to run loopback HTTP/TLS pairing fixtures and Windows protected-storage round-trips; it never pairs or launches on a real source PC.

The explicit Decoder target builds pinned UWP dependencies, extracts the SDK runtime into the test directory, generates twelve intra-frame, 120 inter-coded-frame, and 120 moving-square-frame H.264/HEVC fixtures with the existing FFmpeg CLI/NVENC, and runs eight tests. It also rewrites H.264 matrix metadata into an owned BT.2020 rejection fixture. The Python regression verifies that a decoder error reports the expected message and joins the benchmark worker within five seconds. If ffmpeg is not on PATH, run GenerateDecoderFixtures.ps1 with -FfmpegPath pointing to the existing executable before the direct CMake build/test commands. Widget builds also invoke BuildStreamingLibraries for Sunshine control, Moonlight, FFmpeg, and session libraries. The view-only build removes Moonlight's source-mouse wake-up from an isolated copy of pinned source. Dependency builds use a task-specific temporary build tree and an FFmpeg response-file overlay to support spaces in the workspace path.

Widget builds generate C++/WinRT/XAML, compile HLSL, generate placeholder package logos, and produce an unsigned MSIX. Logs are in `build/widget-{Configuration}.log`; binaries are in `out/`; packages are in `AppPackages/`. No build target installs or starts the app.

## Portable repository checks

These CPU-only checks require CMake 3.24+, a C++20 compiler, Git and Python 3.10+. They do not restore streaming dependencies, install the widget or contact Sunshine:

```text
python tools/audit_repository.py
cmake -S . -B build/core -DFUSER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

GPU, decoder, control and display-probe targets are off by default. GitHub Actions runs the portable checks on Windows and Linux. Hardware/widget verification remains separate. See [contributing](../CONTRIBUTING.md) for staged-file auditing and checkpoint publication.

## Receiver-only paced benchmark

After building the Decoder target, run these sequentially in a temporary PowerShell session so the test runtime PATH stays local to that process:

```powershell
$env:PATH = (Join-Path (Get-Location) 'build/test-runtime/Release') + ';' + $env:PATH
.\build\decoder\Release\fuser_fixture_benchmark.exe .\build\fixtures\moving-pattern.hevc hevc 240 14400
.\build\decoder\Release\fuser_fixture_benchmark.exe .\build\fixtures\moving-pattern.h264 h264 240 14400
```

The arguments are fixture path, codec, paced FPS (0 means unpaced), and total decode inputs. Append `decode-only` to omit concurrent drawing from the timed input loop; output leases and alpha are still checked after decoder shutdown. The tool reports actual decoded/Present-call rates, frame replacements, unavailable GPU slots, pacing misses, and elapsed-time percentiles (including driver/context waits). It verifies key alpha after joined shutdown. It uses an offscreen composition swap chain, so these measurements exclude Sunshine/network, Game Bar, monitor scanout, and local-game load. The 60-second runs measured approximately 240 FPS on the validation receiver; see VALIDATION.md for exact results and limits.

## GPU draw benchmark

After `Build.ps1 -Target GPU -Configuration Release`:

```powershell
.\build\gpu\Release\fuser_gpu_draw_benchmark.exe 2556 1396 2000
```

Arguments are output width, output height and sample count. Use the logged video-area dimensions for a fitted-size comparison. The tool renders an owned 2560x1440 NV12 pattern with Crisp scaling, measures D3D11 draw-command timestamps for exact black, near-black cutoff and default green, rejects disjoint timing, and verifies alpha using diagnostic-only readback. It waits synchronously between samples, excludes Present/decoder/scanout costs and does not connect to Sunshine. See [receiver performance](RECEIVER-PERFORMANCE.md#gpu-draw-timing) for results and limits.

## Source animation fixture

Use an existing Python runtime to serve only the owned HUD page:

```powershell
python .\tools\serve_hud_fixture.py --bind RECEIVER_LAN_IP --source SOURCE_LAN_IP --minutes 120
```

Replace RECEIVER_LAN_IP and SOURCE_LAN_IP with your own addresses. Open `http://RECEIVER_LAN_IP:8765/hud.html` on the captured source display and use its fullscreen button. The page uses a reserved green background, moving white square, four corner markers, and a frame barcode; its animation FPS counts browser callbacks. After the page is ready, disconnect any source-control Moonlight stream locally and connect Software Fuser at HEVC/240. Record the source counter, widget receive/decode/Present rates, and visible keying/corner alignment. The server defaults to 30 minutes, permits up to 120 minutes, exposes no other files, and needs restarting after expiry.

## Explicit local development deployment

The manifest uses Microsoft's Windows 11 unsigned-development publisher OID. `Deploy.ps1` checks the package identity, asks Windows for administrator elevation, and calls `Add-AppxPackage -AllowUnsigned` for this package. It does not enable global Developer Mode or install a signing certificate. This is a local development package, not a signed distribution release. See the [official unsigned-package procedure](https://learn.microsoft.com/en-us/windows/msix/package/unsigned-package).

The verified Release registration is `SoftwareFuser.Widget_6g84c2f4w9w1a`, version 0.2.1.7, package status OK (0). A canceled UAC prompt leaves deployment unfinished; it is not success. Debug deployment additionally supplies the SDK's matching VCLibs debug framework when needed. Protected app-local pairing survives updates with this package identity.

Historical placement baseline: 0.2.0.9 was built and installed, with zero warnings/errors in both widget builds and passing focused layout contracts in Debug/Release. The current 0.2.1.7 checkpoint retains explicit placement actions, usable-area video fitting and opaque black removal, and aligns Crisp HUD brightness/color sampling. **Fit my monitor** requests detected monitor dimensions immediately; **Overlay width/height (px)** and **Apply dimensions** request a typed physical size independently of video negotiation. **Try full-screen fit** is an explicit Windows API compatibility test for the top gap; its result must be verified in Game Bar. Opening/closing/pinning Game Bar initiates no app resize or recenter. Only Reset centers a smaller window. Reset, black transparency, and click-through were previously confirmed, but automatic four-edge coverage remains unverified. `LocalState/runtime.log` records size, position, all four edge gaps, view/chrome geometry, explicit requests, and host responses.

The manifest version determines the package directory under AppPackages/FuserWidget. Debug and Release verification builds have zero warnings/errors. Version 0.2.0.3 added black-key selection and automatic fitting with passing GPU contracts, but its on-screen coverage was incomplete. Version 0.2.0.2's new instrumentation was verified in a short live HEVC check with normal Disconnect. Earlier PresentMon and long passive recording results belong to 0.2.0.1. Use `Get-FileHash -Algorithm SHA256` to verify your built package. Deployment uses the manifest's current version and closes this widget's active process. Existing protected pairing uses the same package identity.

## Runtime checks still required

1. Open Software Fuser through Game Bar (Win+G, widget menu). Confirm it remains idle until an explicit action.
2. Click **Draw test pattern**. Pin the widget and close Game Bar. Verify the key background disappears while white/cyan/red markers remain visible.
3. Enable Game Bar click-through and confirm local input reaches the app underneath.
4. Request a monitor-sized widget. Record actual bounds and all four corners at the intended DPI.
5. Test close/reopen, repeated activation, save/restore, suspension, resize, and opacity.

6. Click Pair with Sunshine and enter the widget's displayed PIN in the source Sunshine PIN page. Select the intended application and Connect. A different active application is preserved.
7. Record actual setup dimensions/codec and receive/decode/present-call rates; inspect LocalState/runtime.log for stage failures and sampled counters. Test Disconnect/Cancel, reconnect, and source restart. Never place PINs or private keys in logs.

Offscreen GPU and controlled pairing tests do not prove these live compositor/lifecycle behaviors or 240 displayed FPS. Audio output is muted; the protocol may still receive audio packets.

## Taskbar placement test

The 0.2.0.4 foreground screenshot showed a full-sized test surface shifted upward about 44 pixels, with the bottom border above the taskbar. Taskbar auto-hide made no difference. Reset later restored movement, but the user clarified that covering the taskbar required manual dragging. The built-in Audio widget can overlap the taskbar in the supplied comparison. The [centering API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) can move or resize a widget to satisfy host bounds; 0.2.0.8 avoids it during Fit and removes automatic resizing on host transitions.

On the tested host, 0.2.0.9's 2560x1440 Apply and full-screen requests were declined. Its app title bar is already hidden with zero height. The user then confirmed manual adjustment cannot cover the top and taskbar simultaneously. The procedure below records behavior; it is not a verified coverage workaround. Presentation remains inside the Game Bar UWP widget. Main fits video above the taskbar; the separate testing branch investigates host frame coverage. See [video fitting](VIDEO-FIT.md).

1. Open Software Fuser through the Game Bar widget menu. Startup unlocks resize controls but does not request a new size or center unless a user-saved Reset is pending.
2. Click **Reset widget position** and try moving its title bar. Reset requests a smaller centered window, preserving the Sunshine pairing and video/key profiles. If controls remain inaccessible, save a reset from the standalone settings page, close the widget, and reopen it through Game Bar.
3. Choose **Black**, click **Draw test pattern**, then click **Fit my monitor** while Game Bar remains open. Record the immediate actual size, position, status, and four edge gaps. No pinning step is required. The cyan rectangle is intentionally inset; judge the outer white border.
4. Enter your monitor's physical width/height under **Overlay width/height (px)**, then click **Apply dimensions**. Check actual size against the requested values. At 125% scaling, for example, 2560x1440 physical pixels requests 2048x1152 view pixels. The stream width/height fields remain separate. Invalid inputs must show a validation error without issuing a resize. Saved dimensions only refill the fields, without automatically applying on reopening.
5. If a top gap remains, click **Try full-screen fit** and record its exact status plus the top/bottom gap values. The public Windows full-screen API can be declined by the Game Bar host; acceptance alone is not proof of coverage. If declined, position manually: move the widget upward by the reported top gap, then extend its bottom edge as needed to retain taskbar coverage. Watch all four gap values and the white border. Use **Reset widget position** to exit full screen or restore accessible controls.
6. Pin and close Game Bar, then reopen it. Confirm Software Fuser does not shrink/recenter your manual placement or initiate another fit. Check transparency against a visible nonblack app and verify click-through. Record any host-driven geometry change separately from an app layout request; retain local geometry logs and report missing edges.

If placement remains clipped, compare the logged client/visible/video-local bounds and centering stages before another layout change. The failed auto-hide test does not require keeping a global taskbar preference changed.

## Sunshine probe

```powershell
.\tools\ProbeSunshine.ps1 -HostAddress SOURCE_LAN_IP
```

Replace SOURCE_LAN_IP with your own source address. The probe reads `/serverinfo` only. It does not pair, launch, resume, or terminate a host application. Preserve any active source session when implementing connection controls.

## Live developer diagnostics

The `fuser_sunshine_diagnostics` target is available when both `FUSER_BUILD_CONTROL_TESTS` and `FUSER_BUILD_DECODER_TESTS` are enabled. The current configured build is `build/diagnostics`; its native runtime uses the same process-local SDK PATH as the fixture tests. It reads only this widget's previously paired protected identity. `apps` inspects the host and enumerates applications; `stream` launches/resumes the selected app and receives video offscreen, with no pairing, host quit, input forwarding, or audio playback.

For your paired source, first enumerate applications, then use the intended application's returned ID to repeat three five-second HEVC/240 connections in one process:

```powershell
cmake --build build/diagnostics --config Release --target fuser_sunshine_diagnostics
$env:PATH = (Join-Path (Get-Location) 'build/test-runtime/Release') + ';' + $env:PATH
$diagnosticState = Join-Path $env:LOCALAPPDATA 'Packages/SoftwareFuser.Widget_6g84c2f4w9w1a/LocalState'
.\build\diagnostics\Release\fuser_sunshine_diagnostics.exe $diagnosticState SOURCE_LAN_IP apps
.\build\diagnostics\Release\fuser_sunshine_diagnostics.exe $diagnosticState SOURCE_LAN_IP stream SOURCE_APP_ID hevc 240 5 3
```

Replace SOURCE_LAN_IP and SOURCE_APP_ID before running the commands. Arguments after `stream` are an actual source app ID, codec, requested FPS, optional seconds per cycle (default 12), and optional cycles (default 1). The maximum total requested streaming duration is one hour. The same session and renderer are reused with joined shutdown between cycles. Application IDs are host-specific; enumerate them before using another source. These diagnostics do not verify the visible Game Bar overlay or monitor scanout.

## Passive live-widget measurements

PresentMon's official portable 2.6.0 console executable is under build/tools/PresentMon-2.6.0, with its license retained. The GitHub release digest and valid Intel signature were checked. The current account is in Performance Log Users, so the recorded trace required no elevation or installation. Target one existing widget process, use a unique ETW session/output name, and omit input tracking:

```powershell
$presentationWidget = @(Get-Process -Name FuserWidget)
if ($presentationWidget.Count -ne 1) { throw 'Select exactly one widget process.' }
$presentationStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
.\build\tools\PresentMon-2.6.0\PresentMon-2.6.0-x64.exe --process_id $presentationWidget[0].Id --output_file "build/fuser-presentmon-$presentationStamp.csv" --session_name "SoftwareFuser-$presentationStamp" --timed 60 --terminate_after_timed --no_console_stats --no_track_input --write_display_metadata --track_gpu_video
```

Analyze each swap chain separately. Missing display timestamps, presentation modes, display-source/layer metadata, and trace errors affect attribution. Display source IDs are local to an adapter; this receiver has source ID 0 on both GPUs. Correlate the read-only display probe's adapter LUID/path and dimensions with the diagnostic widget's renderer adapter and associated display before identifying a physical monitor. Those fields alone do not prove coverage or the route of cross-adapter composition. ETW display updates do not prove distinct source pixels or optical timing. The existing trace measured 225.606 display updates/s; it did not establish the requested 240 FPS.

Observe process resources and logged rates without another video connection:

```powershell
.\tools\ObserveWidget.ps1 -WidgetProcessId $presentationWidget[0].Id -DurationSeconds 1800 -PollSeconds 15
```

The observer verifies the PID/start-time identity, writes only a timestamped CSV under build, and stops at its bounded deadline or process exit. Sample age and disconnect-after-sample fields distinguish current rate data from retained log entries. It does not inject input, control the widget, or attribute network traffic to a stream. Source and local-game workloads still require separate documentation.

After the observer exits, summarize its exact output file:

```powershell
python .\tools\analyze_widget_observation.py .\build\widget-observation-YYYYMMDD-HHMMSS.csv --minimum-seconds 1800 --output .\build\widget-observation-summary.json
```

The analyzer reports observed duration, profiles, counter regressions, process resources, and CPU usage as a percentage of one logical processor. Rate samples older than ten seconds (twice the widget's five-second logging period), or followed by a disconnect, are excluded from rate summaries and counted separately. A partial recording remains marked below the requested duration. Confirm the observer's exit separately: CSV duration alone does not prove process completion. These sampled rates do not establish scanout, a complete frame count, or controlled source/game workloads.

## Checkpoints

Save source, packages, and selected local evidence with a freshly verified installed version:

```powershell
$checkpointWidget = Get-AppxPackage -Name 'SoftwareFuser.Widget'
if (-not $checkpointWidget -or $checkpointWidget.Status.ToString() -ne 'Ok') { throw 'Verify the installed widget before recording its version.' }
python .\tools\create_checkpoint.py --installed-widget $checkpointWidget.Version.ToString()
```

The archive and verification receipt stay under ignored `build/checkpoints`. If the installed version cannot be verified, omit `--installed-widget`; the archive records it as unknown rather than assuming an old version. The tool verifies every archived file hash and ZIP integrity. At each checkpoint, separately inspect the authored Git changes for private data and keys, commit and push them, and confirm that the remote branch matches the local commit. Keep packages, runtime logs, credentials, and private notes excluded from publication.
