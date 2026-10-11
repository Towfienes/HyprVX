#!/usr/bin/env bash
# T-MOVE (Feature 002 US4.1-4.2, FR-008, V-19, T046). Floating client a (300x200 at (140,100)); the user's own
# `bindm = SUPER, mouse:272, movewindow` is added at runtime. Drift on. For z in {2, 1, 0.5} (camera through the Drift
# controls with the pointer at screen (0,0), so P = (0,0)): a Super + left drag starting inside a's drawn box and moved
# by d on screen moves a's drawn box by d +-1 px (O changes by worldDelta = d / z), while `hyprctl clients` (at, size,
# workspace, floating) stays unchanged and socket2 shows no movewindow/movewindowv2 event (the user's bindm never runs).
# The cases run in that order so that a stays on the output (O accumulates over the cases).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
CASES=("2 40 -30" "1 -90 60" "0.5 120 70")

hvx_init_run "$(hvx_form_label)" t_move
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
measure() { # NAME LOGICAL_BOX TOLERANCE (the measured box lands in NAME.json)
    local phys
    phys=$(hvx_phys_box "$2")
    hvx_capture "$1" a
    hvx_analyze "$1" bbox "$HVX_EVID/$1.png" --expect-box "$phys" --tolerance "$3" --region "$(python3 -I "$GEOM" pad --box "$phys" --by 12)"
}
measured() { # NAME -> logical x y of the measured box
    python3 -I -c "
import json, sys
b = json.load(open(sys.argv[1]))['bbox']; s = float(sys.argv[2])
print(b['x'] / s, b['y'] / s)" "$HVX_EVID/$1.json" "$SCALE"
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_events_start
hvx_client_start a 300x200
hvx_vkeyboard_start kb
hvx_vpointer_start vp
hvx_hyprctl_form keyword bindm SUPER,mouse:272,movewindow > "$HVX_EVID/bindm.reply" 2>&1 || hvx_die_blocked "could not add the user's bindm"
hvx_clients_snapshot pre
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
: > "$HVX_EVID/moves.txt"
OX=0 OY=0
for c in "${CASES[@]}"; do
    read -r z dx dy <<< "$c"
    tag="z$z"
    printf 'move 0 0\n' > "$HVX_EVID/$tag-home.cmds"
    hvx_pointer_run "$tag-home.steps" vp "$HVX_EVID/$tag-home.cmds" a
    { hvx_dispatch hyprvx:reset; hvx_dispatch hyprvx:zoom "$z"; } > "$HVX_EVID/$tag.controls.reply" 2>&1 || true
    # Where a is drawn now (it carries the offsets of earlier cases): find it near its expected box.
    exp=$(python3 -I "$GEOM" expect --camera 0 0 "$z" --box 140,100,300,200 --offset "$OX,$OY" --scale 1)
    box=$(python3 -I -c "import json,sys; print(','.join('%g' % v for v in json.loads(sys.argv[1])['logical']))" "$exp")
    measure "$tag-before" "$box" 2 || hvx_die_blocked "$tag: a not where its offset and the camera put it (setup invalid)"
    read -r bx by < <(measured "$tag-before")
    read -r bw bh <<< "$(python3 -I -c "print(300 * $z, 200 * $z)")"
    sx=$(python3 -I -c "print(round($bx + $bw / 2))") sy=$(python3 -I -c "print(round($by + $bh / 2))")
    printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$sx" "$sy" $((sx + dx / 2)) $((sy + dy / 2)) $((sx + dx)) $((sy + dy)) \
        > "$HVX_EVID/$tag-drag.cmds"
    hvx_key kb down KEY_LEFTMETA
    hvx_pointer_run "$tag-drag.steps" vp "$HVX_EVID/$tag-drag.cmds" a
    hvx_key kb up KEY_LEFTMETA
    measure "$tag-after" "$(python3 -I -c "print('%g,%g,%g,%g' % ($bx + $dx, $by + $dy, $bw, $bh))")" 1 \
        || fail "$tag: drag by ($dx,$dy) did not move a's drawn box by d +-1 px"
    OX=$(python3 -I -c "print($OX + $dx / $z)") OY=$(python3 -I -c "print($OY + $dy / $z)")
    echo "$tag d=($dx,$dy) expected O=($OX,$OY)" >> "$HVX_EVID/moves.txt"
    hvx_clients_snapshot "$tag"
    cmp -s "$HVX_EVID/pre.clients.json" "$HVX_EVID/$tag.clients.json" || fail "$tag: hyprctl clients changed (L written)"
done
hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
moves=$(grep -cE ' movewindow(v2)?>>' "$HVX_EVID/events.log" || true)
echo "movewindow events: $moves" >> "$HVX_EVID/moves.txt"
[[ $moves == 0 ]] || fail "$moves movewindow events on socket2"
hvx_finish "$result" "T-MOVE Super + drag moves the drawn window by d, O by d/z, L unchanged, no movewindow (V-19)" \
    "form $HVX_FORM; cases (z dx dy): ${CASES[*]}; user bindm SUPER mouse:272 movewindow; output $SIZE scale $SCALE"
[[ $result == PASS ]]
