#!/usr/bin/env bash
# T-FOCUS (US3.2-3.3, V-17, T058). Camera z = 0.5, P = (0, 0): window A (800x600 at logical 140,100) is drawn at
# (70,50) 400x300; window B (240x160 at logical 960,560, app-id hyprvx-testclient-b) at (480,280) 120x80.
#  1. Motion over empty canvas reaches no window: points inside A's logical box but outside its drawn box get no
#     enter/motion for A (the untransformed hit-test would deliver them).
#  2. A click focuses the window drawn under S: click on drawn B => active window B; click on drawn A at a point that
#     is logically empty canvas => A; click on drawn B again => B. Each click's motion local = surfaceLocal(S) +-1.
# Setup validity first: at identity, a move to A's centre reaches A at (400,300) +-1, else BLOCKED.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_POINTER_TOLERANCE:-1}"

hvx_init_run patched t_focus
hvx_copy_build_info patched
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600
hvx_vpointer_start vp

steps() { # NAME CLIENT... ; commands are read from stdin
    cat > "$HVX_EVID/$1.cmds"
    local name=$1
    shift
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" "$@"
}

# Setup validity (identity, stock path).
printf 'move 540 400\n' | steps setup client
python3 -I - "$HVX_EVID/setup.steps.json" "$TOL" << 'PY' || hvx_die_blocked "identity setup: vpointer move did not reach A at (400,300)"
import json, sys
ev = [e for e in json.load(open(sys.argv[1]))["steps"][0]["events"]["client"] if e["ev"] in ("enter", "motion")]
tol = float(sys.argv[2])
sys.exit(0 if ev and abs(ev[-1]["x"] - 400) <= tol and abs(ev[-1]["y"] - 300) <= tol else 1)
PY

hvx_nested dev camera 0 0 0.5 > "$HVX_EVID/camera.reply" || hvx_die_blocked "dev camera rejected"
hvx_capture camera client
result=PASS

# 1. Empty canvas: first a point on drawn A (sanity), then five points of A's logical box that A is not drawn at.
printf 'move 270 200\nmove 600 500\nmove 700 300\nmove 850 600\nmove 500 650\nmove 900 150\n' | steps empty client
python3 -I - "$HVX_EVID/empty.steps.json" "$HVX_EVID/empty.result.json" << 'PY' || result=FAIL
import json, sys
steps = json.load(open(sys.argv[1]))["steps"]
reasons = []
first = [e for e in steps[0]["events"]["client"] if e["ev"] in ("enter", "motion")]
if not first:
    reasons.append("no_event_on_drawn_window")
for i, step in enumerate(steps[1:], 1):
    hits = [e for e in step["events"]["client"] if e["ev"] in ("enter", "motion")]
    if hits:
        reasons.append(f"motion_on_empty_canvas:{i}:{step['cmd']}")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons}, open(sys.argv[2], "w"), indent=1)
print("empty canvas:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY

# 2. Click focus with a second window.
hvx_client_start client_b 240x160 --app-id hyprvx-testclient-b
hvx_capture camera_ab client
click_check() { # NAME X Y EXPECT_TITLE CLIENT LOCALX LOCALY
    local name=$1 x=$2 y=$3 title=$4 who=$5 lx=$6 ly=$7
    printf 'move %s %s\nclick\n' "$x" "$y" | steps "$name" client client_b
    hvx_hyprctl patched -j activewindow > "$HVX_EVID/$name.activewindow.json" || hvx_die_blocked "$name: activewindow query failed"
    python3 -I - "$HVX_EVID/$name.steps.json" "$HVX_EVID/$name.activewindow.json" "$title" "$who" "$lx" "$ly" "$TOL" \
        "$HVX_EVID/$name.result.json" << 'PY'
import json, sys
steps_path, active_path, title, who, lx, ly, tol, out = sys.argv[1:]
lx, ly, tol = float(lx), float(ly), float(tol)
steps = json.load(open(steps_path))["steps"]
active = json.load(open(active_path))
reasons = []
if active.get("title") != title:
    reasons.append(f"active_window:{active.get('title')!r}")
motion = [e for e in steps[0]["events"][who] if e["ev"] in ("enter", "motion")]
if not motion or abs(motion[-1]["x"] - lx) > tol or abs(motion[-1]["y"] - ly) > tol or motion[-1].get("surface") != "main":
    reasons.append(f"local_mismatch:{motion[-1] if motion else None}")
buttons = [e for e in steps[1]["events"][who] if e["ev"] == "button"]
if [b["state"] for b in buttons] != ["pressed", "released"]:
    reasons.append(f"buttons:{[b['state'] for b in buttons]}")
other = "client" if who == "client_b" else "client_b"
if [e for e in steps[1]["events"][other] if e["ev"] == "button"]:
    reasons.append("button_reached_other_window")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons, "active": active.get("title")}, open(out, "w"), indent=1)
print(f"{out.rsplit('/', 1)[-1]}:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY
}
# Drawn B centre (540,320) -> world (1080,640) -> B local (120,80); drawn A (100,80) -> world (200,160) -> A local (60,60).
click_check click_b1 540 320 client_b client_b 120 80 || result=FAIL
click_check click_a 100 80 client client 60 60 || result=FAIL
click_check click_b2 540 320 client_b client_b 120 80 || result=FAIL

hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-FOCUS empty canvas and click focus (V-17)" "camera P=(0,0) z=0.5; output $SIZE scale $SCALE"
[[ $result == PASS ]]
