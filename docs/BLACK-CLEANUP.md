# Clean black-background video

Version 0.2.1.1 adds **Clean black background**, **Exact black** and **Apply key settings** to the Game Bar widget. They change the existing GPU renderer and apply to a live stream without reconnecting. Saved pairing, stream dimensions and the fit above the taskbar are preserved.

## Live check

In the prepared 0.2.1.2 menu, cleanup controls are in **HUD**, and noise cutoff/softness values are under **Fine tuning**. The installed 0.2.1.1 still uses the original scrolling menu.

1. Open Software Fuser through Win+G and connect the existing black-background HUD. Hide the source fixture's controls with H, or use its fullscreen button, to inspect the artwork alone.
2. Click **Clean black background**. It selects Black, sets tolerance to 0.120 and softness to 0.080, enables **Recover bright HUD edges (Black)**, and saves/applies those settings immediately. The existing HUD opacity is retained.
3. Pin the widget, enable click-through and close Game Bar. Inspect white text and the moving white square over both light and dark receiving applications. Background speckles should disappear and text edges should blend without a dark outline. Check that the four corner markers and full-feed fit still appear.
4. If residue remains, raise tolerance in small increments, then click **Apply key settings**. The displayed numeric values and 0.005 slider steps allow fine adjustments. Increasing tolerance can also erase intentional near-black artwork.
5. If solid dark HUD panels need to retain their original colour, uncheck edge recovery and apply. **Exact black** switches to zero tolerance/softness and disables recovery immediately; it preserves every nonblack source pixel, including compression residue.
6. Disconnect, reopen and reconnect to check that the chosen settings persist. Record the appearance and receive/decode/Present counters separately; visual cleanup does not establish 240 distinct displayed frames per second.

Older saved profiles are restored as saved. The new preset is an explicit action; an existing exact-black profile is not silently migrated.

## Why the pixels remained

Exact-black mode removes only RGB values at black. Small luma/chroma deviations from video compression and filtering therefore survive with full opacity. Raising the cutoff removes low-level residue, but white glyphs antialiased over black contain grey edge pixels. Treating those samples as opaque leaves a dark fringe over a light receiving application.

The cleanup preset first applies the existing distance-based noise cutoff and smooth transition. For black only, optional edge recovery estimates source coverage as the largest decoded RGB component, divides RGB by that coverage, and multiplies the key alpha by coverage. Final HUD opacity and premultiplication then occur once. For a neutral white edge, premultiplied RGB equals alpha, so compositing over white cannot produce a dark fringe. At zero alpha all RGB components remain zero.

This assumes bright or saturated artwork drawn over black. A video feed has no original alpha, so an intentional grey panel cannot be distinguished from a partially covered white pixel using RGB alone. Recovery consequently makes such panels translucent; disabling it preserves their opaque colour above the noise cutoff. Green/magenta keying and disabled keying do not use recovery.

The change adds no frame readback, texture copy, spatial blur or extra sampling to the display path. Keying remains in the D3D11 pixel shader on the decoded NV12 texture inside the Game Bar UWP application. Live performance and final visual acceptance still require the installed-widget check.

## Verification

The hardware regression failed before the shader change because antialiased grey samples retained opaque alpha. Debug and Release core/GPU suites subsequently passed. The tests cover near-black luma and chroma residue in full and limited range, fractional edge coverage, composition over white, overall opacity, opaque white/coloured bodies, disabled recovery/keying, unaffected nonblack keys and a bilinearly rescaled black/white boundary. Existing exact-black, padded-source, lifetime, corner-fit and resize contracts remain included.

Both UWP/XAML/HLSL builds also completed with zero warnings and errors. The Release package was installed, and an independent Windows package query confirms version 0.2.1.1 with status OK. The GPU checks do not establish final live appearance or stream throughput. The controls explicitly configure the slider's [step intervals](https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.slider.stepfrequency) for fine adjustment.

Source screenshots, runtime logs, generated packages and local evidence stay excluded from Git. The screenshot used to identify the problem is not part of the repository.
