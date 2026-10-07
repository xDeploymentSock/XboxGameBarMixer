# Black removal and natural HUD colors

Version 0.2.1.6 separates black removal from opacity and edge blending. On **HUD**, click **Remove black only** to remove decoded black while preserving every other decoded RGB value with opaque alpha. The button applies immediately to an active stream and saves the profile. Both black presets reset HUD opacity to 100%, disable brightness-based edge blending and disable following Game Bar opacity for video. They retain the selected Smooth/Crisp HUD scaling mode.

| Preset | Noise cutoff | Edge softness | Retained pixels |
| --- | --- | --- | --- |
| Remove black only | 0.000 | 0.000 | Unchanged decoded RGB, alpha 1 |
| Remove near-black noise | 0.120 | 0.000 | Unchanged decoded RGB, alpha 1 above the cutoff |

Use **Remove black only** for the requested natural colors. The optional noise preset also removes very dark artwork and compression residue; it does not partially fade surviving pixels. Fine tuning remains available under its own expander. Increasing **Edge softness**, lowering **HUD opacity**, enabling **Blend bright edges (may fade colors)** or enabling **Follow Game Bar opacity for video** explicitly reintroduces transparency. Those controls apply through **Apply key settings** or **Save profile**.

Older black profiles are migrated once to exact black removal with full opacity. Pairing, application selection, stream options, scaling and widget placement are retained. After migration, subsequent explicit fine-tuning choices survive reopening. Green and magenta key settings retain their saved values. The video still renders through the existing D3D11 composition surface inside the Game Bar UWP widget.

## Cause and correction

The earlier cleanup preset inferred coverage from the largest RGB component, which was useful for bright white artwork antialiased over black. That also treated an opaque red with a maximum channel of 200/255 as partially transparent. Hardware readback reproduced decoded BGRA `(15, 18, 200, 255)` becoming `(15, 18, 200, 200)`. The game background then contributed to the displayed color. The old preset also kept any reduced HUD opacity instead of restoring full opacity.

There was a second, independent fade: the entire Composition video visual followed Game Bar's requested pinned opacity. The settings card still respects that preference, but the video visual now stays at opacity 1 unless the user explicitly enables following it. [Microsoft's transparency guidance](https://learn.microsoft.com/en-us/xbox/game-bar/guide/transparency) describes the requested value and applying it to the appropriate elements; it is not a second alpha channel in the video stream.

The new presets use the existing shader with a hard cutoff, no recovery and opacity 1. They add no GPU sampling, frame readback, blur, texture copies or presentation waits. Alpha is binary: removed background is transparent; retained decoded color is opaque. Video compression, 4:2:0 chroma sampling and source text antialiasing can still change fine edges before keying. These presets preserve the decoded colors rather than inventing saturation or reconstructing unavailable source alpha.

## Verification and live check

The new core and hardware GPU regression failed against the prior preset/visual-opacity behavior. With the correction, Debug and Release core/GPU checks pass. GPU readback compares red, yellow, blue, purple, grey and near-black against the same NV12 frame rendered with keying disabled, across Rec. 601/709 and full/limited range. Retained RGB is identical and alpha is 255; exact black removal preserves near-black while the optional cutoff removes it completely. Existing transparent-black, scaling, lifetime and shader contracts remain included. The core check verifies that a pinned opacity request of 0.85 leaves independent video at 1.0, while the opt-in mode follows 0.85.

For live verification, reopen Software Fuser through Win+G, choose **HUD → Remove black only**, reconnect to Desktop and pin the widget. Compare a stationary colored HUD against the source over both dark and light receiving backgrounds. Its retained colors should no longer pick up the game's background or fade when Game Bar closes. **Details** reports video surface opacity separately from the adjustable HUD opacity. If compression specks remain, try **Remove near-black noise**, then check that any intentionally dark artwork survives.

Local logs, source screenshots, packages and test receipts are excluded from Git. The provided screenshots are not published.
