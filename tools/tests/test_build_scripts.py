#!/usr/bin/env python3
"""Safety checks for build-script dry runs, without fetching or building."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / sys.argv.pop(1)
DRY_RUN_ARGS = ["--variant", "v2"] if SCRIPT.name == "apply-patches.sh" else []


class BuildScriptTest(unittest.TestCase):
    def test_requires_agent(self):
        result = subprocess.run(
            ["bash", str(SCRIPT), "--dry-run"],
            env=os.environ | {"HYPRVX_AGENT": ""},
            capture_output=True, text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("HYPRVX_AGENT", result.stderr)

    def test_dry_run_does_not_fetch_or_build(self):
        with tempfile.TemporaryDirectory(prefix="hvx-dry-run-") as directory:
            root = Path(directory)
            marker = root / "executed"
            for tool in ("git", "cmake", "ninja", "meson", "systemd-run", "flock", "ccache"):
                stub = root / tool
                stub.write_text(f"#!/bin/sh\nprintf called >> '{marker}'\nexit 97\n")
                stub.chmod(0o700)
            result = subprocess.run(
                ["bash", str(SCRIPT), *DRY_RUN_ARGS, "--dry-run"],
                env=os.environ | {"HYPRVX_AGENT": "codex", "PATH": f"{root}:{os.defpath}"},
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("DRY RUN:", result.stdout)
            self.assertFalse(marker.exists(), result.stdout)
            if SCRIPT.name == "apply-patches.sh":
                self.assertIn("patchset_id=bc05d46c42b0", result.stdout)
                self.assertNotIn("PROVENANCE.md", result.stdout)
                self.assertNotIn("SHA256SUMS", result.stdout)

    @unittest.skipUnless(SCRIPT.name == "build-hyprland.sh", "Hyprland resource guard only")
    def test_low_memory_stops_before_lock_or_build(self):
        with tempfile.TemporaryDirectory(prefix="hvx-memory-guard-") as directory:
            root = Path(directory)
            marker = root / "executed"
            awk = root / "awk"
            awk.write_text("#!/bin/sh\nprintf '6815743\\n'\n")
            awk.chmod(0o700)
            for tool in ("flock", "git", "cmake", "systemd-run"):
                stub = root / tool
                stub.write_text(f"#!/bin/sh\nprintf called >> '{marker}'\nexit 97\n")
                stub.chmod(0o700)
            result = subprocess.run(
                ["bash", str(SCRIPT), "--variant", "stock"],
                env=os.environ | {"HYPRVX_AGENT": "codex", "PATH": f"{root}:{os.defpath}"},
                capture_output=True, text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("BLOCKED: MemAvailable=6815743", result.stderr)
            self.assertFalse(marker.exists(), result.stdout)


if __name__ == "__main__":
    unittest.main()
