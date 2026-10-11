#!/usr/bin/env bash
# T-DRAG-SCOPE (FR-012, T060). Camera z = 0.5, P = (0, 0); a --resizable 800x600 client at logical (140,100) is drawn at
# (70,50) 400x300.
#  1. A held-button drag inside the client: every motion while BTN_LEFT is held arrives at surfaceLocal(S) +-1 and the
#     press/release reach the client.
#  2. A compositor move drag (the nested config's no-modifier side-button movewindow bind, standing in for Super+LMB)
#     and 3. a border resize drag (resize_on_border, pressed in the drawn border grab area) leave the window's logical
#     geometry unchanged (`hyprctl -j clients` of the nested instance before/after), and the nested log records both
#     as blocked.
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
# The blocked-drag WARN lines must reach the nested log file; Hyprland stops file logging after startup by default.
hvx_hyprctl patched keyword debug:disable_logs false > "$HVX_EVID/logs.reply" || hvx_die_blocked "enabling the nested log failed"
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
# right edge x = 470, (474,200) -> world (948,400), 8 px right of the edge, inside border 2 + grab 15).
before=$(geometry before)
printf 'move 270 200\ndown 275\nmove 370 260\nmove 470 320\nup 275\n' | steps move_drag
after_move=$(geometry after_move)
printf 'move 474 200\ndown\nmove 520 240\nmove 560 280\nup\n' | steps resize_drag
after_resize=$(geometry after_resize)
printf '%s\n%s\n%s\n' "$before" "$after_move" "$after_resize" > "$HVX_EVID/geometry.txt"
[[ $before != missing && $before == "$after_move" && $before == "$after_resize" ]] || { echo "geometry changed while non-identity" >&2; result=FAIL; }

# Identity control: the same drags must act on the stock path. Pressing inside the window centre (540,400) / on the
# right border grab area and dragging.
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
printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$BORDER_X" "$BORDER_Y" $((BORDER_X + 30)) "$BORDER_Y" \
    $((BORDER_X + 60)) "$BORDER_Y" | steps id_resize_drag
id_after_resize=$(geometry id_after_resize)
echo "$id_after_resize" >> "$HVX_EVID/identity-geometry.txt"
[[ $id_after_resize != "$id_after_move" ]] \
    || hvx_die_blocked "identity control: the border resize drag did not resize the window (setup invalid)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
blocked_move=$(grep -c "HyprVX: blocked a compositor move drag" "$HVX_EVID/nested-hyprland.log" || true)
blocked_resize=$(grep -c "HyprVX: blocked a compositor resize drag" "$HVX_EVID/nested-hyprland.log" || true)
echo "blocked move drags: $blocked_move; blocked resize drags: $blocked_resize" | tee "$HVX_EVID/blocked.txt"
[[ $blocked_move -ge 1 && $blocked_resize -ge 1 ]] || result=FAIL
hvx_finish "$result" "T-DRAG-SCOPE held-button drag local coordinates; compositor drags blocked while non-identity (FR-012)" \
    "camera P=(0,0) z=0.5; side-button movewindow bind for Super+LMB; output $SIZE scale $SCALE"
[[ $result == PASS ]]
