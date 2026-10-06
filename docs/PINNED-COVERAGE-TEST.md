# Pinned-only coverage probe

Version 0.2.0.11 on `codex/fixed-startup-coverage` adds one explicit test of full size after Game Bar is dismissed. The 0.2.0.9 baseline and the 0.2.0.10 startup-test packages remain preserved. Sunshine feed dimensions and rendering are unchanged. Rescaling is the user's fallback preference, not this experiment.

## Hypothesis and distinction

The 0.2.0.10 startup test obtains a 2560x1440 client at Y=-44 in foreground mode. The recovered 480x700 widget pins successfully, but normal Fit while Game Bar is foreground rejects 2560x1440. Those tests did not establish full-size minimum/maximum limits after dismissal in `PinnedOnly` mode.

Microsoft documents [PinnedOnly](https://learn.microsoft.com/en-us/xbox/game-bar/api/enum-xgb-displaymode) as the state in which Game Bar has been dismissed while pinned widgets remain visible. Its public [widget API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) exposes minimum/maximum size setters and a resize request. The new hypothesis is that enforcing full size in that state might avoid the foreground title-bar placement adjustment. This is an inference to test, not documented full-screen support.

The button arms a request only when the widget is pinned. The request remains queued during foreground mode. Upon visible pinned-only mode, it waits 300 ms for dismissal layout callbacks, confirms it is still current, then sets the maximum and minimum to the detected monitor size in view pixels, leaves both resize flags true, and requests that size once. It makes no centering or full-screen API call. It records the host result and settled widget/client/visible/video geometry after five seconds. Later state changes do not queue another request.

Reset or another explicit layout action supersedes the request. Unpinning, hiding, or display changes cancel a pending test. Reopening Game Bar cancels an in-flight test's remaining observations rather than applying another fit. Applied size limits remain until an explicit Fit, Apply, or Reset restores flexible limits. Reset remains the recovery route for an inaccessible title bar.

## Live procedure

Run on the original 2560x1440 display at scale 1, whose screen origin is (0,0). The diagnostic alignment flag requires matching widget, client, and visible origins; alternate monitor origins need separate interpretation of the logged coordinates.

1. Open **Software Fuser startup test**. Use **Reset widget position** to obtain reachable controls if needed.
2. Select **Black**, draw the test pattern, and pin the widget using its title bar. Do not connect a stream during this geometry probe.
3. Click **Test coverage after dismissing Game Bar**. Confirm the armed message, then dismiss Game Bar. Leave it dismissed for at least five seconds.
4. Inspect all four outer white edges, including top and taskbar, together. The cyan rectangle is inset intentionally. Confirm black transparency and click-through against a visible underlying application.
5. Reopen Game Bar and inspect retention. Reset if the title bar moves off-screen. Record host movement separately from the one explicit app request.

The local log must contain `Pinned constraints: applied`, the `pinned constraints` request result, and the five-second settled observation while pinned=true and mode=1. A returned true, full-sized buffers, or one aligned snapshot alone does not prove visible full coverage or retention across reopening.

## Verification

The request-gating contract was first run without the pinned-mode gate and failed on foreground execution. With the gate implemented, Debug and Release core contracts passed 1/1. They cover deferred execution, one-time consumption, superseding Reset, and cancellation on display change, hiding, and unpinning. They do not emulate Game Bar's placement rules.

Final Debug and Release widget builds passed with zero warnings/errors. Release manifest inspection confirmed the original package identity, `RemoteHudStartupTest`, pinning enabled, and all six startup size values fixed at 2560x1440. The preserved 0.2.0.9 package hash is unchanged. Deployment and an independent package query verify installed 0.2.0.11 with status OK and the original family identity. Versioned build logs and the installation receipt remain in ignored build output. Live coverage has not yet been verified.

Native Windows UI tooling failed to initialize with `helper_unknown_error: apply deny-read ACLs`. The test therefore requires the user's pin/button/dismiss actions; passive runtime-log inspection remains available. No terminal-based UI input workaround is used.
