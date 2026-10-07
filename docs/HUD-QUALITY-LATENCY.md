# HUD fit, text quality and receiver latency

Version 0.2.1.4 fits the complete source frame against the actual XAML client origin, including changes when Game Bar is pinned or reopened. The previous calculation used Game Bar's frame coordinates: a reported frame Y of 0 could accompany a client Y of 44, placing the bottom of the feed below the reserved taskbar area. Coverage diagnostics now use the same client coordinates as fitting.

The frame still fills the usable rectangle with unequal-axis scaling. It is not cropped or changed to 1080p. Game Bar controls the outer window; video cannot draw outside its hosted client. A top strip outside that client still requires manual widget placement or the separate coverage investigation.

## Live verification

1. Reopen Software Fuser through Win+G. On Connect, click **1440p HUD preset**. It saves 2560 x 1440, HEVC, 240 requested FPS, 100,000 kbps and Crisp HUD scaling. Reconnect for the stream settings to take effect.
2. On Layout, enable **Keep full video above taskbar**, enter the taskbar's physical height and click **Apply video fit**. Use 48 pixels as the starting estimate at 100% display scaling; use the actual height if different. Pin and close Game Bar. Check the source's four corner markers, top position and bottom edge above the taskbar. Reopen and close Game Bar again to check that the fit updates.
3. On HUD, compare **Crisp HUD** with **Smooth**, clicking **Apply key settings** after each change. From 0.2.1.7, Crisp uses nearest luma sampling and reconstructs filtered NV12 chroma at that same source pixel center; Smooth uses bilinear sampling on both planes. Crisp avoids adding interpolation blur to thin strokes but can produce stair-stepping or shimmer during motion. Source antialiasing and codec artifacts remain.
4. On version 0.2.1.6 or later, use **HUD â†’ Remove black only** when judging color and contrast. It restores 100% HUD opacity, disables edge blending and keeps video independent of pinned Game Bar opacity. **Remove near-black noise** removes compression residue with a hard cutoff while retaining opaque colors above it. Optional edge blending and following Game Bar opacity are under Fine tuning; leave both off for natural colors. See [Black removal](BLACK-CLEANUP.md).
5. On Details, check the actual resolution/codec and receive/decode/Present rates during source motion. The local callback-to-Present average and maximum measure the receiver segment only. They exclude source processing, network arrival/assembly, Game Bar composition and monitor scanout. They are not end-to-end latency.
6. Compare a run with the separate source-control Moonlight session connected and one with it disconnected, if source access permits. An extra stream consumes source capture/encode resources. The widget preserves that session and does not disconnect it automatically.

## Receiver changes and evidence

The render worker now waits on a condition with a predicate rather than a fixed 4 ms timer. Publishing under that condition's mutex prevents lost wakeups. It waits for a DXGI presentation slot before choosing the latest decoded frame, leaving the decoder context unlocked during that wait. The composition swap chain limits queued frames to one. Nonblocking, sync-interval-zero presentation avoids holding the shared decoder context while waiting for a refresh or a full presentation queue. GPU read leases are retained until completion; compressed reference frames are still decoded in order.

The approach follows [Microsoft's waitable swap-chain guidance](https://learn.microsoft.com/en-us/windows/uwp/gaming/reduce-latency-with-dxgi-1-3-swap-chains) and [flip-model Present semantics](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present). It remains a composition surface inside the UWP widget.

One before/after Release benchmark used the same owned 2560 x 1440 HEVC inter-frame fixture, 1,200 frames paced at 240 FPS, on the same receiver. It excludes Sunshine, transport, Game Bar and scanout; these results cannot be used as screen latency or displayed FPS.

| Receiver fixture metric | Before | After |
| --- | ---: | ---: |
| CPU decoder submission p95 | 9.57 ms | 0.20 ms |
| Local fixture feed to accepted Present return p95 | 15.89 ms | 0.38 ms |
| Decoded frames | 1,200 | 1,200 |
| Present calls | 661 | 1,193 |
| Attempts with no available GPU read slot | 536 | 0 |

Three six-second live HEVC/240-requested-FPS diagnostics received approximately 234â€“238 frames/s after startup, with zero decode errors and normal joined shutdowns of about 0.17 seconds. The final run measured a local decoded-callback-to-accepted-Present average of 0.30 ms, with a 33.25 ms startup maximum. They ran offscreen while the source-control session remained connected. Installed-widget fit, readability and perceived latency still require the steps above.

## Source tuning

Start with the HUD preset and Sunshine's NVENC P1 setting. Sunshine documents that slower presets trade encoding latency for compression, so raising bitrate is preferable when the wired link has capacity. Keep quarter-resolution two-pass initially. A one-pass comparison is an optional later experiment: Sunshine notes possible bitrate spikes when two-pass is disabled. See [Sunshine's NVENC configuration](https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2configuration.html#nvenc_preset). Source settings are not changed by this widget update.

The foundation still uses SDR NV12 4:2:0. Thin coloured details can remain softer than native RGB artwork; the new preset does not enable 4:4:4 or promise lossless text.
