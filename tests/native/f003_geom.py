#!/usr/bin/env python3
"""Expected values for the Feature 003 native tests (test-tooling 1.5 section 9), re-implemented from
specs/003-trackpad-native-ux/contracts/gesture-model.md and the transform contract, independently of the C++ code.
The single test output sits at the layout origin, so M = (0, 0).

  f003_geom.py pan --camera PX PY Z --delta DX,DY
      panByScreen: P' = P - d/z; prints "PX' PY' Z".
  f003_geom.py scroll-delta --dx DX --dy DY --n N [--factor F]
      Screen pan of a finger scroll sequence of N frames of (DX, DY): -(N*DX, N*DY) * F; prints "x,y".
  f003_geom.py pinch --camera PX PY Z --anchor AX,AY --scale S
      pinchCamera with limits [0.1, 2.0]: z' = clamp(z*s); P' = (P + a/z) - a/z'; prints "PX' PY' Z'".
  f003_geom.py velocity --samples T:DX:DY[,T:DX:DY...] --end T
      ReleaseVelocity (window 100 ms, rest 50 ms, ring 32, merge equal times, clamp 6000 px/s); prints "vx,vy".
  f003_geom.py glide --velocity VX,VY [--strength S] [--min-speed M] [--stop-speed V] [--max-speed X]
      expectedGlide; prints "x,y".
  f003_geom.py selftest
      Cross-checks the functions above against vectors of tests/unit/test_gesture_model.cpp.
Exit codes: 0 ok, 1 selftest failure, 2 bad input.
"""

import argparse
import math
import sys

WINDOW_MS, MAX_REST_MS, MAX_SPEED, CAPACITY = 100, 50, 6000.0, 32
ZMIN, ZMAX = 0.1, 2.0


def pan(px, py, z, dx, dy):
    return px - dx / z, py - dy / z, z


def scroll_delta(dx, dy, n, factor=1.0):
    return -n * dx * factor, -n * dy * factor


def pinch(px, py, z, ax, ay, scale):
    if not math.isfinite(scale) or scale <= 0:
        return px, py, z
    z2 = min(max(z * scale, ZMIN), ZMAX)
    wx, wy = px + ax / z, py + ay / z
    return wx - ax / z2, wy - ay / z2, z2


def velocity(samples, end):
    ring = []
    for t, dx, dy in samples:
        if not (math.isfinite(dx) and math.isfinite(dy)):
            continue
        if ring and ring[-1][0] == t:
            ring[-1] = (t, ring[-1][1] + dx, ring[-1][2] + dy)
            continue
        ring.append((t, dx, dy))
        ring = ring[-CAPACITY:]
    if not ring:
        return 0.0, 0.0
    tn = ring[-1][0]
    if (end - tn) % 2**32 > MAX_REST_MS:
        return 0.0, 0.0
    window = [s for s in ring if (tn - s[0]) % 2**32 <= WINDOW_MS]
    if len(window) < 2:
        return 0.0, 0.0
    sx = sum(s[1] for s in window[1:])
    sy = sum(s[2] for s in window[1:])
    seconds = ((tn - window[0][0]) % 2**32) / 1000.0
    if seconds <= 0:
        return 0.0, 0.0
    vx, vy = sx / seconds, sy / seconds
    speed = math.hypot(vx, vy)
    if speed > MAX_SPEED:
        vx, vy = vx * MAX_SPEED / speed, vy * MAX_SPEED / speed
    return vx, vy


def damping(strength):
    if not math.isfinite(strength) or strength <= 0:
        return None
    return 2.0 + 10.0 * (1.0 - min(strength, 1.0))


def glide(vx, vy, strength=0.5, min_speed=100.0, stop_speed=1.0, max_speed=6000.0):
    lam = damping(strength)
    if lam is None or not (min_speed >= 0 and stop_speed > 0 and max_speed > stop_speed):
        return 0.0, 0.0
    speed = math.hypot(vx, vy)
    start = min(speed, max_speed)
    if speed <= 0 or start < min_speed:
        return 0.0, 0.0
    distance = (start - stop_speed) / lam
    if distance <= 0:
        return 0.0, 0.0
    return vx / speed * distance, vy / speed * distance


