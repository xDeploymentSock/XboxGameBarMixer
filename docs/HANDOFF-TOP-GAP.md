# Handoff: Game Bar overlay cannot cover the top and bottom simultaneously

## Objective and presentation constraint

Solve full-monitor coverage of Software Fuser's transparent Xbox Game Bar widget. The target is 2560x1440 with 240 requested stream FPS. Keep the rendered video inside Game Bar's UWP widget surface. A separate Win32 desktop window using HWND_TOPMOST, layered-window techniques, or DirectComposition is not an accepted substitute unless the user explicitly changes that requirement. The previously proposed companion renderer has not been approved or implemented.

The user permits manual intervention to establish coverage, but their latest test could cover the top or the taskbar, not both at once. They also requested typed monitor dimensions and an Apply button; these controls exist, independently of stream negotiation, but this host rejects a full-monitor resize.

## Current state

- The continued full-coverage goal is active. Version 0.2.0.11 adds a one-shot pinned-only constraints probe, is built in both configurations with zero widget warnings/errors, and is independently verified installed with status OK; see [PINNED-COVERAGE-TEST.md](PINNED-COVERAGE-TEST.md). Normal startup configuration and prior results remain documented in [FIXED-STARTUP-TEST.md](FIXED-STARTUP-TEST.md). Rescaling the incoming feed is the user's fallback, not the current implementation. No new live success is claimed.

- The `codex/fixed-startup-coverage` branch contains a 0.2.0.10 startup-size experiment while main remains at a6a9a85. Read [FIXED-STARTUP-TEST.md](FIXED-STARTUP-TEST.md) for the exact configuration, build evidence, and live result. The correct test activation reports fixed 2560x1440 limits, but Game Bar settles the full-sized client at (0,-44), leaving a 44-pixel bottom gap and an off-screen pin button. Reopening repeats the offset. The subsequent Reset/pin/Fit probe confirms Reset and pinning work: the 480x700 window pins successfully, but the pinned 2560x1440 Fit request is rejected and retains the smaller video area. The 0.2.0.9 source/package baseline is preserved. Full-monitor coverage remains unresolved.

- Installed package on the test branch: SoftwareFuser.Widget, version 0.2.0.11, status OK. The fixed-startup test was 0.2.0.10 and the preserved baseline was 0.2.0.9. Pairing is retained in protected app-local storage under the same package identity.
- Feature commit: b957ff0cf893d4ee4e9386fcf21b4aae84372e07. Follow-up evidence commit: acd297635917584f2acd6a1d21ad520f38f8d4c2. Both were pushed to main at https://github.com/xDeploymentSock/XboxGameBarMixer.git.
- The checkout was clean at handoff preparation. Read the current HEAD rather than assuming it stays at these revisions.
- Debug and Release widget builds: zero warnings/errors. Focused core contracts: 1/1 pass in each configuration. These checks prove code/build behavior, not successful on-screen coverage.
- Black-key transparency, click-through, and Reset were confirmed in earlier live tests. The accepted interim stream milestone is above 200 measured FPS; exact 240 distinct displayed frames and further performance tuning are outside this issue.

## Measured failure

On a 2560x1440 display at scale 1, the persistent settled geometry is:

| Measurement | Value |
| --- | --- |
| Widget bounds | X=0, Y=46, width=2558, height=1394 |
| CoreWindow client bounds | X=0, Y=46, width=2558, height=1394 |
| ApplicationView visible bounds | X=0, Y=46, width=2558, height=1394 |
| Video-local origin | X=0, Y=0 |
| Video area | 2558x1394 |
| UWP title-bar height | 0 |
| UWP title-bar visible / extended | false / false |
| ApplicationView full-screen state | false |
| Uncovered edges | left=0, top=46, right=2, bottom=0 physical pixels |

Two live Apply dimensions requests for 2560x1440 reached XboxGameBarWidget.TryResizeWindowAsync and returned false. Two explicit ApplicationView.TryEnterFullScreenMode attempts also returned false, without an additional resize or center call. The user confirmed full-screen fit was declined.

The gap is outside the application's rendered client surface. The measurements locate it in Game Bar's host/frame placement; changing a nonexistent UWP title-bar height or translating content within its clipped surface is not evidence of fixing monitor coverage. During initial activation there was a transient 2560x1440 client layout before the host applied its smaller settled bounds; do not mistake that transient for success.

The latest user report: “if i go top down, i cant cover the taskbar, if i go bottom up, the top of the widget doesnt automatically go away”. Thus neither dragging nor the public Windows full-screen attempt has solved simultaneous top/bottom coverage on this host.

## Changes already tried

- Early automatic fitting on activation, pin/display transitions, and DPI changes caused repeated shrinking or repositioning. Do not reintroduce that behavior.
- Fixed min/max constraints and centering did not establish full coverage; earlier layouts shifted content upward about 44 pixels, losing the taskbar region. Taskbar auto-hide also failed in the user's test.
- Reset restored an accessible, draggable smaller window. Only Reset currently requests CenterWindowAsync.
- Version 0.2.0.8 made Fit my monitor an immediate explicit resize, leaving dragging/resizing enabled and removing automatic fit/recenter on host transitions. It still retained the top gap.
- Version 0.2.0.9 added physical-pixel Overlay width/height and Apply dimensions, strict input/range validation, DPI conversion, immutable request snapshots, latest-action cancellation, four edge-gap measurements, and the isolated full-screen compatibility test described above.
- A public API review and read-only SDK metadata inspection found sizing/centering but no outgoing arbitrary-position/title-bar-hide host command. The private IXboxGameBarWidgetPrivate4.SetWindowBounds appears among incoming SDK state-notification interfaces; it was not invoked to spoof reported geometry. Treat that interpretation as research evidence, not a proof that every possible in-host approach is impossible.

