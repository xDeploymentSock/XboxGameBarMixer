# Software Fuser

[![Build checks](https://github.com/xDeploymentSock/XboxGameBarMixer/actions/workflows/core-checks.yml/badge.svg?branch=main)](https://github.com/xDeploymentSock/XboxGameBarMixer/actions/workflows/core-checks.yml)

Show a remote PC's HUD over your local screen using a transparent Xbox Game Bar widget. Sunshine streams the video; Software Fuser removes the background color. Your mouse and keyboard stay on the local PC.

Supports H.264/HEVC hardware decoding, black/green/magenta removal, saved pairing and settings, and video fitting above the taskbar.

## Install

**Development preview: 0.2.1.12.** Build locally; a signed download is not available yet.

The receiving PC needs Windows 11 x64, Xbox Game Bar and a D3D11 hardware-decoding GPU. Install [Sunshine](https://github.com/LizardByte/Sunshine) on the source PC. Wired Ethernet is recommended.

To build, install Visual Studio 2022 with v143 C++, UWP C++ tools, Windows SDK 10.0.26100.0, CMake and Git. From this repository, run in Windows PowerShell:

```powershell
.\tools\Build.ps1 -Target Widget -Configuration Release
.\tools\Deploy.ps1 -Configuration Release
```

Deployment installs an unsigned development package, closes the running widget and requests Windows elevation. See the [build guide](docs/BUILD-LATER.md) for details.

## Use

1. Press **Win+G** and open **Software Fuser**.
2. In **Connections**, enter the source PC's address and click **Pair PC**. Enter the displayed PIN in Sunshine, then **Refresh apps**.
3. Select **Desktop** or your intended app and click **Connect**.
4. In **Adjustments**, choose **Clean near-black** and **Crisp HUD**, then **Apply appearance**. Use **Fit my monitor** and **Apply video fit** to fill the area above the taskbar.
5. Pin the widget, enable Game Bar click-through, then close Game Bar. Reopen it to change settings or disconnect.

Pairing and settings survive updates. [All controls](docs/WIDGET-MENU.md) · [Troubleshooting](docs/README.md#troubleshooting)

## Limits

- 1440p at 240 FPS is a requested stream profile. Actual displayed FPS and end-to-end latency depend on both PCs and remain unverified.
- Game Bar controls window placement; manual positioning may be needed. Main fits above the taskbar.
- SDR 8-bit NV12 video is supported. Compression and chroma subsampling can soften small colored text; HDR and 4:4:4 rendering are not implemented.

## Development

C++20, C++/WinRT, XAML and D3D11, with Moonlight transport and FFmpeg decoding. Video renders inside the Game Bar widget; audio playback and remote input forwarding are disabled.

[Contributing](CONTRIBUTING.md) · [Architecture](docs/ARCHITECTURE.md) · [Changelog](CHANGELOG.md) · [Release guide](docs/RELEASE.md) · [Documentation](docs/README.md)

Project code has no distribution license selected yet. Dependency licenses are retained in [third-party notices](THIRD-PARTY-NOTICES.md).
