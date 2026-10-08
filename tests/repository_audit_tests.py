"""Publication contracts use synthetic values only; no real private data."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

AUDIT = Path(__file__).resolve().parents[1] / "tools" / "audit_repository.py"
spec = importlib.util.spec_from_file_location("repository_audit", AUDIT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class RepositoryAuditTests(unittest.TestCase):
    def test_protected_credentials_and_crash_outputs_are_forbidden(self):
        for suffix in ("protected", "pending", "dat", "dmp", "mdmp"):
            with self.subTest(suffix=suffix):
                findings = audit.inspect("local/state." + suffix, b"opaque fixture", set())
                self.assertIn("private_or_generated_file", {f["type"] for f in findings})

    def test_diagnostics_never_echo_matched_secret(self):
        fake_secret = "ghp_" + "x" * 40
        findings = audit.inspect("example.txt", fake_secret.encode(), set())
        self.assertEqual(findings, [{"path": "example.txt", "type": "github_token", "line": 1}])
        self.assertNotIn(fake_secret, repr(findings))

    def test_staged_audit_reads_index_instead_of_modified_working_copy(self):
        with tempfile.TemporaryDirectory(prefix="fuser-audit-") as directory:
            subprocess.run(["git", "init", "-q", directory], check=True)
            path = Path(directory) / "example.txt"
            path.write_text("public fixture", encoding="utf-8")
            subprocess.run(["git", "-C", directory, "add", "example.txt"], check=True)
            path.write_text("ghp_" + "x" * 40, encoding="utf-8")
            staged = subprocess.run([sys.executable, str(AUDIT), "--staged"], cwd=directory, capture_output=True, text=True)
            working = subprocess.run([sys.executable, str(AUDIT)], cwd=directory, capture_output=True, text=True)
            self.assertEqual(staged.returncode, 0, staged.stdout + staged.stderr)
            self.assertEqual(working.returncode, 1, working.stdout + working.stderr)
            self.assertNotIn("ghp_" + "x" * 40, working.stdout + working.stderr)

    def test_forced_protected_blob_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="fuser-audit-") as directory:
            subprocess.run(["git", "init", "-q", directory], check=True)
            path = Path(directory) / "state.protected"
            path.write_bytes(b"opaque fixture")
            subprocess.run(["git", "-C", directory, "add", "-f", path.name], check=True)
            result = subprocess.run([sys.executable, str(AUDIT), "--staged"], cwd=directory, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("private_or_generated_file", result.stdout)

if __name__ == "__main__":
    unittest.main()
