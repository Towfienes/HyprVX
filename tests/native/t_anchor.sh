#!/usr/bin/env bash
# T-ANCHOR (Feature 002 US2, FR-006, V-12, T033). One floating 400x300 client a, whose surface top-left corner is at
# logical (140,100) (config rule). The pointer is put exactly on that corner, Drift is entered (identity camera), and
# the camera is zoomed 20 times at that fixed pointer: 10 steps of `hyprvx:zoom 0.9`, then 10 Super + wheel-up detents
# (zoom_step 1.1 each). The world point under the pointer is a's top-left corner, so after every step the corner marker
# must stay under the pointer: measured top-left within 1 px (physical) of the pointer, cumulative over the 20 steps,
# and the box size w*z, h*z within the analyzer tolerance. Setup validity: the zoom must change the image.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
AX=140
AY=100
W=400
H=300

hvx_init_run "$(hvx_form_label)" t_anchor
hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a "${W}x${H}"
hvx_vkeyboard_start kb
hvx_vpointer_start vp
printf 'move %s %s\n' "$AX" "$AY" > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a
hvx_capture identity a
hvx_analyze identity bbox "$HVX_EVID/identity.png" --expect-box "$(hvx_phys_box "$AX,$AY,$W,$H")" --tolerance "$TOL" \
    || hvx_die_blocked "a is not at its rule position (setup invalid)"
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true

result=PASS
z=1
: > "$HVX_EVID/steps.txt"
measure() { # STEP
    local box
    box=$(hvx_phys_box "$AX,$AY,$(python3 -I -c "print($W * $z)"),$(python3 -I -c "print($H * $z)")")
    hvx_capture "step$1" a
    hvx_analyze "step$1" bbox "$HVX_EVID/step$1.png" --expect-box "$box" --tolerance "$TOL" \
        || { echo "step $1: box not at the anchored position (z=$z)" >&2; result=FAIL; }
    echo "$1 $z $box" >> "$HVX_EVID/steps.txt"
}
for step in $(seq 1 10); do
    hvx_dispatch hyprvx:zoom 0.9 > "$HVX_EVID/step$step.reply" 2>&1 || true
    z=$(python3 -I -c "print($z * 0.9)")
    measure "$step"
done
hvx_differs zoom-validity "$HVX_EVID/step10.png" "$HVX_EVID/identity.png" || hvx_die_blocked "hyprvx:zoom left the image unchanged"
hvx_key kb down KEY_LEFTMETA
for step in $(seq 11 20); do
    printf 'scroll 0 -1\n' > "$HVX_EVID/scroll$step.cmds"
    hvx_pointer_run "scroll$step.steps" vp "$HVX_EVID/scroll$step.cmds" a
    z=$(python3 -I -c "print($z * 1.1)")
    measure "$step"
done
hvx_key kb up KEY_LEFTMETA

# Cumulative drift of the corner under the pointer (the analyzer JSON holds the measured box).
python3 -I - "$HVX_EVID" "$(hvx_phys_box "$AX,$AY,1,1")" > "$HVX_EVID/drift.txt" << 'PY' || result=FAIL
import json, sys
evid, anchor = sys.argv[1], sys.argv[2].split(",")
ax, ay = float(anchor[0]), float(anchor[1])
worst = 0.0
for step in range(1, 21):
    box = json.load(open(f"{evid}/step{step}.json")).get("bbox")
    if not box:
        print(f"step {step}: no box measured"); sys.exit(1)
    worst = max(worst, abs(box["x"] - ax), abs(box["y"] - ay))
print(f"max drift of the anchored corner over 20 steps: {worst} px (limit 1)")
sys.exit(0 if worst <= 1 else 1)
PY
cat "$HVX_EVID/drift.txt"
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-ANCHOR 20 zoom steps (dispatcher, Super + wheel) at a fixed pointer, drift <= 1 px (V-12)" \
    "form $HVX_FORM; pointer and anchor at a's corner ($AX,$AY); 10 x zoom 0.9 then 10 Super+wheel-up detents (zoom_step 1.1); output $SIZE scale $SCALE"
[[ $result == PASS ]]
