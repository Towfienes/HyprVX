#!/usr/bin/env bash
# T-MULTI (Feature 002 US2.1/US2.5 mechanism, FR-005/FR-013, V-04, T016). Three floating 300x200 test clients with
# distinct world offsets O set by the dev control (no policy): for camera states at z in {0.25, 0.5, 1, 2} every window
# whose drawn box lies on the output is found within +-2 px of forwardBox(L + O) with all four markers (analyzer bbox,
# searched only around that window). A TOP layer client (--layer TOP) must stay byte-identical in its region while the
# camera and the offsets change (FR-013). L comes from `hyprctl -j clients` (Hyprland's logical box, never written).
# Setup validity: with offsets 0 and the identity camera each window is found at L (stock placement), else BLOCKED.
# HVX_FORM=P runs the patched build (dev control in the binary); HVX_FORM=S the stock build + plugin S-dev.
# HVX_MULTI_CAMERA=policy (T040, run id t_multi_policy): the policy plugin is loaded, Drift is on, and each camera state
# is reached through the Drift controls with the pointer at screen (0,0): hyprvx:reset, hyprvx:zoom z (anchor (0,0)
# keeps P = 0), hyprvx:pan (-P_x z, -P_y z) (P' = P - d/z). Offsets stay dev offsets (the move drag is T048).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
LAYER_BOX="20,640,240,140"
CAMERA_MODE="${HVX_MULTI_CAMERA:-dev}"
case $CAMERA_MODE in dev|policy) ;; *) echo "HVX_MULTI_CAMERA must be dev or policy" >&2; exit 2 ;; esac
declare -A OFFSET=([a]="-100,-60" [b]="-180,-460" [c]="-60,180")
declare -A CLASS=([a]=hyprvx-testclient [b]=hyprvx-testclient-b [c]=hyprvx-testclient-c)
# P_x P_y z: every window is judged in at least one state per zoom (states chosen so judged windows never overlap the
# layer or each other).
STATES=("-1000 -600 0.25" "0 0 0.5" "0 0 1" "20 20 1" "20 20 2" "700 60 2" "420 420 2")

TEST_ID=t_multi
POLICY=()
[[ $CAMERA_MODE == policy ]] && { TEST_ID=t_multi_policy; POLICY=(--policy); }
hvx_init_run "$(hvx_form_label)" "$TEST_ID"
hvx_start_form "${POLICY[@]}" --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested session (form $HVX_FORM) did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_client_start layer 240x140 --layer TOP --position 20,640 --app-id hyprvx-layer
hvx_hyprctl_form -j clients > "$HVX_EVID/clients.json"
declare -A ADDR BOX
for w in a b c; do
    read -r addr x y bw bh < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" "${CLASS[$w]}") \
        || hvx_die_blocked "client $w not in hyprctl clients"
    ADDR[$w]=$addr
    BOX[$w]="$x,$y,$bw,$bh"
done
printf '%s %s %s\n' a "${BOX[a]}" "${OFFSET[a]}" b "${BOX[b]}" "${OFFSET[b]}" c "${BOX[c]}" "${OFFSET[c]}" > "$HVX_EVID/scene.txt"

# judge NAME PX PY Z OFFSETS(0|1): capture, then bbox of every visible window and the layer region.
result=PASS
judge() {
    local name=$1 px=$2 py=$3 z=$4 use_offset=$5 w exp phys visible region off
    hvx_capture "$name" a
    for w in a b c; do
        off=$([[ $use_offset == 1 ]] && echo "${OFFSET[$w]}" || echo 0,0)
        exp=$(python3 -I "$GEOM" expect --camera "$px" "$py" "$z" --box "${BOX[$w]}" --offset "$off" --scale "$SCALE")
        visible=$(python3 -I -c "import json,sys; print(json.loads(sys.argv[1])['visible'])" "$exp")
        echo "$name $w $exp" >> "$HVX_EVID/expected.txt"
        [[ $visible == True ]] || continue
        phys=$(python3 -I -c "import json,sys; print(json.loads(sys.argv[1])['phys'])" "$exp")
        region=$(python3 -I "$GEOM" pad --box "$phys" --by 8)
        hvx_analyze "$name-$w" bbox "$HVX_EVID/$name.png" --expect-box "$phys" --tolerance "$TOL" --region "$region" || return 1
    done
    python3 -I "$GEOM" crop-equal "$HVX_EVID/identity0.png" "$HVX_EVID/$name.png" "$(hvx_phys_box "$LAYER_BOX")" \
        > "$HVX_EVID/$name-layer.json" || return 1
}

# Setup validity: offsets 0, identity camera.
hvx_nested dev panic > /dev/null || hvx_die_blocked "dev panic failed"
judge identity0 0 0 1 0 || hvx_die_blocked "windows not at their logical boxes with offsets 0 (setup invalid)"
if [[ $CAMERA_MODE == policy ]]; then
    hvx_vpointer_start vp
    printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
    hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a
    hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
fi
set_camera() { # NAME P_X P_Y Z
    if [[ $CAMERA_MODE == dev ]]; then
        hvx_nested dev camera "$2" "$3" "$4" > /dev/null || hvx_die_blocked "$1: dev camera rejected"
        return
    fi
    {
        hvx_dispatch hyprvx:reset
        hvx_dispatch hyprvx:zoom "$4"
        hvx_dispatch hyprvx:pan "$(python3 -I -c "print(-$2 * $4)")" "$(python3 -I -c "print(-$3 * $4)")"
    } > "$HVX_EVID/$1.controls.reply" 2>&1 || true
}

for w in a b c; do
    read -r ox oy <<< "${OFFSET[$w]//,/ }"
    hvx_nested dev offset "${ADDR[$w]}" "$ox" "$oy" > "$HVX_EVID/offset-$w.reply" || hvx_die_blocked "dev offset rejected for $w"
done
hvx_nested dev state > "$HVX_EVID/offsets.state.json" || true
for st in "${STATES[@]}"; do
    read -r px py z <<< "$st"
    name="z${z}_P${px}_${py}"
    set_camera "$name" "$px" "$py" "$z"
    judge "$name" "$px" "$py" "$z" 1 || result=FAIL
done
printf '%s\n' "${STATES[@]}" > "$HVX_EVID/states.txt"
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-MULTI three offset windows +-$TOL px, top layer unchanged (V-04)" \
    "form $HVX_FORM; camera $CAMERA_MODE; offsets a ${OFFSET[a]} b ${OFFSET[b]} c ${OFFSET[c]}; ${#STATES[@]} camera states; output $SIZE scale $SCALE"
[[ $result == PASS ]]
