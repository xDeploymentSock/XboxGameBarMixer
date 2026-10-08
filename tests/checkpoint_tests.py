"""Local archive selection contracts use disposable synthetic repositories."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "create_checkpoint.py"
spec = importlib.util.spec_from_file_location("checkpoint", SCRIPT)
checkpoint = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checkpoint)

class CheckpointTests(unittest.TestCase):
    def test_default_selection_uses_tracked_source(self):
        with tempfile.TemporaryDirectory(prefix="fuser-checkpoint-") as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", directory], check=True)
            (root / "public.txt").write_text("public", encoding="utf-8")
            (root / "runtime.protected").write_bytes(b"private synthetic fixture")
            subprocess.run(["git", "-C", directory, "add", "public.txt"], check=True)
            self.assertEqual(checkpoint.source_files(root), [root / "public.txt"])
            with self.assertRaises(ValueError):
                checkpoint.source_files(root, ["runtime.protected"])
            with self.assertRaises(ValueError):
                checkpoint.source_files(root, ["../outside.txt"])
            (root / "nested").mkdir()
            with self.assertRaises(ValueError):
                checkpoint.source_files(root, ["nested/../public.txt"])
            with self.assertRaises(ValueError):
                checkpoint.source_files(root, [str(root / "public.txt")])

    def test_explicit_credential_inputs_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix="fuser-checkpoint-") as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", directory], check=True)
            for name in ("key.jks", ".env", ".env.development", "local.settings.json",
                         ".ssh/id_rsa", ".aws/credentials"):
                with self.subTest(name=name):
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"synthetic private fixture")
                    with self.assertRaises(ValueError):
                        checkpoint.source_files(root, [name])

if __name__ == "__main__":
    unittest.main()
