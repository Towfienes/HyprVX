#!/usr/bin/env bash
# T-KEYS (Feature 002 US3, FR-010, V-16, T041). Floating clients a, b, c; Drift on with the camera zoomed to 0.5 at
# screen (0,0) (P = (0,0)) and world offsets a (100,50), c (-200,100) (dev control: the input seams read the mechanism
# offset). For each target in a, c, b: a plain left click at the centre of its drawn box forwardBox(L + O) focuses it,
# then `tap KEY_A` (vkeyboard) reaches only that client: its key log gains one KEY_A press, the others none.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
declare -A OFFSET=([a]="100,50" [b]="0,0" [c]="-200,100")
declare -A CLASS=([a]=hyprvx-testclient [b]=hyprvx-testclient-b [c]=hyprvx-testclient-c)

hvx_init_run "$(hvx_form_label)" t_keys
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
key_a() { grep -c '"ev": "key", "key": 30, "state": "pressed"' "$HVX_EVID/$1.jsonl" || true; }

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_vkeyboard_start kb
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a b c
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true
hvx_hyprctl_form -j clients > "$HVX_EVID/clients.json"
declare -A CENTRE
for w in a b c; do
    read -r addr x y bw bh < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" "${CLASS[$w]}") || hvx_die_blocked "no client $w"
    [[ ${OFFSET[$w]} == 0,0 ]] || { read -r ox oy <<< "${OFFSET[$w]//,/ }"; hvx_nested dev offset "$addr" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset $w rejected"; }
    CENTRE[$w]=$(python3 -I -c "
import json, sys
b = json.loads(sys.argv[1])['logical']
print(round(b[0] + b[2] / 2), round(b[1] + b[3] / 2))" "$(python3 -I "$GEOM" expect --camera 0 0 0.5 --box "$x,$y,$bw,$bh" --offset "${OFFSET[$w]}")")
done
hvx_capture drift a
: > "$HVX_EVID/counts.txt"
for target in a c b; do
    declare -A before=()
    for w in a b c; do before[$w]=$(key_a "$w"); done
    read -r cx cy <<< "${CENTRE[$target]}"
    printf 'move %s %s\nclick\n' "$cx" "$cy" > "$HVX_EVID/click-$target.cmds"
    hvx_pointer_run "click-$target.steps" vp "$HVX_EVID/click-$target.cmds" a b c
    hvx_key kb tap KEY_A
    for w in a b c; do hvx_ready "$w"; done
    for w in a b c; do
        now=$(key_a "$w")
        echo "$target $w $((now - before[$w]))" >> "$HVX_EVID/counts.txt"
        want=$([[ $w == "$target" ]] && echo 1 || echo 0)
        ((now - before[$w] == want)) || fail "click on $target then KEY_A: $w got $((now - before[$w])) presses (want $want)"
    done
done
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-KEYS key taps reach only the clicked offset window (V-16)" \
    "form $HVX_FORM; zoom 0.5 at (0,0); offsets a ${OFFSET[a]} c ${OFFSET[c]}; output $SIZE scale $SCALE"
[[ $result == PASS ]]
