#!/usr/bin/env python3
"""HyprVX frame analyzer (contracts/test-tooling.md section 6).

  python3 -I tools/analyze_frame.py bbox IMAGE --expect-box X,Y,W,H --tolerance PX [--allow-clip X,Y,W,H] --out JSON
  python3 -I tools/analyze_frame.py diff IMAGE --reference IMG [--max-pixels N] --out JSON

Exit codes: 0 PASS, 1 FAIL, 2 BLOCKED (unreadable input or bad arguments).
"""

import argparse
import hashlib
import json
from pathlib import Path
import sys

TOOL_VERSION = "1.0"

# hyprvx-testclient corner markers (test-tooling section 4). No other client pixel uses these colors.
MARKERS = {
    "tl": (255, 0, 0),
    "tr": (0, 255, 0),
    "bl": (0, 0, 255),
    "br": (255, 0, 255),
}
# Per-channel distance accepted as a marker pixel. Filtered marker edges blend toward the RGB(32,32,32) background
# by at least half a channel range, so they stay outside this distance.
MARKER_CHANNEL_TOLERANCE = 24


class Blocked(Exception):
    pass


def load_rgb(path):
    """Returns (width, height, bytes in RGB order)."""
    data = Path(path).read_bytes()
    if data.startswith(b"P6"):
        return parse_ppm(data)
    try:
        from PIL import Image
    except ImportError as error:
        raise Blocked(f"PNG input needs Pillow ({error}); capture with grim -t ppm instead") from error
    try:
        with Image.open(path) as image:
            rgb = image.convert("RGB")
            return rgb.width, rgb.height, rgb.tobytes()
    except OSError as error:
        raise Blocked(f"unreadable image {path}: {error}") from error


def parse_ppm(data):
    fields = []
    pos = 2
    while len(fields) < 3:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos) + 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    width, height, maxval = fields
    if maxval != 255:
        raise Blocked(f"unsupported PPM maxval {maxval}")
    pixels = data[pos + 1:pos + 1 + width * height * 3]
    if len(pixels) != width * height * 3:
        raise Blocked("truncated PPM")
    return width, height, pixels


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def parse_box(text):
    try:
        x, y, w, h = (int(v) for v in text.split(","))
    except ValueError as error:
        raise Blocked(f"bad box {text!r}; expected X,Y,W,H") from error
    if w < 0 or h < 0:
        raise Blocked(f"bad box {text!r}; negative size")
    return {"x": x, "y": y, "w": w, "h": h}


def find_markers(width, height, pixels):
    """Bounding box (x0, y0, x1, y1) of each marker color, or None when absent."""
    found = {name: None for name in MARKERS}
    tol = MARKER_CHANNEL_TOLERANCE
    for y in range(height):
        row = y * width * 3
        for x in range(width):
            i = row + x * 3
            r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
            for name, (mr, mg, mb) in MARKERS.items():
                if abs(r - mr) <= tol and abs(g - mg) <= tol and abs(b - mb) <= tol:
                    box = found[name]
                    if box is None:
                        found[name] = [x, y, x, y]
                    else:
                        box[0] = min(box[0], x)
                        box[1] = min(box[1], y)
                        box[2] = max(box[2], x)
                        box[3] = max(box[3], y)
                    break
    return found


def inside(point, region):
    x, y = point
    return region["x"] <= x < region["x"] + region["w"] and region["y"] <= y < region["y"] + region["h"]


