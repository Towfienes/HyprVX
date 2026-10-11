#!/usr/bin/env python3
"""Sample screen points over a drawn surface and judge testclient pointer events (US3, V-16..V-18).

  pointer_points.py points --camera PX PY Z --surface X,Y,W,H --layout W,H [--n 20] [--margin 2] \
      --commands OUT.txt --meta OUT.json
  pointer_points.py check --meta POINTS.json --steps DRIVER.json --client NAME --surface-name main|popup|sub \
      [--tol 1] --out RESULT.json

`--surface` is the surface's logical (world) box: origin and size in unscaled surface units. Expected values are an
independent test oracle of contracts/transform-contract.md with M = (0, 0) (one output at the layout origin):
forward(W) = (W - P)·z and surfaceLocal(S) = P + S/z - origin. Moves are quantized to the 1/256 px that
hyprvx-vpointer sends, and the expectation uses the quantized point. Exit codes: 0 PASS, 1 FAIL, 2 BLOCKED.
"""

import argparse
import json
import sys
from pathlib import Path

SUBPIXEL = 256


def quantize(v):
    return round(v * SUBPIXEL) / SUBPIXEL


def cmd_points(args):
    px, py, z = args.camera
    sx, sy, sw, sh = (float(v) for v in args.surface.split(","))
    lw, lh = (float(v) for v in args.layout.split(","))
    # Drawn box on screen, then the part of it on the output, shrunk by a margin so no point sits on an edge.
    x0, y0 = (sx - px) * z, (sy - py) * z
    x1, y1 = x0 + sw * z, y0 + sh * z
    vx0, vy0 = max(x0, 0.0) + args.margin, max(y0, 0.0) + args.margin
    vx1, vy1 = min(x1, lw) - args.margin, min(y1, lh) - args.margin
    if vx1 - vx0 < 5 or vy1 - vy0 < 4:
        print("BLOCKED: drawn surface not visible enough to sample", file=sys.stderr)
        return 2
    cols, rows = 5, max(4, -(-args.n // 5))
    points, seen = [], set()
    for r in range(rows):
        for c in range(cols):
            s = (quantize(vx0 + (vx1 - vx0) * (c + 0.5) / cols + 0.37 * (r % 2)),
                 quantize(vy0 + (vy1 - vy0) * (r + 0.5) / rows + 0.21 * (c % 2)))
            key = (int(s[0]), int(s[1]))  # Hyprland skips a move whose floored position is unchanged
            if key in seen:
                continue
            seen.add(key)
            local = (px + s[0] / z - sx, py + s[1] / z - sy)
            points.append({"s": list(s), "expect_local": list(local)})
    if len(points) < args.n:
        print(f"BLOCKED: only {len(points)} distinct points", file=sys.stderr)
        return 2
    Path(args.commands).write_text("".join(f"move {p['s'][0]:.6f} {p['s'][1]:.6f}\n" for p in points))
    Path(args.meta).write_text(json.dumps({"camera": args.camera, "surface": args.surface, "drawn": [x0, y0, x1 - x0, y1 - y0],
                                           "points": points}, indent=1) + "\n")
    return 0


def cmd_check(args):
    meta = json.loads(Path(args.meta).read_text())
    steps = json.loads(Path(args.steps).read_text())["steps"]
    if any("blocked" in s for s in steps) or len(steps) != len(meta["points"]):
        result = {"verdict": "BLOCKED", "reasons": ["driver_incomplete"], "points": []}
        Path(args.out).write_text(json.dumps(result, indent=1) + "\n")
        return 2
    reasons, rows, worst = [], [], 0.0
    for i, (point, step) in enumerate(zip(meta["points"], steps)):
        events = [e for e in step["events"].get(args.client, []) if e["ev"] in ("enter", "motion")]
        row = {"i": i, "s": point["s"], "expect_local": point["expect_local"], "got": None, "error": None}
        if not events:
            reasons.append(f"no_event:{i}")
        else:
            last = events[-1]
            row["got"] = {"ev": last["ev"], "surface": last.get("surface"), "x": last["x"], "y": last["y"]}
            err = max(abs(last["x"] - point["expect_local"][0]), abs(last["y"] - point["expect_local"][1]))
            row["error"] = err
            worst = max(worst, err)
            if last.get("surface") != args.surface_name:
                reasons.append(f"wrong_surface:{i}:{last.get('surface')}")
            elif err > args.tol:
                reasons.append(f"local_mismatch:{i}")
        rows.append(row)
    result = {"verdict": "FAIL" if reasons else "PASS", "reasons": reasons, "max_error": worst,
              "tolerance": args.tol, "n": len(rows), "points": rows}
    Path(args.out).write_text(json.dumps(result, indent=1) + "\n")
    print(f"{result['verdict']} n={len(rows)} max_error={worst:.4f} {' '.join(reasons[:6])}".rstrip())
    return 0 if not reasons else 1


def main(argv):
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("points")
    p.add_argument("--camera", nargs=3, type=float, required=True, metavar=("PX", "PY", "Z"))
    p.add_argument("--surface", required=True)
    p.add_argument("--layout", default="1280,800")
    p.add_argument("--n", type=int, default=20)
    p.add_argument("--margin", type=float, default=2.0)
    p.add_argument("--commands", required=True)
    p.add_argument("--meta", required=True)
    c = sub.add_parser("check")
    c.add_argument("--meta", required=True)
    c.add_argument("--steps", required=True)
    c.add_argument("--client", required=True)
    c.add_argument("--surface-name", default="main")
    c.add_argument("--tol", type=float, default=1.0)
    c.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    return cmd_points(args) if args.mode == "points" else cmd_check(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
