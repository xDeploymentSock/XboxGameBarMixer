# Release guide

Main contains the usable-area Game Bar widget. The current package version is **0.2.1.13**; this is an unsigned development preview. Capture blocking is experimental; see [capture privacy](CAPTURE-PRIVACY.md) for verification limits.

## Prepare main

1. Work from a clean checkout of main. Keep active testing branches separate.
2. Review the diff against the previous release commit and update [CHANGELOG.md](../CHANGELOG.md) with user-visible changes.
3. Run publication/link audits, formatter checks, timing-summary tests and portable core tests. [CONTRIBUTING.md](../CONTRIBUTING.md) lists commands.
4. Build the UWP widget in Debug and Release using [the build guide](BUILD-LATER.md). Check build output for errors and warnings.
5. Verify the package manifest matches the source identity/version and the packaged executable matches the built executable. Keep local packages, receipts and logs ignored.
6. For runtime changes, install during an idle window and verify pairing, Connect/Disconnect, menu reopening, keying, fitting and click-through. Recheck saved settings and pairing after updates. Record results in [validation](VALIDATION.md).
7. Review and audit staged files, commit, push and confirm main's GitHub checks pass for that exact commit.

A formatter-only change still needs native compilation. It does not establish new performance or hardware acceptance results.

## Public distribution

Before publishing a downloadable package:

- Select a license for project code and review the retained [dependency notices](../THIRD-PARTY-NOTICES.md).
- Choose and verify the signing/distribution method. The current `Deploy.ps1` is for local unsigned development installation.
- Produce the package from the chosen clean release commit. Publish the version, checksum, prerequisites, installation instructions and known limits with it.
- Tag that commit only after its checks and release notes agree. Use the four-part Windows package version; avoid tagging an unverified local build.

These decisions are still open. Main's source checks and a successful local installation do not make a package a signed public release.

## Performance claims

The UI requests 1440p at 240 FPS. Receive, decode and Present counters describe separate pipeline stages; they do not prove 240 distinct displayed frames or capture-to-screen latency. Use [controlled performance procedures](RECEIVER-PERFORMANCE.md) for those claims, and preserve the recorded limitations in release notes.
