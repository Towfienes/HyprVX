#!/usr/bin/env bash
# T-STALE-MW (Feature 002 US2.5, SC-007, V-06, T018). Three floating test clients; 50 seeded random states, each a
# camera (P, z) plus a world offset per window, all set through the dev control (no policy). Each ready frame (partial
# damage) must equal the frame rendered after `hyprvx-dev full-damage` (diff, 0 pixels). Offsets change between
# states, so stale regions at the old offsets would show. The seed is recorded; HVX_STALE_SEED overrides it. A TOP
# layer client (never transformed) is the ready-rule reference: a window moved off the output gets no frame callbacks.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SEED="${HVX_STALE_SEED:-20261012}"
STATES="${HVX_STALE_STATES:-50}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_stale_mw
echo "$SEED" > "$HVX_EVID/seed"
python3 -I -c "
import random, sys
rng = random.Random(int(sys.argv[1]))
for _ in range(int(sys.argv[2])):
    cam = (rng.uniform(-900, 700), rng.uniform(-600, 500), rng.uniform(0.1, 2.0))
    offs = [(rng.uniform(-600, 600), rng.uniform(-400, 400)) for _ in range(3)]
    print(' '.join('%.6f' % v for v in cam + tuple(c for o in offs for c in o)))" "$SEED" "$STATES" > "$HVX_EVID/states.txt"
hvx_start_form --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested session (form $HVX_FORM) did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_client_start ref 100x60 --layer TOP --position 1170,730 --app-id hyprvx-layer
hvx_hyprctl_form -j clients > "$HVX_EVID/clients.json"
ADDR=()
for cls in hyprvx-testclient hyprvx-testclient-b hyprvx-testclient-c; do
    read -r addr _ < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" "$cls") || hvx_die_blocked "no client $cls"
    ADDR+=("$addr")
done
result=PASS
i=0
while read -r px py z oax oay obx oby ocx ocy; do
    i=$((i + 1))
    hvx_nested dev offset "${ADDR[0]}" "$oax" "$oay" > /dev/null || hvx_die_blocked "state $i: dev offset rejected"
    hvx_nested dev offset "${ADDR[1]}" "$obx" "$oby" > /dev/null || hvx_die_blocked "state $i: dev offset rejected"
    hvx_nested dev offset "${ADDR[2]}" "$ocx" "$ocy" > /dev/null || hvx_die_blocked "state $i: dev offset rejected"
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "state $i: dev camera rejected ($px $py $z)"
    hvx_capture "state$i" ref
    hvx_nested dev full-damage > /dev/null || hvx_die_blocked "state $i: full-damage failed"
    hvx_capture "state$i-full" ref
    hvx_analyze "state$i-diff" diff "$HVX_EVID/state$i.png" --reference "$HVX_EVID/state$i-full.png" --max-pixels 0 || result=FAIL
done < "$HVX_EVID/states.txt"
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-STALE-MW $STATES random camera/offset states vs full damage (V-06)" "form $HVX_FORM; seed $SEED; output $SIZE scale $SCALE"
[[ $result == PASS ]]
