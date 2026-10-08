# Release review: testing candidate 0.2.1.11

Reviewed on 2026-10-07 locally (2026-10-08 UTC). Baseline: main commit `4ddad76b678d12cd466f0cac204aa17eaf8b5a05`, installed UI checkpoint 0.2.1.10. Repairs are isolated on `codex/noscreen-testing`; main and the separate host-coverage worktree are unchanged.

The user requested a full application review and public-release cleanup if NoScreen could not be integrated. The [source assessment](NOSCREEN-ASSESSMENT.md) explains the driver, compatibility and licensing boundaries. No NoScreen source was copied and no driver was installed. The fallback requirements are recorded in the [review specification](RELEASE-REVIEW-SPEC.md).

Candidate 0.2.1.11 is an unsigned development build, currently uninstalled. This review prepares the project for release decisions; it does not certify a signed public release or eliminate every defect.

## Scope and method

The review covered the full baseline snapshot, then the repair diff. Separate reviewers assessed repository/C++ standards and the originating requirements. Findings were checked against source before repair; targeted regression tests were added for reproducible session failures. Existing fixtures were used for control, protected storage, GPU output, hardware decoding and shutdown. No speculative performance rewrite was introduced.

| Area | Reviewed behavior |
| --- | --- |
| Portable core | Configuration limits, application selection, frame contracts, latest-frame mailbox, layout requests, timing and worker-activity synchronization |
| Sunshine control | Pairing challenge flow, protected identity ownership, pinned server-certificate verification, bounded requests/XML handling, cancellation and active-app conflicts |
| Stream session | Callback exception boundaries, transport ownership, connection/startup/stop ordering, resize handoff and worker lifetime |
| Decoder/rendering | Packet/frame reuse, decoded GPU leases, context synchronization, in-flight query retirement, SDR color conversion, opacity/keying and scaling |
| Widget | Game Bar activation/view ownership, async lifetime, saved profile feedback, suspend/close/resume, explicit placement and compact menu behavior |
| Delivery | Pinned source/overlays, UWP linkage, MSIX contents, deployment verification, private archives, publication audit, CI and retained dependency notices |
| Diagnostics/docs | Owned fixtures, bounded measurement tools, truthfulness of frame/latency claims, current controls and release guidance |

Standards sources: [AGENTS.md](../AGENTS.md), [CONTRIBUTING.md](../CONTRIBUTING.md), the installed C++ and code-review skills, and their ownership/concurrency guidance. The separate review axes remain below; they are not combined into one severity ranking.

## Standards

| Finding | Disposition and evidence |
| --- | --- |
| P2: startup discarded a resize queued after renderer ownership transferred | Preserve the pending resize in `begin`. A test against production session startup and a real D3D11 surface fails on the baseline and passes after repair. Transport is simulated. |
| P2: allocations escaped `noexcept` connection callbacks | Format into bounded stack buffers; allocate status storage inside its exception boundary. Injected allocation failures in stage/termination callbacks fail the baseline and pass after repair. Terminal completion survives lost status text. |
| Configuration could race with key updates | Initial assignment now uses the same state mutex as `set_key`. Existing UI busy gating reduced exposure; this is interface hardening, not a claimed observed live race. |
| Shutdown could stop a XAML timer outside its catch boundary and touch XAML again on a later worker-thread destructor | Make page shutdown idempotent and contain timer cleanup. Shutdown still cancels/stops owned work. Native compilation passes; live lifecycle checks remain pending. |
| P2: a resize queued during Disconnect or failed startup could leave the retained preview surface stale | On UI ownership return, synchronize the surface to current host dimensions through one helper. It covers both failed Connect and normal Disconnect. Native compilation and session resize fixtures pass; the UI failure path needs live acceptance. |
| P2: explicit archive inputs could include credential filenames | Reject protected/pending/key stores, environment files, local settings and credential directories. Table-driven synthetic tests cover these inputs, tracked-only selection and outside-root and parent-traversal rejection. |

Final standards re-review found **0 unresolved confirmed findings** after these repairs. The reviewer independently inspected source; verification commands were run by the implementation agent. No P0/P1 issue was confirmed during the review; that is not proof that none exist.

## Spec

