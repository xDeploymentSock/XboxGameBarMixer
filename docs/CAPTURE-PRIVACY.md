# Experimental capture privacy

In **Advanced > Capture privacy**, turn on **Block screen capture (experimental)**. The choice saves and applies immediately, independently of stream and appearance settings. New installations default off. Reopening the widget restores the saved choice.

The widget sets `ApplicationView.IsScreenCaptureEnabled` on its own UWP view. It does not use NoScreen, a driver, a desktop overlay, or capture hooks. Windows property acceptance is not proof that Game Bar's composed output is excluded. Captures may contain a black rectangle across the widget area rather than reveal the application underneath.

A save failure restores the previous switch state. An API failure retains the saved request and reports that it was not applied. Disconnecting does not change the setting. Turning it off enables capture on the same view again.

## Live verification

Use an owned moving test HUD over a recognizable local background during an idle window. Record each tool separately: Snipping Tool, Print Screen, Game Bar capture and OBS Display Capture.

1. With blocking off, capture with Game Bar open and with the widget pinned and Game Bar closed.
2. Turn blocking on and repeat both states. Record **visible**, **blacked out**, or **clean exclusion**. Property readback is not proof.
3. Turn blocking off and confirm the original capture behavior returns.
4. Reopen/restart with blocking on, then test actual Windows suspension/resume. Check saved choice, menu return, streaming and click-through.

Live tool results and suspension/resume acceptance are pending. Keep captures and runtime logs in ignored local evidence paths.

Microsoft documents black captured output for a disabled view: [IsScreenCaptureEnabled](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.applicationview.isscreencaptureenabled). [Game Bar widgets](https://learn.microsoft.com/en-us/xbox/game-bar/overview) render a separate UWP app into the host UI; this experiment must be tested in that host.
