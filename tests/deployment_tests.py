"""Validate synthetic MSIX manifests without elevation or package installation."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NS = "http://schemas.microsoft.com/appx/manifest/foundation/windows10"

@unittest.skipUnless(os.name == "nt", "Windows PowerShell validation only")
class DeploymentTests(unittest.TestCase):
    def validate(self, change=None):
        with tempfile.TemporaryDirectory(prefix="fuser-deployment-") as directory:
            root = Path(directory)
            (root / "tools").mkdir()
            (root / "native" / "widget").mkdir(parents=True)
            script = root / "tools" / "Deploy.ps1"
            shutil.copyfile(ROOT / "tools" / "Deploy.ps1", script)
            shutil.copyfile(ROOT / "native" / "widget" / "Package.appxmanifest", root / "native" / "widget" / "Package.appxmanifest")
            manifest = ET.parse(ROOT / "native" / "widget" / "Package.appxmanifest")
            identity = manifest.find("{" + NS + "}Identity")
            version = identity.attrib["Version"]
            identity.attrib["ProcessorArchitecture"] = "x64"
            if change:
                identity.attrib[change[0]] = change[1]
            folder = root / "AppPackages" / "FuserWidget" / ("FuserWidget_" + version + "_x64_Test")
            folder.mkdir(parents=True)
            with zipfile.ZipFile(folder / ("FuserWidget_" + version + "_x64.msix"), "w") as archive:
                archive.writestr("AppxManifest.xml", ET.tostring(manifest.getroot(), encoding="utf-8"))
            shell = Path(os.environ["SystemRoot"]) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe"
            return subprocess.run([str(shell), "-NoProfile", "-NonInteractive", "-File", str(script), "-ValidateOnly"], capture_output=True, text=True, timeout=20)

    def test_matching_manifest_validates_without_install(self):
        result = self.validate()
        self.assertEqual(result.returncode, 0, "Matching fixture failed validation")
        self.assertTrue(json.loads(result.stdout)["ValidatedOnly"])

    def test_version_architecture_and_publisher_mismatch_are_rejected(self):
        for change in (("Version", "0.0.0.1"), ("ProcessorArchitecture", "arm64"), ("Publisher", "CN=OtherFixture")):
            with self.subTest(field=change[0]):
                result = self.validate(change)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Package identity, version and architecture must match", result.stderr)

if __name__ == "__main__":
    unittest.main()
