#!/usr/bin/env python3
"""Safety checks for build-script dry runs, without fetching or building."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / sys.argv.pop(1)


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
                ["bash", str(SCRIPT), "--dry-run"],
                env=os.environ | {"HYPRVX_AGENT": "codex", "PATH": f"{root}:{os.defpath}"},
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("DRY RUN:", result.stdout)
            self.assertFalse(marker.exists(), result.stdout)


if __name__ == "__main__":
    unittest.main()