def selftest():
    failures = []

    def near(name, got, want, tol):
        if any(abs(g - w) > tol for g, w in zip(got, want)):
            failures.append(f"{name}: got {got}, want {want}")

    near("velocity known", velocity([(t, 10, -5) for t in (100, 110, 120, 130)], 140), (1000, -500), 1e-9)
    near("velocity rest 50", velocity([(t, 10, -5) for t in (100, 110, 120, 130)], 180), (1000, -500), 1e-9)
    near("velocity rest 51", velocity([(t, 10, -5) for t in (100, 110, 120, 130)], 181), (0, 0), 0)
    near("velocity window", velocity([(0, 1000, 0), (100, 10, 0), (110, 10, 0), (120, 10, 0)], 120), (1000, 0), 1e-9)
    near("velocity merge", velocity([(100, 10, 0), (100, 5, 0), (110, 10, 0)], 110), (1000, 0), 1e-9)
    near("velocity ring", velocity([(i, 0.1 * i, 0) for i in range(40)], 39), (2400, 0), 1e-6)
    near("velocity clamp", velocity([(100, 0, 0), (101, 60, 80)], 101), (3600, 4800), 1e-6)
    near("glide 1000", glide(1000, 0), (999 / 7, 0), 1e-9)
    near("glide 2d", glide(300, 400), (499 / 7 * 0.6, 499 / 7 * 0.8), 1e-9)
    near("glide below min", glide(50, 0), (0, 0), 0)
    near("glide clamp", glide(9000, 0), (5999 / 7, 0), 1e-9)
    near("glide off", glide(1000, 0, strength=0), (0, 0), 0)
    if damping(0.5) != 7 or damping(1) != 2 or damping(2) != 2 or damping(0.1) != 11 or damping(0) is not None:
        failures.append("damping mapping")
    zin = pinch(100, 50, 0.5, 640, 400, 50)
    zout = pinch(100, 50, 0.5, 640, 400, 0.01)
    if zin[2] != 2.0 or zout[2] != 0.1:
        failures.append(f"pinch clamp {zin} {zout}")
    for cam in (zin, zout):  # the anchor's world point (start camera) is drawn at the anchor again
        wx, wy = 100 + 640 / 0.5, 50 + 400 / 0.5
        near("pinch anchor", ((wx - cam[0]) * cam[2], (wy - cam[1]) * cam[2]), (640, 400), 1e-9)
    near("pinch invalid", pinch(100, 50, 0.5, 640, 400, 0), (100, 50, 0.5), 0)
    near("pan", pan(10, 20, 0.5, 100, -50), (-190, 120, 0.5), 1e-12)
    near("scroll delta", scroll_delta(0, 5, 10, 1.0), (0, -50), 0)
    for f in failures:
        print("FAIL", f)
    print("selftest", "PASS" if not failures else "FAIL")
    return 0 if not failures else 1


def pair(text):
    x, y = (float(v) for v in text.split(","))
    return x, y


def main(argv):
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("pan"); p.add_argument("--camera", nargs=3, type=float, required=True); p.add_argument("--delta", type=pair, required=True)
    p = sub.add_parser("scroll-delta"); p.add_argument("--dx", type=float, required=True); p.add_argument("--dy", type=float, required=True)
    p.add_argument("--n", type=int, required=True); p.add_argument("--factor", type=float, default=1.0)
    p = sub.add_parser("pinch"); p.add_argument("--camera", nargs=3, type=float, required=True); p.add_argument("--anchor", type=pair, required=True)
    p.add_argument("--scale", type=float, required=True)
    p = sub.add_parser("velocity"); p.add_argument("--samples", required=True); p.add_argument("--end", type=int, required=True)
    p = sub.add_parser("glide"); p.add_argument("--velocity", type=pair, required=True); p.add_argument("--strength", type=float, default=0.5)
    p.add_argument("--min-speed", type=float, default=100.0); p.add_argument("--stop-speed", type=float, default=1.0)
    p.add_argument("--max-speed", type=float, default=6000.0)
    sub.add_parser("selftest")
    try:
        a = parser.parse_args(argv)
    except SystemExit:
        return 2
    if a.cmd == "pan":
        print("%.12g %.12g %.12g" % pan(*a.camera, *a.delta))
    elif a.cmd == "scroll-delta":
        print("%.12g,%.12g" % scroll_delta(a.dx, a.dy, a.n, a.factor))
    elif a.cmd == "pinch":
        print("%.12g %.12g %.12g" % pinch(*a.camera, *a.anchor, a.scale))
    elif a.cmd == "velocity":
        samples = []
        for item in a.samples.split(","):
            t, dx, dy = item.split(":")
            samples.append((int(t), float(dx), float(dy)))
        print("%.12g,%.12g" % velocity(samples, a.end))
    elif a.cmd == "glide":
        print("%.12g,%.12g" % glide(*a.velocity, a.strength, a.min_speed, a.stop_speed, a.max_speed))
    else:
        return selftest()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
