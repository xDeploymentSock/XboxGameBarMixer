# Extra-height startup and inner video viewport

This experiment remains inside the Game Bar UWP surface on `codex/fixed-startup-coverage`. The preserved baseline and the pinned-probe checkpoint remain available. Sunshine transport, decoder, requested source resolution, and rendering shaders are unchanged.

The earlier 2560x1440 startup fixture settled at Y=-44, losing the bottom 44 rows. The subsequent pinned-only 2560x1440 resize test also failed: Game Bar returned false and retained 480x700. Repeating Fit cannot enlarge the drawing when the host refuses to enlarge its client.

## Measured geometry experiment

Version 0.2.0.13 changed only the manifest: a fresh `RemoteHudOverscanTest` extension, displayed as **Software Fuser overscan test**, declares initial/minimum/maximum dimensions of 2560x1484 view pixels with both resize flags enabled. It adds 44 rows to the height that previously settled at Y=-44. A fresh extension avoids reusing the startup fixture's cached smaller geometry.

Both widget builds passed with zero warnings/errors, and the Release package was installed and independently verified with status OK. Public URI activation obtained settled widget, client, and visible bounds (0,-44,2560,1484), and a 2560x1484 video host. This rectangle spans the complete monitor. The initial 2560x1440 client was transient and was not used as the result. The widget was unpinned and hidden in pinned-only mode, so these coordinates alone did not prove visible coverage.

The [manifest guide](https://learn.microsoft.com/en-us/xbox/game-bar/guide/pkg-manifest) documents startup sizing but does not promise full-screen behavior. The [public widget-control URI](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widgetcontrol) is observed to activate this installed host despite the page's older external-activation caveat. The experiment makes no private host-interface calls.

## Inner drawing in version 0.2.0.14

Only the overscan fixture adds a 44-view-pixel top margin to `VideoHost`. A 1484-row client then contains a 1440-row video viewport beginning at local Y=44, which combines with the measured client Y=-44 to begin at screen Y=0. The composition visual and renderer size follow this viewport through the existing size-change handler. The incoming 2560x1440 feed therefore has a 2560x1440 destination; the extra rows contain transparent padding rather than a stretched picture. This is a fixture for the original monitor at scale 1, not automatic support for arbitrary monitor sizes or offsets.

The settings card also starts lower so its controls remain on-screen. Reset removes the margins and requests the established smaller centered window. Fit, Apply, and the older pinned constraints test are separate experiments and may replace the extra-height geometry; avoid them while testing this fixture.

`tools/RunOverscanCoverageProbe.ps1` sends exactly `coverage=startup` to the installed overscan extension. The app requires an idle widget, draws a black diagnostic without saving the stream/key profile, then records the viewport alignment and pin/visibility/mode after five seconds. It requests no resize or centering. An aligned result also requires that client and visible bounds contain the complete viewport. The script stores filtered local evidence in ignored build output. Exit zero means a completed observation; it does not prove visible coverage, pinning, or retention.

Both 0.2.0.14 widget builds passed with zero warnings/errors. Deployment and an independent package query verified installed Release 0.2.0.14, status OK, with the original package identity. The URI probe drew a black diagnostic into a 2560x1440 video host and completed after five seconds with `geometryAligned=true`. Settled client/widget/visible bounds were (0,-44,2560,1484), with video-local origin (0,44). The resulting video rectangle is exactly (0,0,2560,1440). Pinning and visibility were both false in mode 1, so the live visual acceptance remains pending. The UI's older coverage prefix still compares outer widget size as well as video size; interpret the logged video origin, dimensions, and edge gaps for this larger-client fixture.

## Live acceptance still required

Microsoft's [Game Assist FAQ](https://support.microsoft.com/en-au/edge/microsoft-edge-game-assist-faq) says to move the pointer to the top screen edge directly above a widget when its title bar is hidden. This provides a possible way to expose Pin without resetting the oversized client. Its applicability to this custom widget remains a live hypothesis; the user has been asked to test the top edge near the upper-right corner with Game Bar open.

The user reported that the title bar stays hidden. New live logs show the host shifting the oversized client to Y=-88 in foreground mode, with its bottom at 1396. The settled visible foreground client therefore does not contain the whole monitor. Dismissal restores the widget bound to Y=-44, where the client can contain all rows, but it remains unpinned and hidden. This contradicts any inference that 1484-row startup height alone establishes visible four-edge coverage. The containment contract explicitly rejects the foreground (-88,1484) case.

Open **Software Fuser overscan test** through Win+G and draw the diagnostic, or run its probe. Check the outer white border at all four monitor edges including over the taskbar; the cyan box is intentionally inset. Record visible/pinned status and the settled geometry separately. The pin button may remain off-screen because the host title bar is outside the video viewport. Do not treat the extra height as a pin-control fix.

Full success also requires pinning, black transparency, click-through, and retained placement after dismissing/reopening Game Bar. Native Windows UI tooling remains unavailable, so user observation is required for this visual check. No separate desktop renderer or feed rescaling is introduced.

## Prepared retention change in 0.2.0.15

The built 0.2.0.15 package retains the same overscan extension and 2560x1484 startup dimensions. Installed 0.2.0.14 is left available for the pending visual/pin check; 0.2.0.15 has not yet been deployed or live-verified.

Its inner viewport is explicitly the monitor's video size. When settled widget, CoreWindow, and XAML client geometry agree and that client contains the entire original monitor, only the inner padding is adjusted to cancel the measured client offset. Moving a 1484-row client from Y=-44 to Y=0 changes top padding from 44 to zero, preserving the same 1440 video rows. Host callbacks make no resize or centering requests. A client missing an edge is rejected by the containment contract rather than treated as permission to shrink the source. The fixture remains scoped to the monitor whose global origin is (0,0).

Fit uses this inner layout when the existing oversized client contains the monitor. Other explicit host-sizing experiments restore the normal stretched XAML layout before making their requests; Reset also restores that layout. Coverage text evaluates the actual video rectangle and client containment, so the larger outer client is not falsely described as a mismatched video size. A layout observation refreshes those measurements after padding has been arranged.

The new containment regression first failed before implementation. Release and Debug core contracts then passed 1/1, checking the measured extra-height rectangle, host movement, horizontal padding, retention of the final source row's screen coordinate, and rejection of clipped or invalid clients. Final widget builds passed in both configurations with zero warnings/errors. These results validate the implementation contracts and packaging; they do not establish pinning, visible four-edge coverage, or live retention.
