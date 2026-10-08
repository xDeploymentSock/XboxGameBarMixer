# Widget settings menu

Version 0.2.1.10 simplifies the menu to three sections. It opens on **Connections**, restores the existing saved profile, and stays idle until an explicit action. Connection status sits above navigation; **Connect**, **Disconnect / Cancel** and **Save settings** stay below the selected section.

| Section | Everyday controls |
| --- | --- |
| Connections | Sunshine PC, Refresh apps, Pair PC, application selection and a saved-stream summary |
| Adjustments | Clean near-black, Remove black only, Smooth / Crisp HUD scaling, opacity, Fit my monitor and usable-area video fit |
| Advanced | Collapsible stream settings, background-removal fine tuning and placement tools; receiver statistics, Game Bar state and test pattern |

**Clean near-black** applies the preferred hard noise cutoff immediately. **Remove black only** preserves every other decoded color but may leave compression noise. Both restore full opacity. Scaling and opacity edits apply with **Apply appearance**. Green/magenta backgrounds, cutoff, softness and optional fading controls are under **Advanced → Edit background removal**. See [black cleanup](BLACK-CLEANUP.md) and [HUD quality](HUD-QUALITY-LATENCY.md).

**Fit my monitor** remains an explicit host-size request. **Extra bottom space (px)** reserves physical pixels beyond the widget's usable client area; 0 avoids reserving the taskbar twice. **Apply video fit** saves and applies that setting without reconnecting. Reset, typed dimensions and the optional full-screen attempt are under **Advanced → Edit widget placement**. Game Bar can still constrain placement; this UI change does not resolve the separate host-frame coverage issue. See [video fitting](VIDEO-FIT.md).

## Baseline and saved settings

The fresh UI defaults match the selected baseline: 2560 × 1440, HEVC, 240 requested FPS, 100,000 kbps, Crisp HUD, black background, 0.12 hard cutoff, no softness or edge blending, full opacity, independent Game Bar opacity and 0 extra bottom pixels. Existing saved values override these defaults. The legacy black-profile migration remains limited to existing black profiles without a migration marker.

**Use baseline stream settings** explicitly saves the baseline stream values and applies Crisp HUD scaling. Other stream changes require **Save settings** and reconnecting. **Apply appearance** saves appearance separately; **Apply video fit** and **Apply dimensions** save their respective layout values. Opening the menu, switching sections or expanding controls does not save a new profile, start a connection or request a resize. Pairing storage and package identity are retained; no host addresses or credentials are included in the public defaults.

## Native layout

The UI uses the installed **UI/UX Pro Max** skill's minimal style and UWP theme guidance, adapted to a compact Game Bar settings panel. Native Segoe UI/system controls and theme brushes provide focus, hover, pressed and disabled states. The card is opaque for legibility over changing video; status is a polite accessibility live region. No new animations, external fonts or per-frame UI work are added.

The card fits the actual client, up to 420 × 660 view pixels. Below 380 view pixels wide, a dropdown replaces the navigation buttons. Each section scrolls independently; short windows can scroll the whole card to reach the footer. Changing sections resets section scrolling, without changing the video surface. The settings card disappears in Game Bar's pinned-only mode and returns in foreground. Version 0.2.1.12 adds idle-page recovery after suspension; see the [menu recovery check](MENU-RECOVERY.md).

The search's generic landing-page pattern and web typography were unsuitable for this widget and were not applied. Targeted progressive-disclosure queries also returned unrelated matches, so grouping follows the skill's general forms/navigation guidance and the user's requested three-section layout. Video stays inside the existing Game Bar UWP renderer.

## Live verification

1. Open Software Fuser through Win+G. Confirm Connections shows the existing host and saved 1440p / HEVC / 240 FPS / 100,000 kbps profile.
2. Visit Adjustments and Advanced. Expand each Advanced group; check that controls and footer actions remain reachable. Use Tab, Shift+Tab and Space to navigate and select sections.
3. Resize the widget narrower than 380 view pixels. Confirm the section dropdown preserves selection, text wraps, and the footer is reachable in a short window. Repeat with enlarged system text and Windows high contrast.
4. Connect to the selected application. Confirm near-black cleanup, retained colors and live rates behave as before. Apply appearance or fit only when intentionally changing those settings.
5. Pin and close Game Bar. Confirm only the video remains and click-through still reaches the local app. Reopen, disconnect and confirm saved settings are retained.

Compilation and structural checks do not establish native appearance, Narrator announcements or live Game Bar interaction. Record those separately from [performance measurements](RECEIVER-PERFORMANCE.md).
