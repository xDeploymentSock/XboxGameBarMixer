# Roadmap

## Accepted checkpoint

Version **0.2.1.6** is installed with Windows package status OK, and the user accepted its live color appearance. Pairing, live video, pinning, black transparency and click-through are established. Main includes Desktop identity checks, usable-area fitting, Crisp HUD scaling, receiver resource reuse and opaque black removal.

The current throughput milestone accepts measured pipeline rates above 200 FPS with the source display at 244 Hz. Exact 240 distinct displayed source frames, capture-to-screen latency and automatic four-edge host coverage remain unproven. [Validation](VALIDATION.md) separates the measured results from these open questions.

Further feature work is paused at the user's requested break. The items below are future work when development resumes.

## Controlled quality and latency checks

- Compare fine colored HUD text, dark artwork and thin strokes against the source at the same scaling and stream settings. Separate codec/subsampling loss from keying and video opacity.
- Compare 60/120/240 requested FPS and bitrate settings with stock Moonlight on the same wired path.
- Measure capture-to-screen latency and distinct displayed frame IDs with a controlled changing source and suitable external capture. Receive/decode/Present-call counters remain stage-specific evidence.
- Isolate a separate control stream only with the user's approval; record when concurrent sessions affect the workload.

## Layout and lifecycle

- Verify usable-area fitting across manual moves, repeated resize, taskbar reservations, DPI changes and monitor changes.
- Keep the separate testing branch's Game Bar frame/coverage investigation isolated from main. Rendering must remain inside the Game Bar UWP widget.
- Record actual client bounds and all four gaps when testing host requests. A successful resize request alone does not establish monitor coverage.
- Exercise cancellation, reconnect, source restart, suspension, repeated activation and device removal with normal local workloads.

## Packaging and maintenance

- Keep reproducible dependency revisions and public license notices current.
- Evaluate signed distribution packaging separately from the existing local unsigned development installation.
- Run the portable core and repository checks on each checkpoint; run Windows hardware/widget checks when changes affect those paths.
- Preserve private logs and measurement archives locally. Review, commit and push important source/documentation changes, then verify the remote commit.

Earlier implementation tasks and experimental placement results are retained in [development history](DEVELOPMENT-HISTORY.md).
