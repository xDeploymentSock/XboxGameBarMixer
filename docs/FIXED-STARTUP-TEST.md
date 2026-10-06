# Fixed startup coverage test branch

Branch: `codex/fixed-startup-coverage`. Baseline: main at a6a9a85, with the original 0.2.0.9 source/package retained. This experiment follows the [PoC source comparison](GAME-BAR-POC-COMPARISON.md); it is not a verified coverage fix.

## Prepared experiment

The experimental package is version 0.2.0.10. Its widget starts with initial/minimum/maximum client sizes all set to 2560x1440 **view pixels**, for the measured 2560x1440 monitor at scale 1. Both resize-support flags remain true. Activation preserves these manifest constraints, does not restore flexible limits, and defers any previously saved Reset without deleting the saved preference. No initial resize, full-screen, or centering request is made.

The experimental extension ID is `RemoteHudStartupTest`, displayed as **Software Fuser startup test**, to distinguish this test from the original widget's saved host placement. Verify the activated ID in the log: the first run after installation still activated the old `RemoteHud` definition. The application/package identity is unchanged; pairing and Sunshine settings remain in the same protected app-local storage. Sunshine transport, decoding, composition attachment, transparency, click-through, and the renderer are unchanged.

Explicit Fit, Apply dimensions, and Reset retain their previous behavior and restore flexible limits. Use them only after recording the untouched startup result. Reset is the recovery path if the fixed-size window leaves controls inaccessible. Reopening should not be treated as a fresh measurement after Reset without checking the actual constraints and placement: the host may cache the experimental extension's geometry too.

## Build and live procedure

Build using the existing [build commands](BUILD-LATER.md). Building does not install the experiment. The user explicitly requested deployment after preparation; the Release test is now installed as 0.2.0.10.

1. Record the installed version and keep the existing baseline package. Close Software Fuser before explicitly deploying the experimental Release package with `tools/Deploy.ps1`; avoid deployment during gaming. Do not uninstall the application or clear protected app-local storage.
2. Open **Software Fuser startup test** from the Game Bar widget menu on the 2560x1440 display at 100% scale. Do not click Fit, Apply, full-screen fit, or Reset yet. Confirm that the local runtime log records fixed 2560x1440 startup limits and both resize flags enabled.
3. Click **Draw test pattern** with Black selected. Wait at least five seconds for host placement to settle. Record all four edge gaps and widget/client/video geometry; inspect whether the outer white border reaches the top and taskbar simultaneously. Configured dimensions and an initial transient full-sized layout are not coverage proof.
4. Pin, dismiss Game Bar, and reopen it. Check settled bounds, all four visible edges, black transparency, and click-through again. Distinguish host movement from app layout requests in the local log.
5. After recording startup behavior, separately test Fit and typed Apply. They release the experimental fixed limits and may reproduce the baseline host refusal. A startup-only pass is not proof that dynamic sizing works.
6. Use Reset if needed. Preserve private runtime logs/screenshots under ignored build output. Report measured geometry and visible results before changing another sizing variable.

The source main branch and the baseline 0.2.0.9 package remain available. Returning Git to main restores source only; it does not downgrade an installed 0.2.0.10 package. A package rollback requires an explicit Windows deployment that permits downgrade and preserves the package identity. No installation, rollback, or on-screen success is implied by a build.

## Verification status

Debug and Release widget builds completed with zero widget warnings/errors. Each dependency-library rebuild emitted four C4244 conversion warnings in the existing upstream FFmpeg libavutil/common.h header; no streaming or decoder source was changed. The existing core layout contracts passed 1/1 in both configurations. Inspection of the built Release manifest verified version 0.2.0.10, the original package identity, the fresh extension ID, all six fixed dimension values, and both resize flags enabled.

After the user's explicit install request, deployment completed and an independent package query reports installed 0.2.0.10 with status OK and the original package family identity. The installed Release package matches the verified build hash. The original 0.2.0.9 Release package's SHA-256 remains unchanged. The previously measured baseline remains (0,46), 2558x1394 on the 2560x1440 monitor.

## Live result: full size with incorrect placement

The subsequent startup-test activation was verified as `RemoteHudStartupTest`, with minimum and maximum both 2560x1440 and both resize flags true. No app resize or centering request appeared during this run. On the 2560x1440 display at scale 1, Game Bar assigned widget bounds (0,-44), 2560x1440; client and visible bounds later settled to the same values. Video-local origin was (0,0) and the video area was 2560x1440. The bottom gap was 44 pixels. The user's screenshot independently showed the exposed taskbar and reported inability to pin.

Dismissing Game Bar temporarily changed reported widget Y to zero. Reopening initially showed matching client bounds at zero, then Game Bar moved the widget back to Y=-44. Those intermediate measurements are not stable coverage. The widget remained unpinned throughout the recorded transitions. The installed manifest enables pinning and the app does not disable it. The user confirmed that the pin button is off-screen; this is an accessibility consequence of placement, rather than evidence that clicking an accessible pin button fails.

This reproduces the earlier upward placement problem using the PoC's distinct startup configuration. Fixed manifest dimensions establish full client size on this host but have not established four-edge coverage or usable pin controls. The startup-size hypothesis is now tested and fails the coverage acceptance criterion on this host.

The existing Reset button can recover a smaller centered window to make the title bar accessible. A separate follow-up probe is to pin that smaller window, then request Fit my monitor and inspect settled bounds while pinned. This releases the fixed startup limits and must be recorded separately from the completed startup test. The public [widget API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) exposes pin status and pinning support, but no method to set the pinned state or arbitrary window position. No private state notification was invoked to simulate a successful pin or move.
