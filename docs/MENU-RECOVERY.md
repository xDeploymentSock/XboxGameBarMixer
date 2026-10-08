# Settings menu recovery

## Request and scope

Repair the blank Software Fuser settings menu while Win+G is open, on main.
Retain the Connections / Adjustments / Advanced interface, saved settings,
protected pairing, Game Bar UWP presentation and explicit connection actions.
Keep the separate testing branch intact.

## Reproduction and cause

On installed 0.2.1.10, the user reported a blank widget with a title bar while
Game Bar was open. Closing the widget with its X and reopening from the widget
menu restored the settings card. The preceding private log recorded pinned-only
mode, then app suspension. Foreground opacity was 1, with no recorded XAML error.
This supports a retained-page lifecycle failure rather than a broken XAML layout;
it does not establish every possible cause of a blank widget.

The old suspension handler performed terminal view shutdown: it removed Game Bar
mode/visibility subscriptions, set the page's shutdown flag, and released the
widget/window references. No resume handler recreated those subscriptions or the
page. [Microsoft's UWP lifecycle guidance](https://learn.microsoft.com/en-us/windows/uwp/launch-resume/app-lifecycle)
requires restoring explicitly released state and dispatching resume UI work to
the UI thread. A card hidden before suspension could therefore remain hidden on return.

## Repair requirements

- Suspension stops the page's work while retaining the original Game Bar widget,
  CoreWindow and Frame association.
- Resumption dispatches to the captured UI dispatcher and replaces the stopped
  page with an idle settings page loaded from saved settings.
- Duplicate, superseded or queued-after-close resume callbacks cannot replace a
  running/newer view. Closing or replacing a host invalidates pending recovery.
- Shutdown is idempotent; later coroutine destruction and queued size/state/load
  callbacks cannot access a stopped page's XAML/resources.
- The settings menu is visible in foreground and hidden in pinned-only mode.
  Menu navigation never starts a connection or writes a profile.

The small resume-state helper and App recovery code were carried over selectively
from the reviewed testing candidate. Transport, renderer and other testing-branch
changes are excluded.

## Verification

Version 0.2.1.12 is installed with Windows status OK and an executable hash
matching the checked Release package. All 20 saved settings and both protected
pairing files are unchanged. Debug/Release UWP builds each report zero warnings
and errors; both package archives pass CRC, identity/version/x64 and executable
checks. Core contracts pass in Debug/Release and in an MSVC AddressSanitizer
Release build. The initial sanitizer test launch lacked its runtime DLL; rerunning
with the installed MSVC runtime directory and debug symbols passes. Sanitizers
cover the portable helper, not UWP XAML; MSVC UndefinedBehaviorSanitizer is
unavailable. The independent standards and specification reviews each have zero
unresolved findings after repairing the queued callback guard gap.

The user confirmed that the menu returns normally on the installed update.
Filtered runtime evidence shows foreground `menuVisible=true`, `menuOpacity=1`,
hiding in pinned-only mode, and returning in foreground. This collected log has
no `View resumed to idle settings.` entry during the reported check: ordinary
hide/reopen and fresh activation are observed, but a complete OS suspension/resume
cycle remains a separate acceptance step. No Narrator, high-contrast or enlarged
text acceptance is inferred.

Portable contracts cover one recovery per suspension, duplicate notification,
close/replacement invalidation and superseded suspensions. Debug/Release UWP
builds check the native integration. Those checks cannot establish native menu
visibility; the live procedure below remains necessary.

1. Open Software Fuser through Win+G and check the three settings sections.
2. Leave it unpinned and close Game Bar. Wait until the private runtime log records
   `App suspending.`; reopen Game Bar and Software Fuser.
3. Confirm the menu returns without closing/reopening the widget using its X.
   The log should record `View resumed to idle settings.` and foreground
   `menuVisible=true`, `menuOpacity=1` for the existing foreground preference.
4. Check saved settings and reconnect explicitly. Pin and close Game Bar; only
   the video should remain. Reopen and confirm the settings return.
5. Close/reopen the widget normally. Check that the new page remains usable.

A process terminated by Windows follows a fresh activation instead of Resuming.
Runtime logs and persistence snapshots remain private, outside Git.
