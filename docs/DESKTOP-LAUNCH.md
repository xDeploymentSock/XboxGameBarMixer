# Desktop application selection

Version 0.2.1.3 verifies that a connection uses the selected Sunshine application before starting RTSP and presenting video.

Refresh prefers the entry named **Desktop** on its first successful enumeration. Subsequent refreshes on the same source retain the explicitly selected ID and name, including after list reordering. An active Steam session is labelled but does not replace the selection. A removed or renamed selection, absent Desktop, or ambiguous Desktop names require an explicit choice. Changing the source address requires Refresh before Connect.

Connect re-reads the authenticated application list and validates the selected ID/name pair. It refuses a stale mapping instead of starting whichever application now has that ID. If another Sunshine app is active, the error names it and explains how to end that session before reconnecting. The client does not automatically terminate another source session.

After launch/resume, the client checks authenticated `serverinfo` again. The selected ID must be active before RTSP negotiation, decoding or presentation begins. Sunshine's [resume implementation](https://github.com/LizardByte/Sunshine/blob/14ffa6fdaa53f7b51512be2b3d24f3939695403c/src/nvhttp.cpp#L858) resumes the currently running app; passing a Desktop `appid` does not switch an existing Steam session to Desktop. The post-request check catches an application change during startup. A later change after this check remains an external host-state change.

The widget reports the selected name and ID during connection. This provides local evidence of which application was requested without logging stream encryption keys. Private runtime logs remain ignored.

## Source configuration

An application named Desktop is still defined by the source's Sunshine configuration. The GameStream app list exposes its name and ID, not its execution or preparation commands, nor the foreground window. A Desktop entry configured to open Steam can pass identity validation while showing Steam. The client cannot establish the foreground application's identity from `serverinfo` alone.

Sunshine's [default Windows application definitions](https://github.com/LizardByte/Sunshine/blob/master/src_assets/windows/assets/apps.json) leave Desktop without a command and put `steam://open/bigpicture` on the separate Steam Big Picture entry. If Desktop still opens Steam, inspect Sunshine's Applications editor for Desktop's command, detached commands and preparation commands, then check global preparation commands and Steam startup behavior. Do not copy Steam's launch command into Desktop.

## Verification

Debug and Release pass all eleven loopback control/credential tests and the core contract suite. Five new control cases cover idle Desktop launch, matching Desktop resume, stale Desktop ID mapping, another active Steam app, and a switch to Steam during resume. Before the controller changes, four of those cases failed; all pass afterward. Core cases cover default Desktop selection, reordered lists, retained explicit Steam selection, renamed entries, no Desktop, ambiguous Desktop names and an empty list.

Debug and Release widget builds have zero warnings/errors. A four-second live HEVC/240-requested-FPS diagnostic launched Desktop from an idle host, decoded and presented video, and ended normally. Authenticated inspection afterward still identified Desktop. This establishes the requested host application identity and functional streaming; it does not prove which foreground window was visible or 240 distinct displayed frames per second.

The Release package is installed. An independent Windows package query confirms 0.2.1.3 with status OK. Installed-widget acceptance remains pending. To check it, open Software Fuser through Win+G, use Connect → Refresh apps, select Desktop and click Connect. If Steam is already active in Sunshine, end its session in Moonlight or on the source before trying Desktop. Confirm the source shows the intended desktop and that Refresh retains the selection.
