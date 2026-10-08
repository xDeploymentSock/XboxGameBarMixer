# NoScreen integration assessment

Reviewed upstream revision: `c97e728e0ff0536fd00710ed2e7a530444618a70` on 2026-10-07.

## Result

NoScreen cannot be delivered as a drop-in, widget-only integration under the current project's UWP and public-distribution constraints. Proceed with the requested full review and release cleanup on `codex/noscreen-testing`, based on main commit `4ddad76b678d12cd466f0cac204aa17eaf8b5a05`. Main and the separate host-coverage investigation remain untouched.

## Source evidence

- The [kernel entry point](https://github.com/KANKOSHEV/NoScreen/blob/c97e728e0ff0536fd00710ed2e7a530444618a70/kernel/kernel/entry.cpp) creates a kernel device. Its unload callback is null; this is not a UWP library or an MSIX-only addition.
- The [implementation](https://github.com/KANKOSHEV/NoScreen/blob/c97e728e0ff0536fd00710ed2e7a530444618a70/kernel/kernel/library/utils.h) locates an undocumented function inside `win32kfull.sys` using byte signatures, including a Windows 11 24H2 fallback. Those signatures are not a supported Windows compatibility contract.
- The [desktop client](https://github.com/KANKOSHEV/NoScreen/blob/c97e728e0ff0536fd00710ed2e7a530444618a70/user/user/library/ioctl.h) opens the driver device and sends a window handle through `DeviceIoControl`. The sample targets another application's window; it does not implement Game Bar view ownership, lifecycle or device access.
- The inspected [complete tree](https://github.com/KANKOSHEV/NoScreen/tree/c97e728e0ff0536fd00710ed2e7a530444618a70) contains no license file or redistribution terms. No upstream code has been copied into Software Fuser.

Microsoft's [driver signing policy](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-) requires a separate driver-signing and distribution process. This assessment does not establish that a separately developed, licensed and signed driver could never work; it establishes that the referenced code is not a supported integration for this application's present delivery model. No driver was built, loaded or installed, and no Windows security setting was changed.

## Documented Windows alternatives

[SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity) is a desktop API requiring an owned top-level window. It is not a supported way to alter arbitrary Game Bar host windows from this UWP widget.

UWP's [ApplicationView.IsScreenCaptureEnabled](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.applicationview.isscreencaptureenabled) disables capture of an app view, but Microsoft documents a black captured window rather than removal of the overlay with the underlying scene preserved. Its behavior in a Game Bar-hosted composition surface has not been verified here. It is not silently substituted for NoScreen.

## Fallback scope

The [release-review requirements](RELEASE-REVIEW-SPEC.md) define the requested code review, cleanup and evidence. This branch is an investigation and cleanup checkpoint, not a claim that screenshot exclusion or public-package readiness has been achieved.