## Relevant implementation

- native/widget/MainPage.cpp: explicit button handlers, fit_monitor_async, update_coverage, log_view_geometry, activation/display callbacks, and renderer attachment.
- native/widget/MainPage.xaml and MainPage.h: dimension fields and handlers. Overlay dimensions are distinct from VideoWidth/VideoHeight.
- native/core/include/fuser/widget_layout_requests.h: actions fit_monitor, reset_position, apply_dimensions, full_screen_fit; dimensions captured at the click. Hiding/display changes cancel stale work without queuing another fit.
- native/core/include/fuser/monitor_layout.h: physical-to-view conversion, bounds matching, input/range checks, and uncovered_monitor_edges.
- native/widget/App.cpp: Game Bar activation and lifecycle; native/widget/Package.appxmanifest: Standard widget declaration and flexible size constraints.
- native/windows/D3D11Renderer.cpp/.h: GPU chroma-key rendering and premultiplied-alpha composition swap chain. native/streaming/OverlaySession.cpp/.h owns the decoder/render workers.
- tests/core_tests.cpp: geometry, fractional-DPI, dimension parsing/range/snapshot, and cancellation contracts.

## Evidence and workflow

Public history and build instructions are in README.md, docs/VALIDATION.md, docs/BUILD-LATER.md, and docs/ROADMAP.md. Private local evidence is ignored by Git:

- build/widget-0.2.0.8-before-custom-dimensions.log
- build/widget-0.2.0.9-fullscreen-declined.log
- build/widget-Release-0.2.0.9.log and build/widget-Debug-0.2.0.9.log
- build/layout-core-0.2.0.9.log and build/prepared-widget-0.2.0.9.json
- build/checkpoints/software-fuser-custom-dimensions-0-2-0-9.zip
- build/sdk-metadata-inspection contains generated, read-only SDK projections, not a packaged workaround.

Read the live runtime log from this package's LocalState/runtime.log if needed. Keep screenshots, runtime logs, personal paths, LAN addresses, pairing data, and keys out of tracked artifacts. The user explicitly requests commit and push at checkpoints; build/audit-publication.py audits staged blobs before publication. Do not overwrite unrelated work or force-push.

Use the applicable local debugging, C++ standards, and verification skills. Investigate the actual host sizing/frame rules and compare a working in-host example before adding another layout workaround. The built-in Audio widget was shown overlapping the taskbar, which disproves a blanket “Game Bar cannot cover the taskbar” claim but does not demonstrate a third-party full-screen widget.

Deployment uses tools/Deploy.ps1, checks the project identity, closes this widget, and may show Windows UAC. Prior idle updates were authorized; defer disruptive deployment/tests if the user resumes gaming. Native UI automation was unavailable in the preceding session. Use approved UI tooling if available, or ask the user for a focused live test; do not bypass tooling restrictions with generic desktop-control scripts.

## Useful primary references

- [Game Bar widget API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget)
- [Widget manifest](https://learn.microsoft.com/en-us/xbox/game-bar/guide/pkg-manifest)
- [Microsoft samples](https://github.com/microsoft/XboxGameBarSamples)
- [Historical full-screen/title-bar request](https://github.com/microsoft/XboxGameBarSamples/issues/111)
- [Referenced proof of concept](https://github.com/bittzz/XBOXGameBarPoC)
- [Windows full-screen request](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.applicationview.tryenterfullscreenmode)
- [UWP title-bar extension](https://learn.microsoft.com/en-us/uwp/api/windows.applicationmodel.core.coreapplicationviewtitlebar.extendviewintotitlebar)

## Acceptance

Follow-up source comparison: read [GAME-BAR-POC-COMPARISON.md](GAME-BAR-POC-COMPARISON.md). Its packaging manifest fixes the initial/minimum/maximum size before activation and leaves resizing enabled, with no centering call. The 0.2.0.4 attempt kept a small flexible startup manifest, set constraints at runtime, disabled resizing, and centered. Those are different tests. Version 0.2.0.10 now tests the PoC startup configuration at 2560x1440: it obtains full size but repeats the upward offset and fails coverage on this host. Keep Sunshine and the existing in-host renderer. Investigate accessible pin controls separately; do not treat incoming private state setters as host commands.

The outer white diagnostic border reaches all four monitor edges, including the top strip and taskbar, simultaneously; black remains transparent and clicks pass through. Typed dimensions apply predictably, or accurately report a host refusal. Opening/closing/pinning must not undo a manual placement. Verify settled host/client/render bounds and live visuals, not only a boolean API result or draw submission. Build/test and push a verified checkpoint. If the in-host requirement cannot be met, report the precise measured limit and available alternatives without silently switching to a desktop overlay or declaring the issue fixed.
