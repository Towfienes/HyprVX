#!/usr/bin/env python3
"""U0 spike (throwaway) checks used by tests/native/u0_spike.sh. Every subcommand prints one JSON object.
  absent IMAGE X,Y,W,H                 exit 0 when no marker pixel lies in the region
  wait-frame LOG SEQ TIMEOUT_S         exit 0 when frame_done for SEQ arrives in time ("latency_ms")
  configure-states LOG                 states of the last xdg_toplevel configure (9 = suspended, 4 = activated)
  wait-states LOG AFTER_LINE WANT_SUSPENDED TIMEOUT_S   exit 0 when a configure after line AFTER_LINE matches
  outputs LOG                          wl_surface output_enter / output_leave counts
  clients BEFORE AFTER                 exit 0 when workspace.id, at, size, floating, fullscreen match per address
  events LOG T0_NS T1_NS               movewindow and workspace events in [T0, T1]; exit 0 when no movewindow*
  local STEPS STEP CLIENT LX LY TOL    exit 0 when the step's last enter/motion of CLIENT is at (LX, LY) +-TOL
"""
import importlib.util
import json
import sys
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location("af", Path(__file__).resolve().parents[2] / "tools" / "analyze_frame.py")
af = importlib.util.module_from_spec(spec)
spec.loader.exec_module(af)


def lines(path):
    return [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]


def done(obj, ok):
    print(json.dumps(obj))
    sys.exit(0 if ok else 1)


def main(cmd, *a):
    if cmd == "absent":
        w, h, px = af.load_rgb(a[0])
        x, y, rw, rh = map(int, a[1].split(","))
        found = af.find_markers(w, h, px, {"x": x, "y": y, "w": rw, "h": rh})
        present = {k: v for k, v in found.items() if v}
        done({"region": a[1], "markers_found": present}, not present)
    if cmd == "wait-frame":
        log, seq, timeout = a[0], int(a[1]), float(a[2])
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            if any(e.get("seq") == seq and e.get("ev") == "frame_done" for e in lines(log)):
                done({"seq": seq, "delivered": True, "latency_ms": round((time.monotonic() - start) * 1000)}, True)
            time.sleep(0.01)
        done({"seq": seq, "delivered": False, "waited_ms": round(timeout * 1000)}, False)
    if cmd == "configure-states":
        conf = [e for e in lines(a[0]) if e.get("ev") == "configure" and "states" in e]
        done({"configures": len(conf), "last_states": conf[-1]["states"] if conf else None}, bool(conf))
    if cmd == "wait-states":
        log, after, want, timeout = a[0], int(a[1]), a[2] == "1", float(a[3])
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            conf = [e for e in lines(log)[after:] if e.get("ev") == "configure" and "states" in e]
            if conf and (9 in conf[-1]["states"]) == want:
                done({"suspended": want, "states": conf[-1]["states"], "latency_ms": round((time.monotonic() - start) * 1000)}, True)
            time.sleep(0.01)
        conf = [e for e in lines(log) if e.get("ev") == "configure" and "states" in e]
        done({"suspended_wanted": want, "last_states": conf[-1]["states"] if conf else None, "waited_ms": round(timeout * 1000)}, False)
    if cmd == "outputs":
        ev = [e["ev"] for e in lines(a[0])]
        done({"output_enter": ev.count("output_enter"), "output_leave": ev.count("output_leave")}, True)
    if cmd == "clients":
        keys = ("workspace", "at", "size", "floating", "fullscreen")
        pick = lambda c: {"workspace": c["workspace"]["id"], "at": c["at"], "size": c["size"], "floating": c["floating"], "fullscreen": c["fullscreen"]}
        before = {c["address"]: pick(c) for c in json.loads(Path(a[0]).read_text())}
        after = {c["address"]: pick(c) for c in json.loads(Path(a[1]).read_text())}
        diffs = {addr: {"before": before.get(addr), "after": after.get(addr)} for addr in sorted(set(before) | set(after)) if before.get(addr) != after.get(addr)}
        done({"fields": keys, "windows": len(before), "differences": diffs}, not diffs and bool(before))
    if cmd == "events":
        t0, t1 = int(a[1]), int(a[2])
        window = []
        for line in Path(a[0]).read_text().splitlines():
            stamp, _, event = line.partition(" ")
            if t0 <= int(stamp) <= t1:
                window.append(event)
        moves = [e for e in window if e.startswith("movewindow")]
        workspaces = [e for e in window if e.startswith(("workspace>>", "workspacev2>>", "focusedmon"))]
        done({"events": len(window), "movewindow": moves, "workspace": workspaces}, not moves)
    if cmd == "local":
        steps = json.loads(Path(a[0]).read_text())["steps"]
        step, client, lx, ly, tol = int(a[1]), a[2], float(a[3]), float(a[4]), float(a[5])
        ev = [e for e in steps[step]["events"][client] if e["ev"] in ("enter", "motion")]
        last = ev[-1] if ev else None
        ok = bool(last) and abs(last["x"] - lx) <= tol and abs(last["y"] - ly) <= tol and last.get("surface") == "main"
        done({"client": client, "expected": [lx, ly], "last": last}, ok)
    sys.exit(2)


if __name__ == "__main__":
    main(*sys.argv[1:])
