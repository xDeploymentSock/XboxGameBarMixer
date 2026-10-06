# Fixed startup coverage test branch

Branch: `codex/fixed-startup-coverage`. Baseline: main at a6a9a85, with the original 0.2.0.9 source/package retained. This experiment follows the [PoC source comparison](GAME-BAR-POC-COMPARISON.md); it is not a verified coverage fix.

## Prepared experiment

The experimental package is version 0.2.0.10. Its widget starts with initial/minimum/maximum client sizes all set to 2560x1440 **view pixels**, for the measured 2560x1440 monitor at scale 1. Both resize-support flags remain true. Activation preserves these manifest constraints, does not restore flexible limits, and defers any previously saved Reset without deleting the saved preference. No initial resize, full-screen, or centering request is made.

The experimental extension ID is `RemoteHudStartupTest`, displayed as **Software Fuser startup test**, so the original widget's saved host placement is not reused. The application/package identity is unchanged; pairing and Sunshine settings remain in the same protected app-local storage. Sunshine transport, decoding, composition attachment, transparency, click-through, and the renderer are unchanged.

Explicit Fit, Apply dimensions, and Reset retain their previous behavior and restore flexible limits. Use them only after recording the untouched startup result. Reset is the recovery path if the fixed-size window leaves controls inaccessible. Reopening should not be treated as a fresh measurement after Reset without checking the actual constraints and placement: the host may cache the experimental extension's geometry too.

## Build and live procedure

Build using the existing [build commands](BUILD-LATER.md). Building does not install the experiment. Until an explicit test deployment, the installed baseline remains 0.2.0.9.

1. Record the installed version and keep the existing baseline package. Close Software Fuser before explicitly deploying the experimental Release package with `tools/Deploy.ps1`; avoid deployment during gaming. Do not uninstall the application or clear protected app-local storage.
2. Open **Software Fuser startup test** from the Game Bar widget menu on the 2560x1440 display at 100% scale. Do not click Fit, Apply, full-screen fit, or Reset yet. Confirm that the local runtime log records fixed 2560x1440 startup limits and both resize flags enabled.
3. Click **Draw test pattern** with Black selected. Wait at least five seconds for host placement to settle. Record all four edge gaps and widget/client/video geometry; inspect whether the outer white border reaches the top and taskbar simultaneously. Configured dimensions and an initial transient full-sized layout are not coverage proof.
4. Pin, dismiss Game Bar, and reopen it. Check settled bounds, all four visible edges, black transparency, and click-through again. Distinguish host movement from app layout requests in the local log.
5. After recording startup behavior, separately test Fit and typed Apply. They release the experimental fixed limits and may reproduce the baseline host refusal. A startup-only pass is not proof that dynamic sizing works.
6. Use Reset if needed. Preserve private runtime logs/screenshots under ignored build output. Report measured geometry and visible results before changing another sizing variable.

The source main branch and the baseline 0.2.0.9 package remain available. Returning Git to main restores source only; it does not downgrade an installed 0.2.0.10 package. A package rollback requires an explicit Windows deployment that permits downgrade and preserves the package identity. No installation, rollback, or on-screen success is implied by a build.

## Verification status

Debug and Release widget builds completed with zero widget warnings/errors. Each dependency-library rebuild emitted four C4244 conversion warnings in the existing upstream FFmpeg libavutil/common.h header; no streaming or decoder source was changed. The existing core layout contracts passed 1/1 in both configurations. Inspection of the built Release manifest verified version 0.2.0.10, the original package identity, the fresh extension ID, all six fixed dimension values, and both resize flags enabled.

An independent package query still reports installed 0.2.0.9 with status OK, and the original Release package's SHA-256 is unchanged. The test package has not been installed or launched. No live fixed-startup result has been recorded. The previously measured baseline remains (0,46), 2558x1394 on the 2560x1440 monitor.
