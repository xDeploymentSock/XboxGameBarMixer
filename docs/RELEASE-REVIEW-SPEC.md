# Release review requirements

Origin: the user requested a new testing branch to integrate NoScreen after the UI redesign, with a full in-depth code review and cleanup for public release if integration cannot be delivered.

Review baseline: `4ddad76b678d12cd466f0cac204aa17eaf8b5a05` (main, installed UI version 0.2.1.10). Work branch: `codex/noscreen-testing`. The [NoScreen assessment](NOSCREEN-ASSESSMENT.md) records why the fallback applies.

## Requirements

1. Review the complete application snapshot, not only newly changed code: core contracts, network control/pairing, stream startup/shutdown, decoder and GPU frame lifetimes, rendering/keying, widget ownership/settings, build/deployment tools, tests, documentation, CI and third-party notices.
2. Address demonstrated correctness, security and release-hygiene defects. Add focused regression checks for behavioral fixes. Avoid speculative hot-path rewrites; preserve synchronization, encoded reference order and GPU leases.
3. Keep video inside the Game Bar UWP widget. Preserve view-only operation, saved settings and protected pairing. Preserve other clients' active Sunshine sessions and truthful Desktop application selection.
4. Preserve the saved HUD baseline: 2560 x 1440, 240 requested FPS, HEVC, 100,000 kbps, near-black cleanup, opaque retained colors, Crisp scaling and usable-area fit. Do not change these defaults as release cleanup.
5. Preserve other chats' work and main. Commit and push verified checkpoints on the new testing branch. Exclude private addresses, user paths, keys, credentials, captures, logs and generated packages.
6. Run relevant portable and Windows checks. Distinguish builds, simulated control tests, GPU fixtures, live Game Bar behavior, distinct displayed FPS and optical latency. Record unavailable checks and remaining uncertainty.
7. Prepare accurate public-facing build, configuration, troubleshooting and release guidance. Retain dependency notices, corresponding-source requirements and an explicit statement that the owner has not selected a project distribution license. Do not invent a license grant or publish an unapproved binary release.
8. Report actionable findings and their disposition, with separate standards and specification reviews. Document review coverage and remaining release gates; do not claim all bugs or all performance limits are eliminated.
