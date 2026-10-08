# Public release preparation

Current state: installed main UI checkpoint 0.2.1.10; unsigned testing candidate 0.2.1.11 on `codex/noscreen-testing`. The [review report](RELEASE-REVIEW.md) records completed checks and remaining limits. A successful build or local unsigned installation is not a public distribution release.

## Owner and source decisions

1. Select a distribution license for project code. Retained dependency licenses do not grant a license for the project's own code. Follow the combined application's obligations documented in [third-party notices](../THIRD-PARTY-NOTICES.md), including linked Moonlight GPL-3.0 corresponding source; do not assume a permissive binary license.
2. Prepare the exact corresponding source for the proposed binaries: application, pinned Moonlight/submodules and view-only modification, dependency sources/options, build scripts and local vcpkg overlay. A Git checkout or private recovery ZIP alone is not the complete dependency-source bundle.
3. Choose a supported signed delivery path and publisher identity before public packaging. The current development publisher and `AllowUnsigned` installer are local development tools. Evaluate settings/pairing identity migration if the publisher changes.
4. Retain all applicable dependency and Microsoft notices in the proposed package, review redistribution terms for the actual dependency/runtime payload, and publish build instructions matching that version.

NoScreen has not been included. Any later driver integration requires a separate licensed/signed design and compatibility work; see the [assessment](NOSCREEN-ASSESSMENT.md).

## Candidate verification

Use a clean intended branch and an idle receiver. Follow the [build guide](BUILD-LATER.md) for dependencies and toolchain prerequisites.

```powershell
python tools/audit_repository.py
python -m unittest discover -s tests -p '*_tests.py'
.\tools\Build.ps1 -Target Core -Configuration Release
.\tools\Build.ps1 -Target GPU -Configuration Release
.\tools\Build.ps1 -Target Decoder -Configuration Release
.\tools\Build.ps1 -Target Control -Configuration Release
.\tools\Build.ps1 -Target Widget -Configuration Release
.\tools\Deploy.ps1 -Configuration Release -ValidateOnly
```

Run the [combined session checks](BUILD-LATER.md#combined-windows-session-checks) and matching Debug checks. Inspect warnings and actual test results. Confirm package manifest identity/version/architecture, ZIP integrity, executable/dependency hashes against the tested output and all retained license payloads. Sanitizers currently cover portable core only; do not imply they cover UWP or hardware code.

For a local acceptance installation, use `Deploy.ps1` in an authorized idle window; it closes the widget and requests Windows elevation. Independently verify installed identity, version, status and executable hash. Compare existing saved settings and protected pairing before/after without copying keys into reports.

## Native acceptance

Record package version, DPI, requested/actual widget geometry and the test outcome without publishing private hosts or PINs.

| Scenario | Expected result |
| --- | --- |
| Connections/Adjustments/Advanced at normal and narrow widths | Controls remain reachable, keyboard navigation/focus work, status stays readable, native high-contrast/theme behavior is legible |
| Existing profile/update | Saved stream, near-black key, opaque retained colors, Crisp scaling and usable-area fit remain; pairing survives matching-identity updates |
| Background removal and placement | Owned pattern colors remain opaque, reserved black/near-black is removed, whole feed fits usable area, click-through reaches the local app; host size limits are reported truthfully |
| Startup resize | Resize while connection is pending; the surface matches the latest usable area when streaming starts |
| Failed startup and retry | Use an unavailable test source, resize during connection, wait for failure, then retry or draw a preview; retained surface matches current host size |
| Disconnect resize | Resize as the stream stops, then preview/reconnect without another manual resize; current host size is retained |
| Suspend/resume | Use debugger lifecycle controls or an observed real suspension; stream stops, the original widget view returns to fresh idle settings, controls work, no automatic reconnect occurs |
| Close/reopen/repeated activation | No duplicate widget ownership, stale resume navigation or inaccessible dead page; standalone settings do not replace the retained widget view |
| Save failure | Induce a settings-store failure in an owned test/debug setup; HUD preset must retain the error instead of reporting a successful save |
| Active Sunshine app conflicts | Desktop maps to its returned app ID; another client's active app/control session is preserved; mismatched active app produces a clear conflict |
| Decoder/device/transport failure | Failure status appears, cancellation/disconnect completes and reconnect or device recreation works; no retained GPU lease is released too early |

Portable token/session fixtures validate specific logic; they do not replace these native host checks. Do not perform GPU-heavy or disruptive tests during active gameplay.

## Publication checkpoint

Review/stage only intended source, tests and public docs. Run `git diff --check`, `git diff --cached` and `python tools/audit_repository.py --staged`, then commit/push and confirm remote SHA equality. Ensure required CI jobs pass for that commit. Keep host details, keys, app state, screen captures, local logs, build caches, unsigned packages and recovery ZIPs ignored.

Write release notes around the final implementation and proven behavior. Distinguish receive/decode/Present rates from displayed frames, and CPU/GPU timing from optical latency. Publish a release artifact only after the owner/source/delivery decisions and native acceptance above are resolved.
