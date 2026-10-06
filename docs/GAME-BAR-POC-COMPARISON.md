# Fixed startup sizing in the Game Bar proof of concept

The [bittzz proof of concept](https://github.com/bittzz/XBOXGameBarPoC/tree/1fb019cc7ad0d450cbdfc68e783c93123d65adac) keeps presentation inside a Game Bar UWP widget. It provides a specific startup-size configuration worth testing with Software Fuser's existing Sunshine renderer. It does not contain a full-screen API call, host title-bar removal, or arbitrary window-position command. Source inspection alone does not establish simultaneous four-edge coverage on the current host.

## What the source does

The relevant manifest is in the **packaging project**, not the UWP project's own manifest. Its [widget declaration](https://github.com/bittzz/XBOXGameBarPoC/blob/1fb019cc7ad0d450cbdfc68e783c93123d65adac/XBOXGameBarPoc_Package/Package.appxmanifest#L44-L72) declares initial width/height, minimum width/height, and maximum width/height all as 1920x1080. Both resize-support flags remain true. These constraints are present before Game Bar creates the widget; the source does not replace them at runtime.

The [activation code](https://github.com/bittzz/XBOXGameBarPoC/blob/1fb019cc7ad0d450cbdfc68e783c93123d65adac/XBOXGameBarPoc_UWP/App.xaml.cs#L71-L88) constructs XboxGameBarWidget with the current CoreWindow and XAML frame. The [render page](https://github.com/bittzz/XBOXGameBarPoC/blob/1fb019cc7ad0d450cbdfc68e783c93123d65adac/XBOXGameBarPoc_UWP/MainPage.xaml.cs#L17-L24) sizes its Win2D swap chain from that CoreWindow's actual bounds. Its XAML places the swap-chain panel in the page's root Grid. Transparent clearing and rectangle drawing happen inside that client surface.

The separately launched desktop helper writes drawing coordinates into shared memory. It does not create the presentation window or move/resize the UWP widget. Neither that helper nor Win2D is the source's window-sizing mechanism. Software Fuser can retain its existing Sunshine transport, hardware decoder, and premultiplied-alpha D3D11 composition renderer when testing the manifest configuration.

## Difference from Software Fuser's previous test

| Configuration | PoC | Software Fuser 0.2.0.4 |
| --- | --- | --- |
| Initial manifest client size | 1920x1080 | 480x700 |
| Manifest minimum/maximum | Both 1920x1080 | 240x240 / 7680x4320 |
| Full-size limits established | Before widget activation | During a later runtime fit |
| Resize-support flags during fitting | Both true | Both false |
| Centering request | None | CenterWindowAsync after resize attempts |

The previous implementation is preserved at commit e71a187. Its full-size runtime constraints and centering produced a monitor-sized surface shifted upward, with incomplete bottom coverage. That result is not an exact test of the PoC's startup configuration. Version 0.2.0.9 restores flexible limits during OnNavigatedTo and before explicit layout requests; merely changing its manifest would therefore not preserve the PoC's fixed startup constraints.

## Next discriminating experiment

Prepare a controlled in-host startup test with initial/minimum/maximum sizes all set to the target monitor's **view-pixel** dimensions, leave both resize flags true, and preserve those constraints through activation. For a 2560x1440 monitor at scale 1, that is 2560x1440; at scale 1.25 it is 2048x1152. Issue no initial resize, full-screen, or centering request. Use a fresh experimental widget identity/placement so cached Game Bar geometry does not confound the comparison, while retaining the normal widget's package identity and protected pairing.

Draw the existing outer white diagnostic border, then inspect settled widget/client/render bounds and visible top, bottom, left, and right edges together. Test pin/dismiss/reopen, transparency, and click-through. Keep a Reset route available. Do not claim coverage from the initial transient layout or from the configured dimensions alone. If startup sizing passes, separately determine whether explicit Fit and typed Apply can establish and preserve the same result; a fixed startup result alone does not satisfy dynamic sizing acceptance.

The experiment is isolated on `codex/fixed-startup-coverage`; see [FIXED-STARTUP-TEST.md](FIXED-STARTUP-TEST.md) for its configuration and live result. Correct activation with fixed 2560x1440 limits now obtains full client size, but Game Bar places that client at (0,-44) and leaves a 44-pixel bottom gap. Reopening repeats the offset. The user cannot pin it, and the recorded pin state remains false. Thus the distinct startup configuration has been tested and fails four-edge coverage on this host. The previously measured 0.2.0.9 settled bounds remain (0,46), 2558x1394 on a 2560x1440 display.

## Host evidence and limits of the conclusion

A [Microsoft maintainer response from 2021](https://github.com/microsoft/XboxGameBarSamples/issues/93#issuecomment-790198313) explains that full-screen widgets were unsupported and resize requests were restricted by available space around the Home Bar. That historical statement explains the public API limitation; it does not prove the PoC's manifest configuration cannot produce a useful result on another host or at startup. The [2022 follow-up](https://github.com/microsoft/XboxGameBarSamples/issues/111) remains a feature request; consult its current discussion for status.

Read-only inspection of the installed Game Bar 7.326.8061.0 metadata found host interfaces 8 through 10 beyond the pinned SDK's earlier interfaces. Their additions concern home-menu visibility, back navigation/secondary-page title, and recording. They expose no new geometry or frame-hiding command. The internal Game Bar ViewChromeBase has an IsChromeEnabled property, but the widget API does not expose the host's chrome instance. Private widget SetWindowBounds remains an incoming state-notification interface and was not invoked.

Microsoft's advanced sample at bd53d41dce2590727bd393cbafe9425a7e0cb3ae uses the same public size/centering requests; it is not a working full-monitor example. No supported programmatic host-placement solution was found in that comparison. The PoC's fixed manifest at creation establishes full size in the 0.2.0.10 experiment but has not solved placement or usable pin controls on this host. That failure does not authorize switching to a desktop renderer.

Reference clones, generated metadata, host resource dumps, runtime evidence, and packages stay in ignored build output. No separate desktop renderer or new rendering dependency was added.
