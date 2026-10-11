#!/usr/bin/env bash
# T-DRAG-SCOPE (FR-012, T060). Camera z = 0.5, P = (0, 0); a --resizable 800x600 client at logical (140,100) is drawn at
# (70,50) 400x300.
#  1. A held-button drag inside the client: every motion while BTN_LEFT is held arrives at surfaceLocal(S) +-1 and the
#     press/release reach the client.
#  2. A compositor move drag (the nested config's no-modifier side-button movewindow bind, standing in for Super+LMB)
#     leaves the window's logical geometry unchanged (`hyprctl -j clients` of the nested instance before/after) and
#     the nested log records it as blocked.
#  3. A border resize drag (resize_on_border, pressed in the drawn border grab area) of this floating window resizes
#     it through the camera: width + d/z +-1, position unchanged, no "blocked resize" log line. Feature 002 FR-009
#     (patch 0012) replaced Feature 001's "resize blocked" here; tiled resize stays blocked (t_resize.sh).
# Setup validity (identity control, run last): the same move and border-resize drags at identity must change the
# geometry, else the non-identity "unchanged" results are vacuous and the run is BLOCKED.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_POINTER_TOLERANCE:-1}"

hvx_init_run patched t_drag_scope
hvx_copy_build_info patched
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
SESSION_DIR=$(hvx_session_dir)
hvx_client_start client 800x600 --resizable
hvx_vpointer_start vp

steps() { # NAME ; commands from stdin
    cat > "$HVX_EVID/$1.cmds"
    hvx_pointer_run "$1.steps" vp "$HVX_EVID/$1.cmds" client
}
geometry() { # NAME: at/size of the test client from the nested instance
    hvx_hyprctl patched -j clients > "$HVX_EVID/$1.clients.json" || hvx_die_blocked "$1: clients query failed"
    python3 -I -c "
import json, sys
c = [w for w in json.load(open(sys.argv[1])) if w.get('title') == 'client']
print(json.dumps({'at': c[0]['at'], 'size': c[0]['size']}) if len(c) == 1 else 'missing')" "$HVX_EVID/$1.clients.json"
}

hvx_nested dev camera 0 0 0.5 > "$HVX_EVID/camera.reply" || hvx_die_blocked "dev camera rejected"
hvx_capture camera client
result=PASS

# 1. Held-button drag inside the client: (200,150) -> world (400,300) -> local (260,200), etc.
printf 'move 200 150\ndown\nmove 260 190\nmove 330 240\nmove 380 300\nup\n' | steps held
python3 -I - "$HVX_EVID/held.steps.json" "$TOL" "$HVX_EVID/held.result.json" << 'PY' || result=FAIL
import json, sys
steps = json.load(open(sys.argv[1]))["steps"]
tol = float(sys.argv[2])
reasons = []
for i, step in enumerate(steps):
    words = step["cmd"].split()
    events = step["events"]["client"]
    if words[0] == "move":
        sx, sy = float(words[1]), float(words[2])
        lx, ly = sx / 0.5 - 140, sy / 0.5 - 100  # surfaceLocal(S) with P = (0, 0), z = 0.5, origin (140, 100)
        motion = [e for e in events if e["ev"] in ("enter", "motion")]
        if not motion:
            reasons.append(f"no_motion:{i}")
        elif abs(motion[-1]["x"] - lx) > tol or abs(motion[-1]["y"] - ly) > tol or motion[-1].get("surface") != "main":
            reasons.append(f"local_mismatch:{i}:{motion[-1]}")
    else:
        want = "pressed" if words[0] == "down" else "released"
        if [e["state"] for e in events if e["ev"] == "button"] != [want]:
            reasons.append(f"button_{want}_missing:{i}")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons}, open(sys.argv[3], "w"), indent=1)
