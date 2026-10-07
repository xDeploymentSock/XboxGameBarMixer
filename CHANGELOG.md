# Changelog

Notable changes to Software Fuser, grouped using [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions are four-part Windows development-package checkpoints. Release dates are omitted because the imported Git history does not establish them. Version links compare implementation commits; they do not identify signed distribution releases.

Build results, live acceptance and measurement limits belong in [validation](docs/VALIDATION.md). Earlier 0.2.0/0.2.0.1 work predates the first tracked 0.2.0.2 manifest and is preserved in [development history](docs/DEVELOPMENT-HISTORY.md).

## [Unreleased]

### Added

- Documented passive trace-collection failures, owned active-producer/elevation controls and the requirement to validate display measurements before drawing optimization conclusions.
- Recorded installed 0.2.1.8 HEVC/H.264 live HUD receiver results and the user's preference for near-black cleanup, with display and gaming verification limits.

## [0.2.1.8]

Release development package installed and verified locally. Allocation reductions and a live receiver baseline are measured; displayed-frame timing and gaming impact remain pending.

### Added

- Repository publication/link audits, contributor guidance, formatting rules and CPU-only Windows/Linux core CI.
- README and changelog skills recorded in the project skill lock.
- Decode-only fixture benchmarking, decoder wrapper counters and callback-failure/restart coverage.
- Owned-input GPU draw benchmarking with D3D11 timestamps, disjoint rejection and alpha checks.
- Fixture input-preparation timings and optional 100 Mbps H.264/HEVC payload-stress fixtures.
- A bounded benchmark failure regression using owned unsupported-matrix input, plus a [scoped code review](docs/CODE-REVIEW.md).

### Changed

- Reuse decoder packet/receive wrappers per session and transfer hardware frame references into output leases instead of cloning them. Measurements and remaining gates are in [receiver performance](docs/RECEIVER-PERFORMANCE.md).
- Reorganized setup, architecture, current limits and documentation navigation in the README.
- Grouped package changes by version and change type, linked their implementation ranges and archived older development notes.

### Fixed

- Oversized unsigned bitrates passing validation before conversion to Moonlight's signed transport field.
- Developer fixture benchmark hanging while joining a sleeping render worker after decoder failure.

## [0.2.1.7]

### Added

- Hardware regression coverage for red/blue edges at unequal scaling, across Rec. 601/709 and full/limited NV12. Live shade stability remains pending.

### Fixed

- Crisp HUD mixing a selected source brightness sample with color sampled at a different position. Thin colored text now retains the same reconstructed source RGB across resize sampling phases.

## [0.2.1.6]

### Added

- **Remove black only** and **Remove near-black noise** presets with a hard cutoff and opaque retained colors.

### Changed

- Black presets restore 100% HUD opacity and disable edge blending and following Game Bar opacity for video.
- Older black profiles migrate once to exact black removal while retaining pairing, stream settings and placement.

### Fixed

- Opaque colored HUD pixels fading under the earlier brightness-based cleanup preset.
- Pinned Game Bar opacity fading the whole video visual unless explicitly enabled.

## [0.2.1.5]

### Added

- Latency and Present-gap percentile bounds, GPU-pressure diagnostics and retained-frame replacement counts.

### Changed

- Reused NV12 shader views and completed GPU queries to reduce receiver CPU draw cost.

## [0.2.1.4]

### Added

- Smooth/Crisp HUD scaling and a 1440p/HEVC/240-requested-FPS/100 Mbps HUD preset.

### Changed

- Reduced receiver presentation queuing to one frame.

### Fixed

- Video-fit calculations using the wrong origin when the widget client is offset.

## [0.2.1.3]

### Fixed

- Application refresh replacing the selected Desktop entry with an active Steam application.
- Launch/resume proceeding with a stale application ID or a different active application. Connect now validates the selected ID/name before startup and the active ID afterward.

## [0.2.1.2]

### Changed

- Organized settings into Connect, HUD, Layout and Details, with persistent connection controls, collapsible options and navigation for narrow windows.

## [0.2.1.1]

### Added

- Black-noise cleanup, brightness-based edge recovery and immediate application of fine key settings. These older black presets were replaced in 0.2.1.6.

## [0.2.1.0]

### Added

- **Apply video fit** and an adjustable taskbar reservation to scale the complete feed into the usable area, allowing vertical compression.

### Changed

- Kept negotiated stream dimensions separate from the widget's video destination.

## [0.2.0.9]

### Added

- Typed physical overlay dimensions, four measured edge gaps and an explicit full-screen compatibility check.

## [0.2.0.8]

### Changed

- **Fit my monitor** requests the full monitor size immediately; only explicit actions resize the widget, and only Reset centers it.

### Fixed

- App-driven shrink/recenter requests when reopening or pinning Game Bar.

## [0.2.0.7]

### Changed

- Clarified the pinned monitor-coverage control's label.

## [0.2.0.6]

### Added

- **Reset widget position**, restoring a smaller centered window and accessible move/resize controls without clearing pairing or profiles.

## [0.2.0.5]

### Added

- Widget/client/visible geometry, pin/visibility and layout-request diagnostics. This checkpoint was prepared before inclusion in 0.2.0.6.

## [0.2.0.4]

### Changed

- Tested fixed monitor-size constraints while investigating incomplete host coverage. Later versions replaced this fitting behavior.

## [0.2.0.3]

### Added

- Saved black-background key settings, exact-black defaults and optional near-black tolerance.

## [0.2.0.2]

### Added

- Initial tracked streaming baseline: Sunshine pairing, protected credentials, view-only Moonlight transport, hardware H.264/HEVC decoding and transparent Game Bar presentation.
- Packet, queue, host-processing, display and shutdown diagnostics.

[Unreleased]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/da43d2a00d152ba258a111e86ec8c25f59cb5f6c...HEAD
[0.2.1.8]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/5ae44b1d6a4a000e2d6c957b6de045aa8b6f7cc7...da43d2a00d152ba258a111e86ec8c25f59cb5f6c
[0.2.1.7]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/a9b929971a455030da4852ea5d3e818711bcff6b...5ae44b1d6a4a000e2d6c957b6de045aa8b6f7cc7
[0.2.1.6]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/e80fbbcc0fe33c3c435d58ddd9d8e4d7fb96641f...a9b929971a455030da4852ea5d3e818711bcff6b
[0.2.1.5]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/09ce9475434f0f4a97060d350892a1c0686a5e93...e80fbbcc0fe33c3c435d58ddd9d8e4d7fb96641f
[0.2.1.4]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/5db3a059f72435dbad26a6566cdd1f6bdee99a24...09ce9475434f0f4a97060d350892a1c0686a5e93
[0.2.1.3]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/fe282b41120974e4f0e093f2944aeb7a808cf958...5db3a059f72435dbad26a6566cdd1f6bdee99a24
[0.2.1.2]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/2c31b322a9aa3ff51b5d2813de4cfcf2a63099fc...fe282b41120974e4f0e093f2944aeb7a808cf958
[0.2.1.1]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/880f5fd71e54c5f10698549a8c52b38e65499071...2c31b322a9aa3ff51b5d2813de4cfcf2a63099fc
[0.2.1.0]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/b957ff0cf893d4ee4e9386fcf21b4aae84372e07...880f5fd71e54c5f10698549a8c52b38e65499071
[0.2.0.9]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/e10e4e006bff18d3ea1f400f493648567066e011...b957ff0cf893d4ee4e9386fcf21b4aae84372e07
[0.2.0.8]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/2362feda66f4f2d9fd2aa2abc3e108a38dca94ca...e10e4e006bff18d3ea1f400f493648567066e011
[0.2.0.7]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/4c4d53177039dff0cb643fa1c9afe1e6783c7de1...2362feda66f4f2d9fd2aa2abc3e108a38dca94ca
[0.2.0.6]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/cd54ca3ad603f0ea2385c1ab8c71bc204b6faaa6...4c4d53177039dff0cb643fa1c9afe1e6783c7de1
[0.2.0.5]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/e71a187d3562a27ebe9578a872ae7b13e4fdbb54...cd54ca3ad603f0ea2385c1ab8c71bc204b6faaa6
[0.2.0.4]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/f5a848f5857540c363dcfb8bcf8a667152d0ecdb...e71a187d3562a27ebe9578a872ae7b13e4fdbb54
[0.2.0.3]: https://github.com/xDeploymentSock/XboxGameBarMixer/compare/bb428367a625bb6037391372dfabe09da1bb16b9...f5a848f5857540c363dcfb8bcf8a667152d0ecdb
[0.2.0.2]: https://github.com/xDeploymentSock/XboxGameBarMixer/commit/bb428367a625bb6037391372dfabe09da1bb16b9
