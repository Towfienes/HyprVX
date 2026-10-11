#!/usr/bin/env python3
"""Geometry and image helpers for the Feature 002 native tests (test-tooling 1.4 section 9).

  f002_geom.py expect --camera PX PY Z --box X,Y,W,H [--offset OX,OY] [--scale S] [--layout W,H]
      Expected drawn box of a window with logical box L and world offset O: forwardBox(L + O) with M = (0, 0) (the
      single test output at the layout origin), as an independent oracle of transform contract 1.2:
      forward(W) = (W - P)·z. Prints JSON {"logical": [x, y, w, h], "phys": "x,y,w,h", "visible": bool}; visible
      means the whole box lies on the output (only such windows are judged by bbox).
  f002_geom.py pad --box X,Y,W,H --by N
      The box grown by N px on every side (an analyzer --region around one window).
  f002_geom.py crop-equal A.png B.png X,Y,W,H
      Exit 0 when the region is pixel-identical in both images, 1 otherwise (prints the differing pixel count).
  f002_geom.py client CLIENTS.json CLASS
      "address x y w h" of the mapped client of that class in `hyprctl -j clients` output.
  f002_geom.py world-point --camera PX PY Z --screen SX,SY
      inverse(S) = P + S/z (world point under a screen point), as "x,y".
Exit codes: 0 ok/PASS, 1 FAIL, 2 bad input.
"""

import argparse
import json
import sys


def box(text):
    return tuple(float(v) for v in text.split(","))


def cmd_expect(a):
    px, py, z = a.camera
    x, y, w, h = box(a.box)
    ox, oy = box(a.offset) if a.offset else (0.0, 0.0)
    lw, lh = box(a.layout)
    sx, sy, sw, sh = (x + ox - px) * z, (y + oy - py) * z, w * z, h * z
    s = a.scale
    phys = "%d,%d,%d,%d" % (round(sx * s), round(sy * s), round(sw * s), round(sh * s))
    visible = sx >= 0 and sy >= 0 and sx + sw <= lw and sy + sh <= lh
    print(json.dumps({"logical": [sx, sy, sw, sh], "phys": phys, "visible": visible}))
    return 0


def cmd_pad(a):
    x, y, w, h = box(a.box)
    print("%d,%d,%d,%d" % (x - a.by, y - a.by, w + 2 * a.by, h + 2 * a.by))
    return 0


def cmd_crop_equal(a):
    from PIL import Image
    x, y, w, h = (int(v) for v in box(a.region))
    one = Image.open(a.a).convert("RGB").crop((x, y, x + w, y + h))
    two = Image.open(a.b).convert("RGB").crop((x, y, x + w, y + h))
    ra, rb = one.tobytes(), two.tobytes()
    diff = sum(1 for i in range(0, len(ra), 3) if ra[i:i + 3] != rb[i:i + 3])
    print(json.dumps({"region": [x, y, w, h], "differing_pixels": diff}))
    return 0 if diff == 0 else 1


def cmd_client(a):
    clients = json.load(open(a.clients))
    for c in clients:
        if c.get("class") == a.cls and c.get("mapped", True):
            print(c["address"], *c["at"], *c["size"])
            return 0
    print(f"no client of class {a.cls}", file=sys.stderr)
    return 2


def cmd_world_point(a):
    px, py, z = a.camera
    sx, sy = box(a.screen)
    print("%r,%r" % (px + sx / z, py + sy / z))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("expect")
    p.add_argument("--camera", nargs=3, type=float, required=True)
    p.add_argument("--box", required=True)
    p.add_argument("--offset")
    p.add_argument("--scale", type=float, default=1.0)
    p.add_argument("--layout", default="1280,800")
    p.set_defaults(fn=cmd_expect)
    p = sub.add_parser("pad")
    p.add_argument("--box", required=True)
    p.add_argument("--by", type=int, required=True)
    p.set_defaults(fn=cmd_pad)
    p = sub.add_parser("crop-equal")
    p.add_argument("a")
    p.add_argument("b")
    p.add_argument("region")
    p.set_defaults(fn=cmd_crop_equal)
    p = sub.add_parser("client")
    p.add_argument("clients")
    p.add_argument("cls")
    p.set_defaults(fn=cmd_client)
    p = sub.add_parser("world-point")
    p.add_argument("--camera", nargs=3, type=float, required=True)
    p.add_argument("--screen", required=True)
    p.set_defaults(fn=cmd_world_point)
    args = parser.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
