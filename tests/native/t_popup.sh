#!/usr/bin/env bash
# T-POPUP (US3.4, FR-003, V-18 and V-19, T059). The client maps an xdg_popup at parent-local (300,200) 200x120 and a
# wl_subsurface at (40,380) 160x100 (test-tooling 1.3), i.e. world boxes (440,300,200,120) and (180,480,160,100).
#  V-18: for each camera state both are drawn attached to the parent, transformed and unclipped (all four markers of
#        their own palette at forward(box) +-2 px), and pointer moves over their drawn location reach them (enter/motion
#        for surface popup/sub at surfaceLocal(S) +-1).
#  V-19: a second client maps a popup at parent-local (3000,3000), outside every monitor; the nested compositor must
#        keep running (session verified, dev state answers, a capture succeeds) with identity and non-identity camera.
#        A compositor that dies here is a FAIL (the defect), not a BLOCKED run.
# Setup validity: at identity the popup must be where the client asked (popup_configure 300,200) and the identity
# checks must pass, else BLOCKED (the scene is wrong, not the camera).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
PTOL="${HVX_POINTER_TOLERANCE:-1}"
POPUP="440,300,200,120"
SUB="180,480,160,100"
POINTS="$HVX_REPO/tests/native/pointer_points.py"

hvx_init_run patched t_popup
hvx_copy_build_info patched
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600 --popup 300,200,200,120 --subsurface 40,380,160,100 ${HVX_CLIENT_ARGS:-}
hvx_wait_for "$HVX_EVID/client.jsonl" '"ev": "popup_configure"' || hvx_die_blocked "popup was never configured"
grep -q '"ev": "popup_configure", "x": 300, "y": 200, "w": 200, "h": 120' "$HVX_EVID/client.jsonl" \
    || hvx_die_blocked "compositor placed the popup elsewhere at identity (setup invalid)"
hvx_vpointer_start vp

box_on_screen() { # WORLD_BOX PX PY Z -> logical screen box X,Y,W,H of forward(box)
    python3 -I -c "
import sys
x, y, w, h = map(float, sys.argv[1].split(',')); px, py, z = map(float, sys.argv[2:5])
print('%r,%r,%r,%r' % ((x - px) * z, (y - py) * z, w * z, h * z))" "$@"
}

result=PASS
judge_state() { # NAME PX PY Z
    local name=$1 px=$2 py=$3 z=$4 set box
    hvx_nested dev camera "$px" "$py" "$z" > "$HVX_EVID/$name.reply" || hvx_die_blocked "$name: dev camera rejected"
    hvx_capture "$name" client
    for set in popup sub; do
        box=$([[ $set == popup ]] && echo "$POPUP" || echo "$SUB")
        hvx_analyze "$name-$set" bbox "$HVX_EVID/$name.png" --marker-set "$set" --tolerance "$TOL" \
            --expect-box "$(hvx_phys_box "$(box_on_screen "$box" "$px" "$py" "$z")")" || result=FAIL
        python3 -I "$POINTS" points --camera "$px" "$py" "$z" --surface "$box" --n 10 --margin 3 \
            --commands "$HVX_EVID/$name-$set.cmds" --meta "$HVX_EVID/$name-$set.points.json" \
            || hvx_die_blocked "$name: cannot sample the drawn $set"
        hvx_pointer_run "$name-$set.steps" vp "$HVX_EVID/$name-$set.cmds" client
        python3 -I "$POINTS" check --meta "$HVX_EVID/$name-$set.points.json" --steps "$HVX_EVID/$name-$set.steps.json" \
            --client client --surface-name "$set" --tol "$PTOL" --out "$HVX_EVID/$name-$set.input.json" || result=FAIL
    done
}

# Identity first: the same checks on the stock path (control), then two camera states.
hvx_nested dev panic > /dev/null || hvx_die_blocked "panic failed"
judge_state identity 0 0 1
[[ $result == PASS ]] || hvx_die_blocked "popup/subsurface checks fail at identity (setup invalid)"
judge_state z05 0 0 0.5
judge_state z15 100 150 1.5

# V-19: a popup outside every monitor, at identity and with the camera on.
offmon=PASS
hvx_client_start client_off 300x200 --popup 3000,3000,100,80
hvx_wait_for "$HVX_EVID/client_off.jsonl" '"ev": "popup_configure"' || offmon=FAIL
for st in "identity 0 0 1" "z05 0 0 0.5"; do
    read -r tag px py z <<< "$st"
    if [[ $tag == identity ]]; then
        hvx_nested dev panic > /dev/null 2>&1 || offmon=FAIL
    else
        hvx_nested dev camera "$px" "$py" "$z" > /dev/null 2>&1 || offmon=FAIL
    fi
    hvx_nested status > "$HVX_EVID/offmon-$tag.status" 2>&1 || offmon=FAIL
    hvx_nested dev state > "$HVX_EVID/offmon-$tag.state.json" 2>&1 || offmon=FAIL
    hvx_nested capture "$HVX_EVID/offmon-$tag.png" > /dev/null 2>&1 || offmon=FAIL
done
echo "V-19 off-monitor popup: $offmon" | tee "$HVX_EVID/offmon.result"
[[ $offmon == PASS ]] || result=FAIL

hvx_nested dev panic > /dev/null 2>&1 || true
if ! hvx_stop >> "$HVX_EVID/session.log" 2>&1; then
    # A compositor that died during V-19 is the defect under test; clean the stale session without signalling anything.
    [[ $offmon == FAIL ]] || hvx_die_blocked "session did not stop"
    hvx_nested recover >> "$HVX_EVID/session.log" 2>&1 || true
    HVX_OWN_SESSION=0
fi
hvx_finish "$result" "T-POPUP popup and subsurface drawing and input (V-18), off-monitor popup (V-19)" \
    "popup $POPUP, subsurface $SUB; states identity, z0.5 P(0,0), z1.5 P(100,150); output $SIZE scale $SCALE"
[[ $result == PASS ]]
