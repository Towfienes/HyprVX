import os
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.dont_write_bytecode = True

from tools.hyprland_event_probe import EventLineBuffer, find_socket, socket_candidates


class SocketDiscoveryTest(unittest.TestCase):
    def test_runtime_socket_is_first_candidate(self):
        paths = socket_candidates("instance-a", "/run/user/1000")
        self.assertEqual(paths[0], pathlib.Path("/run/user/1000/hypr/instance-a/.socket2.sock"))

    def test_tmp_socket_is_fallback_candidate(self):
        paths = socket_candidates("instance-a", None)
        self.assertEqual(paths, [pathlib.Path("/tmp/hypr/instance-a/.socket2.sock")])

    def test_duplicate_runtime_and_fallback_path_is_listed_once(self):
        paths = socket_candidates("instance-a", "/tmp")
        self.assertEqual(paths, [pathlib.Path("/tmp/hypr/instance-a/.socket2.sock")])

    def test_missing_signature_raises_clear_error(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(RuntimeError, "HYPRLAND_INSTANCE_SIGNATURE"):
                socket_candidates()

    def test_find_socket_returns_first_existing_candidate(self):
        with tempfile.TemporaryDirectory() as directory:
            first = pathlib.Path(directory) / "first.sock"
            second = pathlib.Path(directory) / "second.sock"
            second.touch()
            self.assertEqual(find_socket([first, second]), second)

    def test_find_socket_error_lists_candidates(self):
        candidates = [pathlib.Path("/one.sock"), pathlib.Path("/two.sock")]
        with self.assertRaises(RuntimeError) as raised:
            find_socket(candidates)
        self.assertIn("/one.sock", str(raised.exception))
        self.assertIn("/two.sock", str(raised.exception))


class EventLineBufferTest(unittest.TestCase):
    def test_fragmented_event_waits_for_newline(self):
        buffer = EventLineBuffer()
        self.assertEqual(buffer.feed(b"openwindow>>abc"), [])
        self.assertEqual(buffer.feed(b"\n"), ["openwindow>>abc"])

    def test_multiple_events_in_single_chunk(self):
        buffer = EventLineBuffer()
        self.assertEqual(buffer.feed(b"one>>a\ntwo>>b\n"), ["one>>a", "two>>b"])

    def test_empty_lines_are_preserved(self):
        self.assertEqual(EventLineBuffer().feed(b"\n\n"), ["", ""])

    def test_crlf_delimiter_is_normalized(self):
        self.assertEqual(EventLineBuffer().feed(b"activewindow>>x\r\n"), ["activewindow>>x"])

    def test_finish_returns_trailing_partial_event(self):
        buffer = EventLineBuffer()
        buffer.feed(b"closewindow>>abc")
        self.assertEqual(buffer.finish(), ["closewindow>>abc"])

    def test_finish_is_empty_after_complete_event(self):
        buffer = EventLineBuffer()
        buffer.feed(b"workspace>>2\n")
        self.assertEqual(buffer.finish(), [])

    def test_invalid_utf8_is_replaced(self):
        self.assertEqual(EventLineBuffer().feed(b"event>>\xff\n"), ["event>>\ufffd"])


if __name__ == "__main__":
    if len(sys.argv) > 1:
        case = sys.argv.pop()
        suite = unittest.defaultTestLoader.loadTestsFromName(case, module=sys.modules[__name__])
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        raise SystemExit(not result.wasSuccessful())
    unittest.main()
