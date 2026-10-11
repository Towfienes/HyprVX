#!/usr/bin/env python3
"""Synthetic-image tests for tools/analyze_frame.py.

Expected answers come from the parameters used to draw each image (test-tooling section 4 content), never from
the analyzer itself.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ANALYZER = Path(__file__).resolve().parents[1] / "analyze_frame.py"
CANVAS = (1280, 800)
CANVAS_COLOR = (17, 17, 17)


# Palettes from test-tooling section 4 (1.3): background, then markers TL, TR, BL, BR.
PALETTES = {
    "main": ((32, 32, 32), ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 0, 255))),
    "popup": ((40, 40, 80), ((255, 255, 0), (0, 255, 255), (255, 128, 0), (128, 0, 255))),
    "sub": ((48, 40, 32), ((255, 0, 128), (128, 255, 0), (0, 128, 255), (0, 255, 128))),
}


def testclient(w, h, palette="main"):
    """hyprvx-testclient content: background, 50 px grid, 16x16 corner markers."""
    background, markers = PALETTES[palette]
    image = Image.new("RGB", (w, h), background)
    px = image.load()
    for y in range(h):
        for x in range(w):
            if x % 50 == 0 or y % 50 == 0:
                px[x, y] = (64, 64, 64)
    for (x0, y0), color in zip(((0, 0), (w - 16, 0), (0, h - 16), (w - 16, h - 16)), markers):
        for y in range(y0, y0 + 16):
            for x in range(x0, x0 + 16):
                px[x, y] = color
    return image


def frame(client, x, y, zoom=1.0, clip=None):
    """Composite the client at (x, y) scaled by zoom (bilinear, like GL_LINEAR), optionally clipped to a box."""
    canvas = Image.new("RGB", CANVAS, CANVAS_COLOR)
    if zoom != 1.0:
        client = client.resize((round(client.width * zoom), round(client.height * zoom)), Image.BILINEAR)
    layer = Image.new("RGB", CANVAS, CANVAS_COLOR)
    layer.paste(client, (x, y))
    if clip is None:
        return layer
    cx, cy, cw, ch = clip
    canvas.paste(layer.crop((cx, cy, cx + cw, cy + ch)), (cx, cy))
    return canvas


class AnalyzerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="hvx-analyzer-")
        self.dir = Path(self.tmp.name)
        self.client = testclient(800, 600)

    def tearDown(self):
        self.tmp.cleanup()

    def run_analyzer(self, *args):
        out = self.dir / "out.json"
        result = subprocess.run([sys.executable, "-I", str(ANALYZER), *args, "--out", str(out)],
                                capture_output=True, text=True)
        return result.returncode, json.loads(out.read_text()) if out.exists() else None

    def save(self, image, name, fmt="PNG"):
        path = self.dir / name
        image.save(path, fmt)
        return str(path)

    def test_identity_window_passes_with_exact_box(self):
        path = self.save(frame(self.client, 140, 100), "id.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "140,100,800,600", "--tolerance", "0")
        self.assertEqual(code, 0, out)
        self.assertEqual(out["verdict"], "PASS")
        self.assertEqual(out["bbox"], {"x": 140, "y": 100, "w": 800, "h": 600})
        self.assertEqual(out["markers"]["tr"], [939, 100])
        self.assertEqual(out["markers"]["br"], [939, 699])

    def test_quarter_zoom_bilinear_within_two_pixels(self):
        # 800x600 at z = 0.25 -> 200x150 at the drawn position.
        path = self.save(frame(self.client, 300, 200, zoom=0.25), "z025.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "300,200,200,150", "--tolerance", "2")
        self.assertEqual(code, 0, out)
        self.assertEqual(out["reasons"], [])

    def test_half_zoom_ppm_input(self):
        path = self.save(frame(self.client, 10, 20, zoom=0.5), "z05.ppm", "PPM")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "10,20,400,300", "--tolerance", "2")
        self.assertEqual(code, 0, out)

    def test_displacement_is_reported(self):
        path = self.save(frame(self.client, 150, 100), "shift.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "140,100,800,600", "--tolerance", "2")
        self.assertEqual(code, 1)
        self.assertIn("displacement_mismatch", out["reasons"])
        self.assertIn("bbox_mismatch", out["reasons"])
        self.assertNotIn("scale_mismatch", out["reasons"])

    def test_wrong_scale_is_reported(self):
        path = self.save(frame(self.client, 0, 0, zoom=0.5), "scale.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "0,0,200,150", "--tolerance", "2")
        self.assertEqual(code, 1)
        self.assertIn("scale_mismatch", out["reasons"])

    def test_rc1_style_clip_hides_markers(self):
        # Scaled window clipped to an untransformed sub-box (the RC1 symptom): BR marker disappears.
        path = self.save(frame(self.client, 0, 0, zoom=2.0 / 3.0, clip=(0, 0, 400, 300)), "rc1.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "0,0,533,400", "--tolerance", "2")
        self.assertEqual(code, 1)
        self.assertIn("marker_missing:br", out["reasons"])

    def test_allow_clip_accepts_markers_outside_the_visible_region(self):
        # Window partly off the right edge of the output: TR/BR fall outside the visible region.
        path = self.save(frame(self.client, 1000, 100), "offscreen.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "1000,100,800,600", "--tolerance", "2",
                                      "--allow-clip", "0,0,1280,800")
        self.assertEqual(code, 0, out)
        code, out = self.run_analyzer("bbox", path, "--expect-box", "1000,100,800,600", "--tolerance", "2")
        self.assertEqual(code, 1)
        self.assertIn("marker_missing:tr", out["reasons"])

    def test_diff_identical_and_one_pixel(self):
        base = frame(self.client, 140, 100)
        a = self.save(base, "a.png")
        b = self.save(base.copy(), "b.png")
        code, out = self.run_analyzer("diff", a, "--reference", b)
        self.assertEqual(code, 0, out)
        self.assertEqual(out["diff"]["pixels"], 0)
        changed = base.copy()
        changed.putpixel((5, 7), (18, 17, 17))
        c = self.save(changed, "c.png")
        code, out = self.run_analyzer("diff", c, "--reference", b)
        self.assertEqual(code, 1)
        self.assertEqual(out["diff"]["pixels"], 1)
        self.assertEqual(out["diff"]["region"], {"x": 5, "y": 7, "w": 1, "h": 1})
        self.assertEqual(out["reasons"], ["pixels_differ"])
        code, out = self.run_analyzer("diff", c, "--reference", b, "--max-pixels", "1")
        self.assertEqual(code, 0)

    def test_region_excludes_a_second_client_with_the_same_markers(self):
        # A second testclient (e.g. a layer surface) uses the same marker colors; --region isolates the window.
        image = frame(self.client, 140, 100)
        image.paste(testclient(240, 140), (980, 620))
        path = self.save(image, "two.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "140,100,800,600", "--tolerance", "0")
        self.assertEqual(code, 1)
        code, out = self.run_analyzer("bbox", path, "--expect-box", "140,100,800,600", "--tolerance", "0",
                                      "--region", "0,0,970,800")
        self.assertEqual(code, 0, out)
        self.assertEqual(out["bbox"], {"x": 140, "y": 100, "w": 800, "h": 600})

    def test_marker_sets_select_popup_and_subsurface(self):
        # Main window with a popup at window-local (300, 200) and a subsurface at (40, 380), both at z = 0.5.
        image = frame(self.client, 140, 100)
        image.paste(testclient(200, 120, "popup"), (440, 300))
        image.paste(testclient(160, 100, "sub"), (180, 480))
        path = self.save(image, "popup.png")
        code, out = self.run_analyzer("bbox", path, "--expect-box", "440,300,200,120", "--tolerance", "0",
                                      "--marker-set", "popup")
        self.assertEqual(code, 0, out)
        self.assertEqual(out["bbox"], {"x": 440, "y": 300, "w": 200, "h": 120})
        code, out = self.run_analyzer("bbox", path, "--expect-box", "180,480,160,100", "--tolerance", "0",
                                      "--marker-set", "sub")
        self.assertEqual(code, 0, out)
        # The main set still finds the main window and ignores the other palettes.
        code, out = self.run_analyzer("bbox", path, "--expect-box", "140,100,800,600", "--tolerance", "0")
        self.assertEqual(code, 0, out)
        # A popup drawn at the wrong place is a displacement, not a pass.
        code, out = self.run_analyzer("bbox", path, "--expect-box", "300,200,200,120", "--tolerance", "2",
                                      "--marker-set", "popup")
        self.assertEqual(code, 1)
        # Without a popup in the frame, every popup marker is missing.
        plain = self.save(frame(self.client, 140, 100), "plain.png")
        code, out = self.run_analyzer("bbox", plain, "--expect-box", "440,300,200,120", "--marker-set", "popup")
        self.assertEqual(code, 1)
        self.assertTrue({"marker_missing:tl", "marker_missing:tr", "marker_missing:bl",
                         "marker_missing:br"} <= set(out["reasons"]), out["reasons"])

    def test_blocked_on_unreadable_input_bad_arguments_or_size_mismatch(self):
        bad = self.dir / "bad.png"
        bad.write_bytes(b"not an image")
        code, out = self.run_analyzer("bbox", str(bad), "--expect-box", "0,0,1,1")
        self.assertEqual(code, 2)
        self.assertEqual(out["verdict"], "BLOCKED")
        good = self.save(frame(self.client, 0, 0), "good.png")
        code, out = self.run_analyzer("bbox", good, "--expect-box", "0,0,oops")
        self.assertEqual(code, 2)
        small = self.save(Image.new("RGB", (10, 10)), "small.png")
        code, out = self.run_analyzer("diff", good, "--reference", small)
        self.assertEqual(code, 2)
        code, _ = self.run_analyzer("bbox", good)
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