print("held-button drag:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY

# 2. Compositor move drag (side button 275) and 3. border resize drag (BTN_LEFT in the right border grab area: drawn
# right edge x = 470, (474,300) -> world (948,600), 8 px right of the edge, inside border 2 + grab 15; below the
# window centre y = 400, so stock picks the bottom-right corner: LayoutManager.hpp cornerFromBox).
before=$(geometry before)
printf 'move 270 200\ndown 275\nmove 370 260\nmove 470 320\nup 275\n' | steps move_drag
after_move=$(geometry after_move)
# Resize drags pace their moves by > 1 frame (pointer_driver.py "pause"): Hyprland skips faster resize updates.
printf 'move 474 300\ndown\npause 40\nmove 520 340\npause 40\nmove 560 380\nup\n' | steps resize_drag
after_resize=$(geometry after_resize)
printf '%s\n%s\n%s\n' "$before" "$after_move" "$after_resize" > "$HVX_EVID/geometry.txt"
[[ $before != missing && $before == "$after_move" ]] || { echo "move drag changed the geometry while non-identity" >&2; result=FAIL; }
# Bottom-right corner resize by d = (560 - 474, 380 - 300) = (86, 80) screen px = (172, 160) world units at z = 0.5
# (worldDelta); the top-left corner stays.
python3 -I - "$after_move" "$after_resize" "$TOL" > "$HVX_EVID/resize.check" << 'PY' || { echo "border resize did not follow the camera" >&2; result=FAIL; }
import json, sys
a, b, tol = json.loads(sys.argv[1]), json.loads(sys.argv[2]), float(sys.argv[3])
want = [a["size"][0] + 172, a["size"][1] + 160]
ok = a["at"] == b["at"] and all(abs(b["size"][i] - want[i]) <= tol for i in (0, 1))
print(json.dumps({"before": a, "after": b, "expect_size": want, "ok": ok}))
sys.exit(0 if ok else 1)
PY

# Identity control: the same drags must act on the stock path. Pressing inside the window centre (540,400) / on the
# right border grab area and dragging. It validates the "unchanged" results above, so it runs only if they passed: a
# FAIL already stands, and the window it moved would put the control's press positions elsewhere.
if [[ $result == PASS ]]; then
    hvx_nested dev panic > /dev/null || hvx_die_blocked "panic failed"
    hvx_capture identity client
    id_before=$(geometry id_before)
    printf 'move 540 400\ndown 275\nmove 590 430\nmove 640 460\nup 275\n' | steps id_move_drag
    id_after_move=$(geometry id_after_move)
    printf '%s\n%s\n' "$id_before" "$id_after_move" > "$HVX_EVID/identity-geometry.txt"
    [[ $id_before != missing && $id_after_move != "$id_before" ]] \
        || hvx_die_blocked "identity control: the side-button move drag did not move the window (setup invalid)"
    BORDER_X=$(python3 -I -c "import json,sys; g=json.loads(sys.argv[1]); print(g['at'][0] + g['size'][0] + 8)" "$id_after_move")
    BORDER_Y=$(python3 -I -c "import json,sys; g=json.loads(sys.argv[1]); print(g['at'][1] + g['size'][1] // 2)" "$id_after_move")
    printf 'move %s %s\ndown\npause 40\nmove %s %s\npause 40\nmove %s %s\nup\n' "$BORDER_X" "$BORDER_Y" $((BORDER_X + 30)) "$BORDER_Y" \
        $((BORDER_X + 60)) "$BORDER_Y" | steps id_resize_drag
    id_after_resize=$(geometry id_after_resize)
    echo "$id_after_resize" >> "$HVX_EVID/identity-geometry.txt"
    [[ $id_after_resize != "$id_after_move" ]] \
        || hvx_die_blocked "identity control: the border resize drag did not resize the window (setup invalid)"
fi

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
blocked_move=$(grep -c "HyprVX: blocked a compositor move drag" "$HVX_EVID/nested-hyprland.log" || true)
blocked_resize=$(grep -c "HyprVX: blocked a compositor resize drag" "$HVX_EVID/nested-hyprland.log" || true)
echo "blocked move drags: $blocked_move; blocked resize drags: $blocked_resize" | tee "$HVX_EVID/blocked.txt"
[[ $blocked_move -ge 1 && $blocked_resize == 0 ]] || result=FAIL
hvx_finish "$result" "T-DRAG-SCOPE held-button drag local coordinates; move blocked, floating resize through the camera (FR-012, F002 FR-009)" \
    "camera P=(0,0) z=0.5; side-button movewindow bind for Super+LMB; output $SIZE scale $SCALE"
[[ $result == PASS ]]
