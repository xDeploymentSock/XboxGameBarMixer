# Fit the whole feed above the taskbar

Version 0.2.1.0 on main adds a destination fit for the existing Game Bar UWP renderer. The whole incoming frame fills the usable drawing area, allowing unequal horizontal and vertical scaling. The source resolution, codec, pairing, requested FPS and transport remain unchanged.

## Controls

Open Software Fuser through Win+G. Leave **Keep full video above taskbar** checked, enter **Taskbar height (physical px)** and click **Apply video fit**. The initial height is an adjustable estimate of 48 view pixels converted to physical pixels at the current DPI; it is not a measured taskbar rectangle. Use 0 for a hidden taskbar. Uncheck the option and apply to restore drawing across the full widget client.

The fit is enabled by default. Applied settings persist. Moving or resizing the widget, opening it again, or changing DPI recomputes the drawing area without requesting another host resize. If the taskbar changes height, update its physical pixel reservation. **Fit my monitor** and **Apply dimensions** still request changes to the outer widget size; **Apply video fit** only changes its drawing area and works during a stream without reconnecting.

All four source corners should remain visible inside the fitted area. Manually enlarge or place the widget to increase the area available to the video. The host's top strip remains outside this drawing surface; full-monitor host coverage is being investigated separately on the testing branch.

## Geometry and rendering

The destination is the intersection of the actual XAML client with this monitor's bounds above the reserved bottom strip, expressed in widget-local view pixels using the Game Bar bounds. A client at (0,46) with size 2558x1394 on a 2560x1440 display and a 48-physical-pixel taskbar reservation produces a 2558x1346 destination. An oversized client starting at Y=-44 uses a 44-view-pixel local offset and a 2560x1392 destination, so the input's top pixels are rescaled into the visible area rather than disappearing offscreen.

XAML sizes and clips the video host to that destination. Its composition brush explicitly fills the destination, and the existing render-thread resize mechanism updates the swap chain. The NV12 shader continues to sample the complete decoded visible source rectangle; hardware allocation padding remains excluded. No desktop companion window, topmost HWND, layered window, CPU frame readback or source crop is added to the display path. GPU readback is confined to tests.

The existing [Game Bar bounds and event APIs](https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-widget) supply placement updates. The calculation uses the same display-coordinate convention as the baseline coverage reporting. Cross-monitor placement and live composition still require validation on the installed widget.

## Validation

The geometry regression first failed when using the entire client as the destination. Core and hardware GPU contracts then passed in both Debug and Release. They cover the measured host size, negative top origin, horizontal clipping, fractional DPI, a zero reservation, empty intersections and invalid inputs. GPU readback verifies all four markers from a full NV12 input at 2558x1346 and after a second resize to 1000x700, with transparent black between the markers. Existing colour, padded-source, resource-lifetime and resize checks also pass.

Both UWP/XAML/HLSL widget builds have zero warnings and errors. The Release 0.2.1.0 package is prepared locally. It has not replaced the other chat's installed coverage test. Live stream scaling, taskbar alignment and monitor changes are pending; no new FPS or full-monitor coverage claim is made.

Run `tools/Build.ps1 -Target GPU -Configuration Release` for the geometry/GPU suite, repeat with Debug, and build the widget in both configurations using the existing native dependencies. Runtime logs and generated packages remain ignored by Git.
