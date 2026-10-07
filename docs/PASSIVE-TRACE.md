# Passive DXGI API trace

This optional Windows diagnostic captures only DXGI Present start/stop events from explicitly selected existing processes. It creates no window or GPU resources, injects no code, and changes no display, timer, priority or application setting. Its unique ETW session lasts 1-30 seconds and stops during normal completion or exception cleanup. The installed Game Bar widget does not depend on this tool.

## Build and verify the collector

Use a Visual Studio 2022 developer environment with the Windows SDK:

```powershell
cmake -S . -B build/passive-trace -A x64 -DFUSER_BUILD_TRACE_PROBE=ON
cmake --build build/passive-trace --config Release --target fuser_dxgi_api_trace
ctest --test-dir build/passive-trace -C Release --output-on-failure
```

The test registers a separate owned ETW provider and emits exactly 100 start/stop pairs. It requires 200 events, positive start/stop counts, no event/buffer loss, no callback failure and no capacity overflow. It uses no DXGI swap chain or rendering load. Debug and Release passed locally, and inspection of their CSVs confirmed 100 complete ordered pairs with a common QPC frequency. This validates the ETW transport path, not the availability of DXGI instrumentation in the target app.

## Observe an existing widget

Keep the user's current workload and placement unchanged. Select the existing widget process, then collect a short private sample:

```powershell
$traceWidgetId = (Get-Process FuserWidget -ErrorAction Stop).Id
& build/passive-trace/Release/fuser_dxgi_api_trace.exe 10 build/widget-api-trace.csv $traceWidgetId
$traceExit = $LASTEXITCODE
```

Add an existing game's process ID as another argument when observing both applications. Up to eight distinct process IDs are accepted. Process handles remain open throughout the capture so a terminated process cannot be mistaken for a later process reusing its ID. The CSV records only process/thread IDs, event IDs, raw QPC timestamps and their frequency. Output files are overwritten at the caller's specified path; use a new filename to preserve comparisons. Raw CSVs and build artifacts are ignored by Git.

The collector scopes the provider to those process IDs and event IDs 42/43. It does not enable kernel, GPU scheduling, input, DWM or stack-walk providers. Permission errors are reported without changing account privileges or requesting elevation. Windows may require membership in Performance Log Users or an already elevated diagnostic shell.

## Interpret results

| Exit code | Meaning |
| --- | --- |
| 0 | Loss-free event collection with start/stop events for every target and all target process handles still alive. Synthetic mode also requires exactly 100 pairs. |
| 1 | Invalid numeric/process argument, Windows API failure or output failure. |
| 2 | Incorrect command shape. |
| 3 | Incomplete measurement: missing events, a target exited, event/buffer loss, callback failure or capture-capacity overflow. |

Pair starts and stops by process and thread, discard incomplete boundary pairs, and divide QPC differences by the recorded frequency for API durations. Collection success alone does not verify every pair or a Present call's HRESULT. The tool does not decode the event payload or distinguish failed/test Present calls, swap chains, unique source frames or display updates. **It does not measure displayed FPS, GPU completion or capture-to-screen latency.** A display-aware trace and controlled changing source-frame IDs are still needed for those conclusions.

The initial eight-second capture of an existing Moonlight process received zero DXGI events with zero ETW loss and returned code 3. The widget and game had already exited, and no producer was started for that check. That sample supplies no FPS or latency result and does not establish why DXGI events were absent. That initial check is now followed by active-target failures. An eight-second trace of the restarted game retained zero events, reported 1,917 lost events and returned code 3 while its process remained alive. A private control added sequential file logging alongside real-time delivery: its owned synthetic test still captured 200 events without loss, but an eight-second live game/widget capture retained zero DXGI events and reported 5,112 lost events. Both target process handles remained alive. Offline decoding of the ETL, including relaxed raw mode, found only two session metadata events and no DXGI records; the trace summary confirmed the lost-event count. File buffers were written, so this cannot be explained solely by this tool's real-time consumer omitting otherwise available DXGI records.

The registered provider manifest matches the selected GUID and event IDs, but these checks do not identify the event-loss cause or establish that every filter/Windows instrumentation path works. Increasing buffers or accepting a successful process exit would not make the missing sample valid. Actual target-app timing and displayed-frame measurement remain unavailable. Use the [in-process timing candidate](RECEIVER-PERFORMANCE.md#in-process-timing-candidate) to separate receiver phases in a later idle deployment while retaining this collection failure as an explicit limitation. The file-backed control and captures remain private diagnostics, not another installed component.

The provider/event definitions follow [PresentMon's versioned DXGI descriptors](https://github.com/GameTechDev/PresentMon/blob/v2.6.0/PresentData/ETW/Microsoft_Windows_DXGI.h). Microsoft documents [scope/event filters](https://learn.microsoft.com/en-us/windows/win32/api/evntprov/ns-evntprov-event_filter_descriptor), [provider enablement](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-enabletraceex2) and [real-time consumption](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-opentracew).
