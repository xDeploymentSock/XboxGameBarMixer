"""Check that a decoder failure joins the benchmark worker instead of hanging."""
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Pass the benchmark executable and owned unsupported-matrix fixture.")
    try:
        result = subprocess.run(
            [sys.argv[1], sys.argv[2], "h264", "240", "1"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except subprocess.TimeoutExpired:
        raise SystemExit("Benchmark hung while joining its render worker after decoder failure.")
    if result.returncode != 1 or "BT.2020 input is not supported" not in result.stderr:
        raise SystemExit("Benchmark must report the expected decoder rejection and exit with code 1.")
    print("Benchmark decoder failure reported; render worker joined normally.")


if __name__ == "__main__":
    main()
