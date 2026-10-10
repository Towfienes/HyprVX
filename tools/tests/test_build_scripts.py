#!/usr/bin/env python3
"""Safety checks for build-script dry runs, without fetching or building."""
import os
import errno
import importlib.util
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, call, patch

SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / sys.argv.pop(1)
DRY_RUN_ARGS = ["--variant", "v2"] if SCRIPT.name == "apply-patches.sh" else []
spec = importlib.util.spec_from_file_location("hyprvx_build", SCRIPT.parent.parent / "tools/hyprvx_build.py")
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


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
            variants = ("stock", "v2", "patched") if SCRIPT.name == "build-hyprland.sh" else (None,)
            for variant in variants:
                with self.subTest(variant=variant):
                    args = ["--variant", variant] if variant else DRY_RUN_ARGS
                    result = subprocess.run(
                        ["bash", str(SCRIPT), *args, "--dry-run"],
                        env=os.environ | {"HYPRVX_AGENT": "codex", "PATH": f"{root}:{os.defpath}"},
                        capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("DRY RUN:", result.stdout)
                    self.assertFalse(marker.exists(), result.stdout)
                    if variant:
                        if variant == "patched":
                            self.assertIn("-DHYPRVX_DEV=ON", result.stdout)
                        else:
                            self.assertNotIn("HYPRVX_DEV", result.stdout)
                        self.assertNotIn("-DCMAKE_CXX_FLAGS=", result.stdout)
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


class PsiTriggerTest(unittest.TestCase):
    def require_kernel_psi(self):
        if not Path("/proc/pressure/memory").exists():
            self.skipTest("NOT RUN: /proc/pressure/memory is absent")

    def test_real_kernel_accepts_exact_script_payload(self):
        self.require_kernel_psi()
        self.assertEqual(build.PSI_TRIGGER, b"full 100000 2000000\0")
        pressure = build.register_pressure_trigger()
        try:
            select.poll().register(pressure, select.POLLPRI)
        finally:
            os.close(pressure)
        with self.assertRaises(OSError) as error:
            os.fstat(pressure)
        self.assertEqual(error.exception.errno, errno.EBADF)

    def test_real_kernel_rejects_unterminated_payload(self):
        self.require_kernel_psi()
        pressure = os.open("/proc/pressure/memory", os.O_RDWR | os.O_NONBLOCK)
        try:
            try:
                os.write(pressure, b"full 100000 2000000")
            except OSError as error:
                self.assertEqual(error.errno, errno.EINVAL)
            else:
                self.skipTest("NOT RUN: this kernel accepts the unterminated PSI payload")
        finally:
            os.close(pressure)

    def test_registration_failure_stays_blocked_and_closes_fd(self):
        with patch.object(build.os, "open", return_value=202), \
                patch.object(build.os, "write", side_effect=OSError(errno.EPERM, "denied")), \
                patch.object(build.os, "close") as close:
            with self.assertRaisesRegex(SystemExit, "BLOCKED: cannot register the memory PSI trigger"):
                build.register_pressure_trigger()
            close.assert_called_once_with(202)

    def test_short_trigger_write_stays_blocked_and_closes_fd(self):
        with patch.object(build.os, "open", return_value=202), \
                patch.object(build.os, "write", return_value=1), \
                patch.object(build.os, "close") as close:
            with self.assertRaisesRegex(SystemExit, "BLOCKED:.*incomplete memory PSI trigger write"):
                build.register_pressure_trigger()
            close.assert_called_once_with(202)


class PsiWatchdogTest(unittest.TestCase):
    def setUp(self):
        self.process = Mock(pid=123)
        self.process.poll.return_value = None
        self.process.wait.side_effect = lambda: self.process.poll.return_value
        self.poller = Mock()
        self.avg10 = Mock()
        self.unit = "hyprvx-build-codex-test.scope"
        self.stop = Mock(side_effect=self.stop_scope)
        self.close = self.enterContext(patch.object(build.os, "close"))
        self.enterContext(patch.object(build.os, "pidfd_open", return_value=201))

    def stop_scope(self, command, *, check):
        self.assertEqual(command, ["systemctl", "--user", "stop", self.unit])
        self.assertTrue(check)
        self.process.poll.return_value = -15

    def watch(self):
        build.watch_build(self.process, 202, self.unit, poller=self.poller,
                          avg10_reader=self.avg10, stop_command=self.stop)

    def assert_stopped_and_closed(self):
        self.stop.assert_called_once_with(["systemctl", "--user", "stop", self.unit], check=True)
        self.process.wait.assert_called_once_with()
        self.assertEqual(self.close.call_args_list, [call(202), call(201)])

    def test_full_avg10_above_ten_blocks_and_stops_scope(self):
        self.poller.poll.return_value = [(202, select.POLLPRI)]
        self.avg10.return_value = 10.01
        with self.assertRaisesRegex(SystemExit, "BLOCKED: memory full avg10 exceeded 10"):
            self.watch()
        self.assert_stopped_and_closed()
        self.avg10.assert_called_once_with()
        self.assertEqual(self.poller.register.call_args_list,
                         [call(201, select.POLLIN), call(202, select.POLLPRI)])

    def test_full_avg10_at_or_below_ten_keeps_waiting(self):
        for value in (0.0, 9.99, 10.0):
            with self.subTest(avg10=value):
                self.process.poll.return_value = None
                self.poller.reset_mock()
                self.avg10.return_value = value
                self.avg10.reset_mock()
                events = iter([[(202, select.POLLPRI)], [(201, select.POLLIN)]])

                def next_events():
                    batch = next(events)
                    if batch == [(201, select.POLLIN)]:
                        self.process.poll.return_value = 0
                    return batch

                self.poller.poll.side_effect = next_events
                self.watch()
                self.assertEqual(self.poller.poll.call_count, 2)
                self.avg10.assert_called_once_with()
                self.stop.assert_not_called()

    def test_descriptor_failure_blocks_and_stops_scope(self):
        for flag in (select.POLLERR, select.POLLHUP, select.POLLNVAL):
            with self.subTest(event=flag):
                self.process.poll.return_value = None
                self.stop.reset_mock()
                self.close.reset_mock()
                self.process.wait.reset_mock()
                self.poller.poll.return_value = [(202, flag)]
                with self.assertRaisesRegex(SystemExit, "BLOCKED: memory PSI notification descriptor failed"):
                    self.watch()
                self.assert_stopped_and_closed()
                self.avg10.assert_not_called()

    def test_poller_failure_still_stops_scope(self):
        self.poller.poll.side_effect = OSError(errno.EIO, "poll failed")
        with self.assertRaises(OSError):
            self.watch()
        self.assert_stopped_and_closed()

    def test_stop_failure_still_closes_descriptors(self):
        self.poller.poll.return_value = [(202, select.POLLERR)]
        self.stop.side_effect = subprocess.CalledProcessError(1, "systemctl")
        with self.assertRaises(subprocess.CalledProcessError):
            self.watch()
        self.assertEqual(self.close.call_args_list, [call(202), call(201)])


class BuildMemoryTest(unittest.TestCase):
    def test_post_lock_memory_and_psi_boundaries(self):
        for available, avg10, reason in (
                (6815743, 0.0, "memory headroom or PSI changed"),
                (6815744, 10.01, "memory headroom or PSI changed"),
                (6815744, 10.0, "build directory is a symlink")):
            with self.subTest(MemAvailable_KiB=available, full_avg10=avg10), \
                    patch.object(build.Path, "read_text", return_value=f"MemAvailable: {available} kB\n"), \
                    patch.object(build.Path, "is_symlink", return_value=True), \
                    patch.object(build, "full_avg10", return_value=avg10), \
                    patch.object(build, "output") as output, \
                    patch.object(build.subprocess, "Popen") as start:
                with self.assertRaisesRegex(SystemExit, "BLOCKED: " + reason):
                    build.main(SCRIPT.parent.parent, "stock", False)
                output.assert_not_called()
                start.assert_not_called()


class GlazePinTest(unittest.TestCase):
    def fake_git(self, head=None, status=""):
        head = build.GLAZE_COMMIT if head is None else head
        return lambda *args: head if args[0] == "rev-parse" else status

    def test_pinned_clean_source_is_accepted(self):
        with tempfile.TemporaryDirectory(prefix="hvx-glaze-") as directory:
            (Path(directory) / ".git").mkdir()
            build.check_glaze_source(Path(directory), self.fake_git())

    def test_wrong_commit_dirty_tree_or_missing_source_blocks(self):
        with tempfile.TemporaryDirectory(prefix="hvx-glaze-") as directory:
            root = Path(directory)
            (root / ".git").mkdir()
            for git in (self.fake_git(head="0" * 40), self.fake_git(status=" M CMakeLists.txt")):
                with self.assertRaisesRegex(SystemExit, "BLOCKED: glaze source is not clean"):
                    build.check_glaze_source(root, git)
            with self.assertRaisesRegex(SystemExit, "BLOCKED: pinned glaze source is missing"):
                build.check_glaze_source(root / "absent", self.fake_git())
            link = root / "link"
            link.symlink_to(root)
            with self.assertRaisesRegex(SystemExit, "BLOCKED: pinned glaze source is missing"):
                build.check_glaze_source(link, self.fake_git())

    def test_dry_run_configures_offline_glaze_and_keeps_git_disabled(self):
        for variant in ("stock", "v2", "patched"):
            result = subprocess.run(["bash", str(SCRIPT), "--variant", variant, "--dry-run"],
                                    env=os.environ | {"HYPRVX_AGENT": "claude"}, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE", result.stdout)
            self.assertIn("-DFETCHCONTENT_FULLY_DISCONNECTED=ON", result.stdout)
            self.assertIn("-DFETCHCONTENT_SOURCE_DIR_GLAZE=", result.stdout)
            self.assertIn("glaze-" + build.GLAZE_TAG, result.stdout)


def load_tests(loader, tests, pattern):
    suite = loader.loadTestsFromTestCase(BuildScriptTest)
    if SCRIPT.name == "build-hyprland.sh":
        suite.addTests(loader.loadTestsFromTestCase(PsiTriggerTest))
        suite.addTests(loader.loadTestsFromTestCase(PsiWatchdogTest))
        suite.addTests(loader.loadTestsFromTestCase(BuildMemoryTest))
        suite.addTests(loader.loadTestsFromTestCase(GlazePinTest))
    return suite


if __name__ == "__main__":
    unittest.main()
