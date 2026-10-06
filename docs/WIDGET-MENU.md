# Widget settings menu

Version 0.2.1.2 replaces the long settings column with four views. Version 0.2.1.3 retains that menu and adds [Desktop application validation](DESKTOP-LAUNCH.md). The menu opens on Connect. Its status and stream-rate summary sit above navigation, with Connect, Disconnect/Cancel and Save profile below the content.

| View | Controls |
| --- | --- |
| Connect | Sunshine PC, pairing, application refresh and selection; stream resolution, requested FPS, codec and bitrate under **Stream options** |
| HUD | Background, immediate black-cleanup/exact-black presets, opacity and edge recovery; noise cutoff, softness and numeric values under **Fine tuning** |
| Layout | Whole-feed fit above the taskbar, taskbar reservation, monitor fit, Reset, typed overlay dimensions and the optional full-screen attempt |
| Details | Latest receiver statistics, click-through state, actual coverage and local preview/clear |

The footer remains available while scrolling a view. During a pending action, Disconnect becomes Cancel and profile/preset application is disabled. The idle Disconnect button is disabled. A compact header shows the same receive, decode and Present rates that the detailed sample records; these remain counts of pipeline activity, not proof of monitor scanout.

## Smaller windows

The menu card adapts to the actual widget client, with a maximum width of 420 view pixels and height of 720. Below a 340-view-pixel card width, a settings-page dropdown replaces the four navigation buttons. Very short windows can scroll the entire menu so the footer remains reachable. Selecting a different view resets its scroll position without changing settings, the video fit or the widget position.

Stream options and HUD fine tuning start collapsed and are expanded independently. The native XAML controls remain instantiated even when hidden, so loading, saving, connecting and applying a preset use the same fields and stored keys as before. Switching views or expanding options creates no host resize or transport action. The settings card still disappears in Game Bar's pinned-only mode.

## Validation and deployment

Debug and Release UWP/C++/XAML/HLSL builds completed with zero warnings/errors. A structural check preserved all 32 previously named controls, all 19 existing event handlers and every profile-field default/range. All 22 current handlers are declared and defined, names are unique, and the theme resources exist in the target SDK. Layout calculations were checked at 240x240, 320x480, 480x700 and 2560x1440 client sizes.

These checks establish compilation, wiring and intended bounds. Actual native appearance, keyboard navigation, high-contrast behavior, expanded-panel scrolling and live view switching still need the installed-widget check. No new rendering or performance result is attributed to this menu change.

The menu was initially prepared in 0.2.1.2 while 0.2.1.1 remained installed. It is now included in installed 0.2.1.3, with Windows package status OK. Live menu and black-cleanup acceptance remain pending.

After installation, open Software Fuser through Win+G. Visit all four views and expand Stream options and Fine tuning. Confirm that the current profile is restored, connection controls remain reachable, presets still apply to a live feed, and Reset/Apply video fit behave as before. Resize to a narrow window and confirm the dropdown preserves the selected view, then pin and close Game Bar to confirm only the overlay remains.