| Finding | Disposition and evidence |
| --- | --- |
| Suspending permanently shut down the retained settings page | Keep the original Game Bar widget/CoreWindow/Frame, stop the session, and dispatch resume to the UI thread with a generation token. Resume creates fresh idle settings without reconnecting. Portable tests reject stale/duplicate tokens; native host resume is pending. |
| HUD preset overwrote a save failure with a success message | The save helper returns success explicitly; preset feedback is conditional. The error remains visible. Native build verified; induced storage-failure UI acceptance is pending. |
| Public docs described an unlinked widget, obsolete controls and retired opacity behavior | Update dependency, background-removal, architecture, README, build and release guidance from current source. Publication audit checks tracked link targets and private-data patterns. |

The preserved baseline is 1440p, 240 requested FPS, HEVC, 100 Mbps, opaque near-black cleanup, Crisp scaling and fitting above the taskbar. No HWND companion renderer, automatic reconnect, remote input forwarding, source-control termination or NoScreen substitution was added.

Final specification re-review found **0 unresolved confirmed findings**. The earlier missing release documents and historical control-label mismatch are resolved. Owner licensing and live host acceptance are explicit release gates rather than completed work.

## Verification

| Check | Result and scope |
| --- | --- |
| Windows native suite, Debug and Release | 22/22 passed in each: control/pairing/cancellation, protected credentials, core contracts, GPU shaders, H.264/HEVC hardware decode/concurrent presentation, benchmark failure shutdown and three session regressions |
| Session regression validity | All three fail against baseline `OverlaySession` and pass after restoring the repaired implementation. No connection to a real source PC. |
| Python tool contracts | 20/20 passed locally, including Windows-only synthetic package validation without installation |
| MSVC AddressSanitizer | Portable core 1/1 passed with `/fsanitize=address /Zi`, exception unwinding enabled and non-incremental linking. Does not sanitize the UWP/hardware/transport paths. |
| MSVC static analysis | Portable core `/analyze` build completed without diagnostics; 1/1 core test passed. Not a whole-application static-analysis claim. |
| UWP Debug/Release and streaming libraries | Built successfully for x64/SDK 10.0.26100.0. Final widget logs contain no compiler warnings/errors; dependency headers retain upstream warnings in separate builds. |
| PowerShell tools | All tool scripts parsed; synthetic `Deploy -ValidateOnly` fixtures accept matching manifests and reject mismatched version/architecture/publisher |
| Prepared MSIX contents | Final Debug/Release CRC, manifest/version/x64 and executable hashes verified; all nine notice files and nine Release/ten Debug DLLs match their build inputs. `ValidateOnly` passed for both; candidate remains uninstalled. |
| Private recovery archive | Created from staged source/current package/selected local logs; 150 input hashes and ZIP CRC verified. Records installed 0.2.1.10 separately from prepared 0.2.1.11. Archive/receipt remain ignored and are not public release bundles. |
| GitHub Actions | Portable Windows/Linux and Linux ASan/UBSan jobs added; push-run results pending |

The native suite uses owned fixtures and offscreen surfaces. It does not certify Game Bar composition, 240 distinct displayed frames, source quality, capture exclusion, game impact or optical latency. Existing measured performance remains in [validation](VALIDATION.md) and [receiver performance](RECEIVER-PERFORMANCE.md); no latency gain is claimed for this correctness cleanup.

## Remaining acceptance and release gates

See the [public release checklist](PUBLIC-RELEASE-CHECKLIST.md) for the concrete acceptance procedure. In particular:

- Live Game Bar menu/keyboard/high-contrast checks and suspend/resume/close/reopen acceptance remain pending.
- Resize during failed Connect and Disconnect, retry, save-error feedback and GPU-device-removal behavior need native acceptance.
- Root distribution licensing, the complete matching corresponding-source bundle, dependency redistribution review and a signed release delivery path remain owner decisions/work.
- Automatic simultaneous top/taskbar coverage remains the separate investigation; this candidate preserves usable-area fitting.
- HDR/P010/4:4:4 support, distinct displayed-frame proof and optical latency remain outside this cleanup's verified scope.

Local build logs, package hashes, archive receipts and any future runtime evidence stay ignored. Public Git contains authored source, tests and documentation only.
