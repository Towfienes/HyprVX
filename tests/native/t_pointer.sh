#!/usr/bin/env bash
# T-POINTER (US3.1, V-16, T057): with the camera at z = 0.25 plus pans (and z = 0.5, 1.5, 2.0), hyprvx-vpointer moves
# to >= 20 screen points S over the drawn window per camera state; every enter/motion the client receives must be
# for its main surface at surfaceLocal(S) = P + S/z - W(origin) within +-1 unit (SC-003). Setup validity first: at
# identity a move to S must arrive at S - W(origin) (stock behaviour); if not, the vpointer mapping is wrong and the
# run is BLOCKED, never judged.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_POINTER_TOLERANCE:-1}"
WIN="140,100,800,600" # logical box of the window (windowrule move 140 100, --size 800x600)
POINTS="$HVX_REPO/tests/native/pointer_points.py"

hvx_init_run patched t_pointer
hvx_copy_build_info patched
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600 ${HVX_CLIENT_ARGS:-}
hvx_vpointer_start vp

# name px py z: one camera state, sampled and judged.
result=PASS
judge_state() {
    local name=$1 px=$2 py=$3 z=$4
    hvx_nested dev camera "$px" "$py" "$z" > "$HVX_EVID/$name.reply" || hvx_die_blocked "$name: dev camera rejected"
    hvx_capture "$name" client
    python3 -I "$POINTS" points --camera "$px" "$py" "$z" --surface "$WIN" --commands "$HVX_EVID/$name.cmds" \
        --meta "$HVX_EVID/$name.points.json" || hvx_die_blocked "$name: cannot sample the drawn window"
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" client
    python3 -I "$POINTS" check --meta "$HVX_EVID/$name.points.json" --steps "$HVX_EVID/$name.steps.json" \
        --client client --surface-name main --tol "$TOL" --out "$HVX_EVID/$name.result.json"
}

# Setup validity (stock path, identity camera).
hvx_nested dev panic > /dev/null || hvx_die_blocked "panic failed"
python3 -I "$POINTS" points --camera 0 0 1 --surface "$WIN" --n 5 --commands "$HVX_EVID/setup.cmds" \
    --meta "$HVX_EVID/setup.points.json" || hvx_die_blocked "setup sampling failed"
hvx_pointer_run setup.steps vp "$HVX_EVID/setup.cmds" client
python3 -I "$POINTS" check --meta "$HVX_EVID/setup.points.json" --steps "$HVX_EVID/setup.steps.json" --client client \
    --surface-name main --tol "$TOL" --out "$HVX_EVID/setup.result.json" \
    || hvx_die_blocked "identity setup check failed: vpointer moves do not land at S (setup invalid)"

states=("z025_a -1460 -1100 0.25" "z025_b -460 -300 0.25" "z05 -500 -200 0.5" "z15 300 250 1.5" "z2 400 300 2.0")
for st in "${states[@]}"; do
    # shellcheck disable=SC2086
    judge_state $st || result=FAIL
done
printf '%s\n' "${states[@]}" > "$HVX_EVID/states.txt"
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-POINTER surface-local pointer coordinates (V-16)" \
    "window $WIN; 5 camera states x >= 20 points; tolerance $TOL; output $SIZE scale $SCALE"
[[ $result == PASS ]]
