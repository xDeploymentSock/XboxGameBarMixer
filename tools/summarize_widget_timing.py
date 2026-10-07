"""Summarize two private, same-connection widget timing checkpoints."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re

MAX_LOG_BYTES = 64 * 1024 * 1024
HEADER = re.compile(r"(?m)^(\d+) (HEVC|H\.264|AV1) (\d+)x(\d+) / setup FPS (\d+)")
PHASE_LABELS = (
    "Decode call", "Presentation ready wait", "Presentation timeout wait", "Draw call", "Present API call"
)
STAGES = (
    "idle", "preparing-decode", "submitting-decode", "publishing-frame", "waiting-frame", "resizing",
    "waiting-present", "acquiring-frame", "drawing", "presenting", "gpu-backoff", "releasing-frame",
    "stopped", "not-observed"
)
COUNTERS = {
    "received_units": r"Decode submission: units (\d+)",
    "accepted_present_samples": r"Callback-to-Present: samples (\d+)",
    "display_replacements": r"Replaced display frames (\d+)",
    "decoder_errors": r"Decode errors (\d+)",
    "skipped_frame_indexes": r"skipped before callback (\d+)",
    "gpu_slot_retries": r"GPU slot retries (\d+)",
    "present_retries": r"Present retries (\d+)",
    "presentation_wait_timeouts": r"presentation wait timeouts (\d+)",
    "transport_queue_overflows": r"transport queue overflows (\d+)"
}


@dataclass(frozen=True)
class Distribution:
    samples: int
    total_us: int
    maximum_us: int
    p95_bound_us: int
    p99_bound_us: int


@dataclass(frozen=True)
class Snapshot:
    timestamp_ms: int
    codec: str
    width: int
    height: int
    requested_fps: int
    connection_started_us: int
    elapsed_us: int
    counters: dict
    phases: dict
    worker_activity: dict


def match(block, pattern, label):
    value = re.search(pattern, block, re.MULTILINE)
    if value is None:
        raise ValueError("Latest rate block is missing " + label + ".")
    return value


def parse_latest(text):
    headers = list(HEADER.finditer(text.lstrip("\ufeff")))
    if not headers:
        raise ValueError("No widget rate block found.")
    # Never fall back to an older complete block after a newer incomplete one.
    text = text.lstrip("\ufeff")
    header = headers[-1]
    block = text[header.start():]
    if re.search(r"(?m)^\d+ (?:Disconnected\.|App suspending|Widget window closed|Connecting to )", block):
        raise ValueError("The latest rate block was followed by a lifecycle change.")
    session = match(block, r"^Session timing: started tick us (\d+) \| elapsed us (\d+)$", "session timing identity")
    token = int(session[1])
    if token <= 0:
        raise ValueError("The session start token is unavailable.")
    phases = {}
    for label in PHASE_LABELS:
        value = match(block, "^" + re.escape(label) +
                      r": samples (\d+) \| total us (\d+) \| maximum us (\d+) \| p95 bound us (\d+) \| p99 bound us (\d+)$", label)
        phases[label] = Distribution(*map(int, value.groups()))
    activity = match(block, r"^Worker activity: decode ([a-z-]+) \| age us (\d+) \| render ([a-z-]+) \| age us (\d+) \| transport queue overflows (\d+)$", "worker activity")
    if activity[1] not in STAGES or activity[3] not in STAGES:
        raise ValueError("Unknown worker stage.")
    return Snapshot(int(header[1]), header[2], int(header[3]), int(header[4]), int(header[5]), token, int(session[2]),
                    {key: int(match(block, pattern, key)[1]) for key, pattern in COUNTERS.items()}, phases,
                    {"decoder_stage": activity[1], "decoder_stage_age_us": int(activity[2]),
                     "render_stage": activity[3], "render_stage_age_us": int(activity[4])})


def summarize(begin, end, minimum_seconds=60):
    if begin.connection_started_us != end.connection_started_us:
        raise ValueError("The checkpoints belong to different connections.")
    if (begin.codec, begin.width, begin.height, begin.requested_fps) != (end.codec, end.width, end.height, end.requested_fps):
        raise ValueError("The stream profile changed.")
    seconds = (end.elapsed_us - begin.elapsed_us) / 1000000
    if seconds <= 0 or seconds < minimum_seconds:
        raise ValueError("The timing interval is too short or out of order.")
    added = {key: end.counters[key] - begin.counters[key] for key in COUNTERS}
    if min(added.values()) < 0:
        raise ValueError("A cumulative receiver counter decreased.")
    if not added["received_units"] or not added["accepted_present_samples"]:
        raise ValueError("The interval contains no active receive/presentation comparison.")
    phases = {}
    for label in PHASE_LABELS:
        first, last = begin.phases[label], end.phases[label]
        samples = last.samples - first.samples
        total_us = last.total_us - first.total_us
        if samples < 0 or total_us < 0 or last.maximum_us < first.maximum_us:
            raise ValueError("A cumulative timing counter decreased.")
        if not samples and total_us:
            raise ValueError("An approximate snapshot has inconsistent timing totals; retry the checkpoints.")
        phases[label] = {
            "added_completed_samples": samples,
            "added_total_us": total_us,
            "interval_average_ms": total_us / samples / 1000 if samples else None,
            # The log does not expose buckets. Do not invent interval percentiles.
            "ending_cumulative_max_ms": last.maximum_us / 1000,
            "ending_cumulative_p95_bound_ms": last.p95_bound_us / 1000,
            "ending_cumulative_p99_bound_ms": last.p99_bound_us / 1000,
        }
    return {
        "scope": "Receiver CPU calls and counters; excludes GPU execution, display updates and optical latency.",
        "duration_seconds": seconds,
        "duration_clock": "steady session elapsed time",
        "profile": {"codec": end.codec, "width": end.width, "height": end.height, "requested_fps": end.requested_fps},
        "connection_started_us": end.connection_started_us,
        "added_counters": added,
        "received_units_per_second": added["received_units"] / seconds,
        "accepted_present_samples_per_second": added["accepted_present_samples"] / seconds,
        # Live snapshots can straddle in-flight decode/presentation work.
        "receiver_balance_residual": added["received_units"] - added["accepted_present_samples"] - added["display_replacements"],
        "completed_call_timings": phases,
        "ending_worker_activity": end.worker_activity,
    }


def load_snapshot(path):
    with path.open("rb") as stream:
        data = stream.read(MAX_LOG_BYTES + 1)
    if len(data) > MAX_LOG_BYTES:
        raise ValueError("A log exceeds the 64 MiB diagnostic limit.")
    # splitlines normalizes Windows CRLF without altering the original file.
    return parse_latest("\n".join(data.decode("utf-8-sig").splitlines()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("begin_log", type=Path)
    parser.add_argument("end_log", type=Path)
    parser.add_argument("--output", type=Path, help="Optional private JSON output, preferably under build/.")
    args = parser.parse_args()
    try:
        result = summarize(load_snapshot(args.begin_log), load_snapshot(args.end_log))
        output = json.dumps(result, indent=2, allow_nan=False) + "\n"
        if args.output:
            args.output.write_text(output, encoding="utf-8")
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error) if isinstance(error, ValueError) and not isinstance(error, UnicodeError)
                     else "Unable to read UTF-8 checkpoints or write the private output.")
    print(output, end="")


if __name__ == "__main__":
    main()