def analyze_bbox(image, expect, tolerance, allow_clip):
    width, height, pixels = load_rgb(image)
    found = find_markers(width, height, pixels)
    reasons = []

    # Expected marker corners derived from the expected box.
    corners = {
        "tl": (expect["x"], expect["y"]),
        "tr": (expect["x"] + expect["w"] - 1, expect["y"]),
        "bl": (expect["x"], expect["y"] + expect["h"] - 1),
        "br": (expect["x"] + expect["w"] - 1, expect["y"] + expect["h"] - 1),
    }
    for name in MARKERS:
        if found[name] is None and not (allow_clip and not inside(corners[name], allow_clip)):
            reasons.append(f"marker_missing:{name}")

    markers = {}
    if found["tl"]:
        markers["tl"] = [found["tl"][0], found["tl"][1]]
    if found["tr"]:
        markers["tr"] = [found["tr"][2], found["tr"][1]]
    if found["bl"]:
        markers["bl"] = [found["bl"][0], found["bl"][3]]
    if found["br"]:
        markers["br"] = [found["br"][2], found["br"][3]]

    bbox = None
    if found["tl"] and found["br"]:
        bbox = {"x": found["tl"][0], "y": found["tl"][1],
                "w": found["br"][2] - found["tl"][0] + 1, "h": found["br"][3] - found["tl"][1] + 1}
        displaced = abs(bbox["x"] - expect["x"]) > tolerance or abs(bbox["y"] - expect["y"]) > tolerance
        scaled = abs(bbox["w"] - expect["w"]) > tolerance or abs(bbox["h"] - expect["h"]) > tolerance
        if displaced:
            reasons.append("displacement_mismatch")
        if scaled:
            reasons.append("scale_mismatch")
        if displaced or scaled:
            reasons.append("bbox_mismatch")
        # The other two markers must agree with the box spanned by TL and BR.
        for name, (cx, cy) in (("tr", (bbox["x"] + bbox["w"] - 1, bbox["y"])),
                               ("bl", (bbox["x"], bbox["y"] + bbox["h"] - 1))):
            if name in markers and (abs(markers[name][0] - cx) > tolerance or abs(markers[name][1] - cy) > tolerance):
                if "bbox_mismatch" not in reasons:
                    reasons.append("bbox_mismatch")
    elif not allow_clip:
        if "bbox_mismatch" not in reasons:
            reasons.append("bbox_mismatch")

    return {"bbox": bbox, "markers": markers, "diff": None, "reasons": reasons}


def analyze_diff(image, reference, max_pixels):
    w1, h1, p1 = load_rgb(image)
    w2, h2, p2 = load_rgb(reference)
    if (w1, h1) != (w2, h2):
        raise Blocked(f"size differs from reference: {w1}x{h1} vs {w2}x{h2}")
    count = 0
    box = None
    for i in range(0, len(p1), 3):
        if p1[i:i + 3] != p2[i:i + 3]:
            count += 1
            x, y = (i // 3) % w1, (i // 3) // w1
            box = [x, y, x, y] if box is None else [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
    diff = {"pixels": count, "max_pixels": max_pixels, "reference_sha256": sha256(reference),
            "region": None if box is None else {"x": box[0], "y": box[1], "w": box[2] - box[0] + 1, "h": box[3] - box[1] + 1}}
    reasons = ["pixels_differ"] if count > max_pixels else []
    return {"bbox": None, "markers": {}, "diff": diff, "reasons": reasons}


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["bbox", "diff"])
    parser.add_argument("image")
    parser.add_argument("--expect-box")
    parser.add_argument("--tolerance", type=int, default=2)
    parser.add_argument("--allow-clip")
    parser.add_argument("--reference")
    parser.add_argument("--max-pixels", type=int, default=0)
    parser.add_argument("--out", required=True)
    try:
        args = parser.parse_args(argv)
    except SystemExit:
        return 2

    result = {"tool_version": TOOL_VERSION, "mode": args.mode, "image_sha256": None, "verdict": "BLOCKED",
              "bbox": None, "markers": {}, "diff": None, "reasons": []}
    try:
        result["image_sha256"] = sha256(args.image)
        if args.mode == "bbox":
            if not args.expect_box:
                raise Blocked("bbox needs --expect-box")
            allow = parse_box(args.allow_clip) if args.allow_clip else None
            result.update(analyze_bbox(args.image, parse_box(args.expect_box), args.tolerance, allow))
        else:
            if not args.reference:
                raise Blocked("diff needs --reference")
            result.update(analyze_diff(args.image, args.reference, args.max_pixels))
        result["verdict"] = "FAIL" if result["reasons"] else "PASS"
    except (Blocked, OSError) as error:
        result["verdict"] = "BLOCKED"
        result["reasons"] = [f"blocked:{error}"]

    Path(args.out).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(f"{result['verdict']} {' '.join(result['reasons'])}".rstrip())
    return {"PASS": 0, "FAIL": 1, "BLOCKED": 2}[result["verdict"]]


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
