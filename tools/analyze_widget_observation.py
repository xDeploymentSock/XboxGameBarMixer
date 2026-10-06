"""Summarize passive widget observations without treating stale logs as live data."""
import argparse
import csv
from datetime import datetime
import json
import math
from pathlib import Path
import statistics


def number(row, field):
    value = float(row[field])
    if not math.isfinite(value):
        raise ValueError(f"Non-finite {field}")
    return value


def summarize(path, minimum_seconds):
    with path.open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    if len(rows) < 2:
        raise ValueError("At least two complete observations are required")
    times = [datetime.fromisoformat(row["observed_utc"]) for row in rows]
    if any(time.utcoffset() is None for time in times):
        raise ValueError("Observation timestamps must include a UTC offset")
    gaps = [(right - left).total_seconds() for left, right in zip(times, times[1:])]
    if any(gap <= 0 for gap in gaps):
        raise ValueError("Observation timestamps must increase")
    duration = (times[-1] - times[0]).total_seconds()
    profiles = sorted({tuple(row[field] for field in ("codec", "width", "height", "setup_fps")) for row in rows})
    fresh = [row for row in rows if 0 <= number(row, "rate_sample_age_seconds") <= 10
             and row["disconnect_after_sample"].lower() == "false"]
    rates = {}
    for field in ("received_units_per_second", "decoded_per_second", "present_calls_per_second"):
        values = [number(row, field) for row in fresh]
        rates[field] = {"min": min(values), "max": max(values), "sample_mean": statistics.mean(values)} if values else None
    resources = {}
    for field in ("working_set_bytes", "private_bytes", "handles"):
        values = [number(row, field) for row in rows]
        resources[field] = {"first": values[0], "last": values[-1], "min": min(values), "max": max(values), "last_minus_first": values[-1] - values[0]}
    errors = [number(row, "decode_errors") for row in rows]
    replacements = [number(row, "replaced_display_frames") for row in rows]
    cpu = [number(row, "process_cpu_seconds") for row in rows]
    counter_regressions = sum(right < left for values in (errors, replacements, cpu) for left, right in zip(values, values[1:]))
    return {
        "source_csv": str(path.resolve()),
        "observations": len(rows), "first_utc": times[0].isoformat(), "last_utc": times[-1].isoformat(),
        "observed_span_seconds": duration, "minimum_requested_seconds": minimum_seconds,
        "covers_requested_span": duration >= minimum_seconds - 1,
        "maximum_observation_gap_seconds": max(gaps),
        "pids": sorted({row["pid"] for row in rows}), "profiles": profiles,
        "fresh_rate_observations": len(fresh), "stale_or_disconnected_observations": len(rows) - len(fresh),
        "maximum_rate_sample_age_seconds": max(number(row, "rate_sample_age_seconds") for row in rows),
        "rates_from_fresh_samples": rates,
        "decode_errors_first": errors[0], "decode_errors_last": errors[-1], "decode_errors_max": max(errors),
        "replacements_first": replacements[0], "replacements_last": replacements[-1],
        "counter_regressions": counter_regressions, "resources": resources,
        "cpu_seconds_delta": cpu[-1] - cpu[0],
        "cpu_percent_of_one_logical_processor": (cpu[-1] - cpu[0]) * 100 / duration,
        "limits": "Periodic app rate samples are not a complete frame count or scanout measurement. Resource endpoints do not prove absence of leaks. Source and local-game workloads are uncontrolled. CSV span does not establish observer-process completion."
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--minimum-seconds", type=float, default=1800)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not math.isfinite(args.minimum_seconds) or args.minimum_seconds <= 0:
        parser.error("Minimum duration must be positive and finite")
    try:
        result = summarize(args.csv, args.minimum_seconds)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    output = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    print(output, end="")


if __name__ == "__main__":
    main()
