# Supporting references

Research baseline: 2026-10-05. Refer to pinned source revisions when vendor code is integrated; GitHub branch links can change.

## Game Bar and UWP

- [Game Bar SDK](https://learn.microsoft.com/en-us/xbox/game-bar/): widget framework and documentation entry point.
- [Official samples](https://github.com/microsoft/XboxGameBarSamples): activation, SDK pins, and metadata marshaling. The foundation uses the inspected WidgetSample's version pins and manifest interface IDs.
- [Click-through](https://learn.microsoft.com/en-us/xbox/game-bar/guide/click-through): user-controlled mouse pass-through for pinned widgets.
- [Transparency](https://learn.microsoft.com/en-us/xbox/game-bar/guide/transparency): requested opacity and transparent content.
- [Widget API](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget): resize/centering constraints and widget state.
- [Known issues](https://learn.microsoft.com/en-us/xbox/game-bar/known-issues): fullscreen Vulkan/OpenGL and lifecycle limitations.
- [Desktop communication](https://learn.microsoft.com/en-us/xbox/game-bar/guide/communicating-apps): optional future packaged desktop companion.
- [Composition swap-chain interop](https://learn.microsoft.com/en-us/windows/win32/api/windows.ui.composition.interop/nf-windows-ui-composition-interop-icompositorinterop-createcompositionsurfaceforswapchain).
- [XAML visual attachment](https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.hosting.elementcompositionpreview.setelementchildvisual?view=winrt-26100).
- [HLSL build options](https://learn.microsoft.com/en-us/cpp/build/reference/hlsl-property-pages?view=msvc-170).
- [Windows 11 unsigned development packages](https://learn.microsoft.com/en-us/windows/msix/package/unsigned-package): per-package publisher OID and elevated `Add-AppxPackage -AllowUnsigned` used for local deployment.
- [Windows data protection](https://learn.microsoft.com/en-us/windows/uwp/security/data-protection): `DataProtectionProvider` with `LOCAL=user`, used for this app's private identity and host pins.
- [Original Game Bar PoC](https://github.com/bittzz/XBOXGameBarPoC): local rectangle drawing and shared-memory communication, not a video transport implementation. Its game-specific memory-reading code is not included in this framework.

## Streaming, decoding, and measurement

- [Sunshine](https://github.com/LizardByte/Sunshine) and [configuration](https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2configuration.html).
- [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt): desktop baseline and client reference.
- [Moonlight streaming core](https://github.com/moonlight-stream/moonlight-common-c): custom client integration and bundled ENet requirement.
- [Pinned decode-unit timing contract](https://github.com/moonlight-stream/moonlight-common-c/blob/f900dd4767759c7b9d0e93bcea666b55c69ea62f/src/Limelight.h#L149): receive/enqueue timestamps and absent/repeated host-latency interpretation.
- [Exact source Sunshine idle-frame logic](https://github.com/LizardByte/Sunshine/blob/14ffa6fdaa53f7b51512be2b3d24f3939695403c/src/video.cpp#L2051) and [packet host-latency logic](https://github.com/LizardByte/Sunshine/blob/14ffa6fdaa53f7b51512be2b3d24f3939695403c/src/stream.cpp#L1333): matches the user's reported source version; idle targets do not measure delivered FPS.
- [Exact Windows capture pacing](https://github.com/LizardByte/Sunshine/blob/14ffa6fdaa53f7b51512be2b3d24f3939695403c/src/platform/windows/display_base.cpp#L206): absolute pacing groups, late-target/snapshot recovery, and the restricted refresh-rate adjustment; source inspection does not measure actual capture throughput.
- [MTT virtual display example configuration](https://github.com/VirtualDrivers/Virtual-Display-Driver/blob/master/Virtual%20Display%20Driver%20%28HDR%29/vdd_settings.xml): supports configuring 1440p and global 240 Hz modes. The installed source-driver version/configuration is unverified, and an advertised mode is not evidence of actual changing-frame throughput.
- [Moonlight pairing](https://github.com/moonlight-stream/moonlight-qt/blob/master/app/backend/nvpairingmanager.cpp): host control and certificate exchange.
- [Windows D3D11VA renderer](https://github.com/moonlight-stream/moonlight-qt/blob/master/app/streaming/video/ffmpeg-renderers/d3d11va.cpp).
- [Direct3D 11 multithreading and DXGI](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro): immediate-context calls and DXGI Present must not run concurrently; used to correct the reproduced HEVC hang.
- [UWP FFmpeg decoder reference](https://github.com/TheElixZammuto/moonlight-xbox/blob/master/Streaming/FFmpegDecoder.cpp): useful implementation reference, not a drop-in Game Bar widget.
- [Moonlight performance statistics](https://github.com/moonlight-stream/moonlight-docs/wiki/Frequently-Asked-Questions): metric meaning and compositor/display latency limitations.
- [PresentMon definitions](https://github.com/GameTechDev/PresentMon/blob/main/README-CaptureApplication.md): present and display measurements.
- [PresentMon 2.6.0 console usage and CSV fields](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/README-ConsoleApplication.md) and [official release](https://github.com/GameTechDev/PresentMon/releases/tag/v2.6.0): portable CLI used for the live widget trace. Its valid Intel signature and release digest were verified before use; measurement limits are recorded in VALIDATION.md.
- [High-resolution Windows waitable timers](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw): receiver benchmark pacing; timer deadlines do not prove display timing.
- [FFmpeg overlay filter](https://www.ffmpeg.org/ffmpeg-filters.html#overlay-1): generates the owned moving-square compressed fixtures.
- [NVIDIA codec matrix](https://developer.nvidia.com/video-encode-decode-support-matrix) and [encoder performance](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/nvenc-application-note/index.html): hardware-specific capability and preset-dependent throughput. Check AMD/Intel primary documentation if those are the actual GPUs.

## Skills

- [C++ coding standards](https://skills.sh/affaan-m/ecc/cpp-coding-standards).
- [Systematic debugging](https://skills.sh/obra/superpowers/systematic-debugging).
- [Verification before completion](https://skills.sh/obra/superpowers/verification-before-completion).
- [Optional Windows UI testing](https://skills.sh/microsoft/win-dev-skills/winui-ui-testing): recommend if UI automation becomes a concrete need; no install is required for source authoring.
