# Main code review

This review follows the installed 0.2.1.7 color checkpoint. It covers stream startup/shutdown, decoded-frame ownership, renderer resources, settings validation, and local verification tools. The installed widget and source-control session were left running. The production fixes below are included in the subsequently installed 0.2.1.8 package; independent installation evidence is in [validation](VALIDATION.md).

## Correctness fixes

| Finding | Trigger and result | Verification |
| --- | --- | --- |
| Unsigned bitrate accepted by a signed transport field | A typed bitrate above 2,147,483,647 kbps passed validation, then converted to a negative Moonlight bitrate. Reject it before host control begins. | The boundary regression failed before the fix; zero, maximum signed, first overflowing, and maximum unsigned values are covered. |
| Benchmark error cleanup could hang | BT.2020 decoding fails before publishing a frame. Unwinding requests the render worker to stop, but the original condition-variable wait receives no notification. Use C++20 stop-aware waits so destruction can join the worker. | An owned H.264 fixture with rewritten matrix metadata reproduced a hang beyond five seconds. A subprocess regression requires the expected decoder error and exit code within five seconds. |

The unsupported-matrix fixture changes only VUI metadata on generated test content. It is rebuilt locally and remains outside Git. The regression runner terminates a hung benchmark instead of leaving a child process behind.

## Performance decisions

Two short Release baselines each submitted 1,200 owned 1440p frames at 240 requested inputs per second. CPU decoder-submission p95 was 0.2323 ms for H.264 and 0.2164 ms for HEVC. Draw CPU time including lock wait had p95 of 0.0208 and 0.0187 ms respectively. These runs had variable presentation pacing and were not controlled live latency tests or before/after optimization comparisons.

The subsequent [performance pass](RECEIVER-PERFORMANCE.md#current-optimization-pass-0218-candidate) measures and implements packet/receive-wrapper reuse and frame-reference transfer. At the time of this initial review, that work remained a profiling candidate. This review does not establish that changing it would materially reduce latency, so the decoder's validated frame leases and shared D3D11 context lock are retained. The existing renderer already caches plane views and completion queries, keeps only the latest decoded display frame, and wakes on frame notifications and presentation readiness.

The production render loop has no fixed four-millisecond polling wait. Its one-millisecond fallback is used only when GPU read slots are occupied. No global timer-resolution change is added. Local scalar declarations and compile-time type deduction do not themselves cause heap allocation.

## Verification and limits

Release and Debug results are recorded in [validation](VALIDATION.md). The checks exercise portable contracts, controlled pairing and Desktop selection, encrypted credential storage, GPU keying/color sampling, sequential/concurrent hardware decoding, and benchmark failure cleanup.

No package deployment or real-host performance run was performed for this review. Installed 0.2.1.7 shade stability, Game Bar display timing, and representative local-game load still need live verification. Host-frame coverage remains a separate testing-branch investigation.
