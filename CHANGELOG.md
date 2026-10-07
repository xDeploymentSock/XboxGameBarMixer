# Changelog

Development checkpoints use the Windows package's four-part version. These notes describe implemented changes; [validation](docs/VALIDATION.md) records which builds and live checks were completed. Older experiments are preserved in [development history](docs/DEVELOPMENT-HISTORY.md).

## Unreleased

- Reorganized the README, build guidance and backlog around the accepted 0.2.1.6 checkpoint.
- Added repository publication/link checks, contributor guidance and CPU-only Windows/Linux core CI.

## 0.2.1.6

- Added **Remove black only** and **Remove near-black noise** presets that retain surviving decoded colors with opaque alpha.
- Restored full HUD opacity for black presets and made following Game Bar video opacity an explicit option.
- Migrated older black profiles once while retaining pairing, stream settings and placement.
- Live color appearance was accepted by the user after installation.

## 0.2.1.5

- Reused NV12 shader views and completed GPU queries to reduce receiver CPU draw cost.
- Added bounded latency/submission-gap diagnostics and accurate retained-frame replacement counts.

## 0.2.1.4

- Corrected fitting relative to the client origin and added selectable Crisp HUD scaling.
- Added a 1440p/HEVC/100 Mbps HUD preset and reduced receiver presentation queuing.

## 0.2.1.3

- Validated the selected Sunshine application before launch/resume and verified the active application afterward.
- Retained the chosen application through refresh and reported conflicts instead of resuming another app as Desktop.

## 0.2.1.2

- Reorganized settings into Connect, HUD, Layout and Details, with persistent connection/status controls and collapsible fine tuning.

## 0.2.1.1

- Added black-background cleanup presets and immediate key-setting application. The older brightness-based edge treatment was replaced by the opaque black presets in 0.2.1.6.

## 0.2.1.0

- Added **Apply video fit** to scale the entire feed into the usable area above an adjustable taskbar reservation, allowing vertical compression.
- Kept negotiated stream dimensions separate from the widget's visible destination.

## 0.2.0 through 0.2.0.9

- Established Sunshine pairing, protected credentials, hardware decode, transparent video, click-through and measured live throughput above 200 FPS.
- Added explicit monitor-fit/reset requests, typed physical widget dimensions, geometry diagnostics and a full-screen compatibility check.
- Host rejection and the unresolved top/taskbar coverage tradeoff are retained in the validation record.
