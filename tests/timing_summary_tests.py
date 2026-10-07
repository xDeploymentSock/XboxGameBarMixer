"""Owned log fixtures for interval summary correctness and failure rejection."""

from contextlib import redirect_stdout
from dataclasses import replace
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("widget_timing_summary", ROOT / "tools/summarize_widget_timing.py")
summary = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = summary
spec.loader.exec_module(summary)


def block(timestamp, elapsed, token=12345, units=1000, presents=900, replaced=100, calls=1000, total_us=100000):
    lines = [f"{timestamp} HEVC 2560x1440 / setup FPS 240",
             f"Replaced display frames {replaced} | Decode errors 0",
             "Packet diagnostics: frame index 1000 | skipped before callback 0 | peak pending decode units 3",
             f"Decode submission: units {units} | total us 100000 | maximum us 1000",
             f"Callback-to-Present: samples {presents} | total us 100000 | maximum us 1000 | p95 bound us 250 | p99 bound us 500",
             "Render pressure: replaced pending frames 0 | GPU slot retries 0 | Present retries 0 | presentation wait timeouts 0",
             f"Session timing: started tick us {token} | elapsed us {elapsed}"]
    for label in summary.PHASE_LABELS:
        samples, total, maximum, percentile = (0, 0, 0, 0) if label == "Presentation timeout wait" else (calls, total_us, 1000, 1000)
        lines.append(f"{label}: samples {samples} | total us {total} | maximum us {maximum} | p95 bound us {percentile} | p99 bound us {percentile}")
    lines.append("Worker activity: decode idle | age us 10 | render waiting-present | age us 100 | transport queue overflows 0")
    return "\n".join(lines) + "\n"


class TimingSummaryTests(unittest.TestCase):
    def setUp(self):
        self.begin_text = block(1000, 5000000)
        self.end_text = block(76000, 80000000, units=19000, presents=15000, replaced=4000, calls=19000, total_us=3700000)
        self.begin = summary.parse_latest(self.begin_text)
        self.end = summary.parse_latest(self.end_text)

    def test_interval_totals_and_cumulative_bounds_stay_distinct(self):
        result = summary.summarize(self.begin, self.end)
        self.assertEqual(result["duration_seconds"], 75)
        self.assertEqual(result["received_units_per_second"], 240)
        self.assertEqual(result["accepted_present_samples_per_second"], 188)
        self.assertEqual(result["receiver_balance_residual"], 0)
        timing = result["completed_call_timings"]["Draw call"]
        self.assertEqual(timing["added_completed_samples"], 18000)
        self.assertEqual(timing["interval_average_ms"], 0.2)
        self.assertEqual(timing["ending_cumulative_p95_bound_ms"], 1)
        self.assertIsNone(result["completed_call_timings"]["Presentation timeout wait"]["interval_average_ms"])
        self.assertNotIn("interval_p95", json.dumps(result))

    def test_wall_clock_change_does_not_change_monotonic_duration(self):
        end = replace(self.end, timestamp_ms=500)
        self.assertEqual(summary.summarize(self.begin, end)["duration_seconds"], 75)

    def test_reconnect_is_rejected_even_with_larger_counters_and_same_profile(self):
        with self.assertRaisesRegex(ValueError, "different connections"):
            summary.summarize(self.begin, replace(self.end, connection_started_us=67890))

    def test_profile_changes_and_counter_resets_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "profile changed"):
            summary.summarize(self.begin, replace(self.end, width=1920))
        counters = dict(self.end.counters, decoder_errors=-1)
        with self.assertRaisesRegex(ValueError, "counter decreased"):
            summary.summarize(self.begin, replace(self.end, counters=counters))
        phases = dict(self.end.phases)
        phases["Draw call"] = replace(phases["Draw call"], samples=0)
        with self.assertRaisesRegex(ValueError, "timing counter decreased"):
            summary.summarize(self.begin, replace(self.end, phases=phases))

    def test_inactive_or_short_intervals_do_not_become_latency_results(self):
        with self.assertRaisesRegex(ValueError, "too short"):
            summary.summarize(self.begin, replace(self.end, elapsed_us=6000000))
        with self.assertRaisesRegex(ValueError, "no active"):
            summary.summarize(self.begin, replace(self.end, counters=dict(self.begin.counters)))

    def test_newer_incomplete_block_never_falls_back_to_older_data(self):
        with self.assertRaisesRegex(ValueError, "missing session"):
            summary.parse_latest(self.begin_text + "76000 HEVC 2560x1440 / setup FPS 240\n")

    def test_disconnect_or_new_connection_after_sample_is_rejected(self):
        for event in ("Disconnected.", "App suspending", "Widget window closed", "Connecting to owned application"):
            with self.subTest(event=event), self.assertRaisesRegex(ValueError, "lifecycle change"):
                summary.parse_latest(self.end_text + "77000 " + event + "\n")

    def test_unrelated_log_text_is_not_exported(self):
        parsed = summary.parse_latest("Owned unrelated free text\n" + self.end_text + "Owned unrelated trailing text\n")
        result = summary.summarize(self.begin, parsed)
        self.assertNotIn("Owned unrelated", json.dumps(result))

    def test_unavailable_token_and_unknown_stage_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "token is unavailable"):
            summary.parse_latest(self.end_text.replace("started tick us 12345", "started tick us 0"))
        with self.assertRaisesRegex(ValueError, "Unknown worker stage"):
            summary.parse_latest(self.end_text.replace("render waiting-present", "render unexpected"))


    def test_inconsistent_in_flight_phase_totals_are_rejected(self):
        phases = dict(self.end.phases)
        phases["Draw call"] = replace(self.begin.phases["Draw call"], total_us=self.begin.phases["Draw call"].total_us + 1)
        with self.assertRaisesRegex(ValueError, "inconsistent timing totals"):
            summary.summarize(self.begin, replace(self.end, phases=phases))

    def test_windows_utf8_checkpoints_and_private_cli_output(self):
        with tempfile.TemporaryDirectory() as directory:
            begin_path = Path(directory) / "begin.log"
            end_path = Path(directory) / "end.log"
            output_path = Path(directory) / "summary.json"
            begin_path.write_bytes(self.begin_text.replace("\n", "\r\n").encode("utf-8-sig"))
            end_path.write_bytes(self.end_text.replace("\n", "\r\n").encode("utf-8-sig"))
            captured = io.StringIO()
            with patch.object(sys, "argv", ["summary", str(begin_path), str(end_path), "--output", str(output_path)]), redirect_stdout(captured):
                summary.main()
            self.assertEqual(json.loads(output_path.read_text(encoding="utf-8")), json.loads(captured.getvalue()))
            self.assertEqual(json.loads(captured.getvalue())["duration_seconds"], 75)
            self.assertNotIn(directory, captured.getvalue())

    def test_bounded_reads_and_legacy_format_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "checkpoint.log"
            path.write_text(self.end_text, encoding="utf-8")
            with patch.object(summary, "MAX_LOG_BYTES", 16), self.assertRaisesRegex(ValueError, "exceeds"):
                summary.load_snapshot(path)
        legacy = self.end_text.replace("Session timing: started tick us 12345 | elapsed us 80000000\n", "")
        with self.assertRaisesRegex(ValueError, "missing session"):
            summary.parse_latest(legacy)


if __name__ == "__main__":
    unittest.main()
