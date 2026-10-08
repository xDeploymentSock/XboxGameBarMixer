# Main code review

This review follows the installed 0.2.1.7 color checkpoint. It covers stream startup/shutdown, decoded-frame ownership, renderer resources, settings validation, and local verification tools. The installed widget and source-control session were left running. The production fixes below are included in the subsequently installed 0.2.1.8 package; independent installation evidence is in [validation](VALIDATION.md).

## Correctness fixes

| Finding | Trigger and result | Verification |
| --- | --- | --- |
| Unsigned bitrate accepted by a signed transport field | A typed bitrate above 2,147,483,647 kbps passed validation, then converted to a negative Moonlight bitrate. Reject it before host control begins. | The boundary regression failed before the fix; zero, maximum signed, first overflowing, and maximum unsigned values are covered. |
| Benchmark error cleanup could hang | BT.2020 decoding fails before publishing a frame. Unwinding requests the render worker to stop, but the original condition-variable wait receives no notification. Use C++20 stop-aware waits so destruction can join the worker. | An owned H.264 fixture with rewritten matrix metadata reproduced a hang beyond five seconds. A subprocess regression requires the expected decoder error and exit code within five seconds. |

The unsupported-matrix fixture changes only VUI metadata on generated test content. It is rebuilt locally and remains outside Git. The regression runner terminates a hung benchmark instead of leaving a child process behind.

## Performance decisions

Two short Release baselines each submitted 1,200 owned 1440p frames at 240 requested inputs per second. CPU decoder-submission p95 was 0.2323 ms for H.264 and 0.2164 ms for HEVC. Draw CPU time including lock wait had p95 of 0.0208 and 0.0187 ms respectively. These runs had variable presentation pacing and were not controlled live latency tests or before/after optimization comparisons.

The subsequent [performance pass](RECEIVER-PERFORMANCE.md#current-optimization-pass-0218-candidate) measures and implements packet/receive-wrapper reuse and frame-reference transfer. At the time of this initial review, that work remained a profiling candidate. This review does not establish that changing it would materially reduce latency, so the decoder's validated frame leases and shared D3D11 context lock are retained. The existing renderer already caches plane views and completion queries, keeps only the latest decoded display frame, and wakes on frame notifications and presentation readiness.

The production render loop has no fixed four-millisecond polling wait. Its one-millisecond fallback is used only when GPU read slots are occupied. No global timer-resolution change is added. Local scalar declarations and compile-time type deduction do not themselves cause heap allocation.

## Verification and limits

Release and Debug results are recorded in [validation](VALIDATION.md). The checks exercise portable contracts, controlled pairing and Desktop selection, encrypted credential storage, GPU keying/color sampling, sequential/concurrent hardware decoding, and benchmark failure cleanup.

No package deployment or real-host performance run was performed for this review. Installed 0.2.1.7 shade stability, Game Bar display timing, and representative local-game load still need live verification. Host-frame coverage remains a separate testing-branch investigation.

## UI checkpoint 0.2.1.10

Local review of `ac19d13...81c738c`, the three-section UI feature commit. The requirements were the user's request to clean up the UI with the installed design skill, make current settings the baseline and keep the interface simple. The standards sources were AGENTS.md, CONTRIBUTING.md, .editorconfig, C++ Core Guidelines and the installed UI skill's native-layout guidance. Both independent review agents stopped at usage limits without returning reports; the primary agent performed the two reviews below. This is not an independent review.

### Standards

No implementation findings. C++ changes stay on the XAML thread, reuse existing controls/settings keys, validate section indices and preserve renderer/session ownership and shutdown. Named controls and handlers compile in Debug/Release; theme keys exist in the target SDK. No decoder, transport or rendering hot path changes. Native controls provide normal focus/pressed/disabled behavior, while narrow windows use a section dropdown. Native keyboard, enlarged text, high-contrast and screen-reader behavior remain unverified.

### Spec

No implementation findings against “make my current settings the baseline and keep things simple.” Connections, Adjustments and Advanced replace four sections. Routine controls are visible; tuning and placement groups are collapsed. Fresh defaults match the selected non-private baseline, saved values override them, and opening/switching sections initiates no resize or stream action. The 20 saved values and two protected pairing files remain unchanged after installation. Main was updated without touching the separate testing checkout. The rendering path remains inside Game Bar UWP. Native visual simplicity and live pinned behavior still need the user's verification.

Findings: Standards 0; Spec 0. Both axes retain the native-interaction verification gap.


## Menu recovery (0.2.1.12)

Reviewed the focused main working-tree changes against `4ddad76`, with the user's
missing-menu report and [repair requirements](MENU-RECOVERY.md#repair-requirements)
as the spec. Standards sources: AGENTS.md, CONTRIBUTING.md, .editorconfig,
installed C++/verification/UI skills and the twelve Fowler smell heuristics.
Separate standards and specification agents completed static reviews.

### Standards

Zero unresolved hard-rule findings and zero actionable heuristic smells. Resume
uses the captured UI dispatcher, retains the original widget/window/frame, and
rejects stale/duplicate tokens. Shutdown is idempotent; timer exceptions are
contained. Profile/pairing code is unchanged. The repeated host/frame lookup keeps
lifecycle ownership explicit and did not warrant a new abstraction.

### Spec

Zero unresolved findings. Both reviewers initially identified queued bounds/DPI
callbacks reaching XAML after shutdown. Every queued delegate now checks the
shutdown flag, as do layout/state/load handlers. Fresh UWP builds followed that
repair. Resume reloads saved idle settings, with no automatic connection, profile
write or unrelated testing-branch changes.

The agents did not independently rerun tests. The primary agent ran the checks
recorded in [validation](VALIDATION.md#installed-menu-recovery-02112). Static review
and compilation do not establish native menu visibility. After installation the
user confirmed normal menu return; the collected log does not establish a full OS
suspension/resume cycle.

## Release readability cleanup

Compared the staged cleanup against `d9a3cb8`. Requirements: prepare main for release, make native code readable, and keep the README short and understandable. No runtime feature or package-version change is included.

### Standards

Independent review: 0 unresolved documented-standard findings and 0 actionable judgment-call smells. All 28 reformatted native files retain the original lexical tokens, comments and string literals. Include order, ownership, locking and frame lifetimes are preserved. The formatter checks 32 tracked project-owned native files using a pinned version. FFmpeg headers are classified as external; project code retains `/W4 /permissive-`.

### Spec

Independent review: 0 unresolved findings. The README focuses on local installation and everyday use; detailed guides have an index. Coding standards, formatter enforcement, sanitized portable CI and a release guide support source readiness. License selection and signing/distribution remain explicit decisions before a public package is published.

### Verification

Fresh Debug/Release portable contracts, 12 timing-summary tests, MSVC AddressSanitizer contracts, formatting, publication/link audits and staged whitespace checks pass. Complete Debug/Release widget builds, including streaming libraries, finish without compiler warnings or errors. Both MSIX packages pass CRC and identity checks, and their executable bytes match the corresponding build.

Linux AddressSanitizer/UndefinedBehaviorSanitizer checks are enforced by the new CI job. Local MSVC verification supplies AddressSanitizer only. This cleanup does not establish new GPU, live performance or OS suspension results, and it does not reinstall the widget.

Findings: Standards 0 unresolved; Spec 0 unresolved.
