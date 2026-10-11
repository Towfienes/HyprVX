#!/usr/bin/env bash
# T-PANIC-DRIFT (Feature 002 US1.5, FR-002, V-09, T027): from Drift with 10 random non-identity cameras and window offsets
# (dev control), `hyprvx:panic` returns the first ready frame after the reply to the stock image of the same scene
# (diff, 0 pixels), 10/10; afterwards Drift is OFF (Super+Escape reaches the focused client).
# Reference: a stock session of the scene (floating clients a, b, c; c focused).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SEED="${HVX_PANIC_SEED:-20261011}"

hvx_init_run "$(hvx_form_label)" t_panic_drift
hvx_copy_build_info stock
echo "$SEED" > "$HVX_EVID/seed"
python3 -I -c "
import random, sys
rng = random.Random(int(sys.argv[1]))
for _ in range(10):
    cam = (rng.uniform(-900, 700), rng.uniform(-600, 500), rng.uniform(0.1, 2.0))
    offs = [(rng.uniform(-600, 600), rng.uniform(-400, 400)) for _ in range(3)]
    print(' '.join('%.6f' % v for v in cam + sum(offs, ())))" "$SEED" > "$HVX_EVID/cases.txt"
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
scene() {
    hvx_client_start a 300x200
    hvx_client_start b 300x200 --app-id hyprvx-testclient-b
    hvx_client_start c 300x200 --app-id hyprvx-testclient-c
}

hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/stock.session.log" 2>&1 \
    || { cat "$HVX_EVID/stock.session.log" >&2; hvx_die_blocked "stock session did not start"; }
HVX_OWN_SESSION=1
scene
hvx_capture stock c
for w in a b c; do mv "$HVX_EVID/$w.jsonl" "$HVX_EVID/stock.$w.jsonl"; done
hvx_stop >> "$HVX_EVID/stock.session.log" 2>&1 || hvx_die_blocked "stock session did not stop"

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
scene
hvx_vkeyboard_start kb
ADDR=("$(hvx_window_address hyprvx-testclient)" "$(hvx_window_address hyprvx-testclient-b)" "$(hvx_window_address hyprvx-testclient-c)")
hvx_capture pre c
i=0
while read -r px py z oax oay obx oby ocx ocy; do
    i=$((i + 1))
    hvx_dispatch hyprvx:on > "$HVX_EVID/case$i.on.reply" 2>&1 || true
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "case $i: dev camera rejected"
    hvx_nested dev offset "${ADDR[0]}" "$oax" "$oay" > /dev/null || hvx_die_blocked "case $i: dev offset rejected"
    hvx_nested dev offset "${ADDR[1]}" "$obx" "$oby" > /dev/null || hvx_die_blocked "case $i: dev offset rejected"
    hvx_nested dev offset "${ADDR[2]}" "$ocx" "$ocy" > /dev/null || hvx_die_blocked "case $i: dev offset rejected"
    hvx_capture "case$i-drift" c
    hvx_differs "case$i-validity" "$HVX_EVID/case$i-drift.png" "$HVX_EVID/stock.png" \
        || hvx_die_blocked "case $i: Drift state looks like stock (setup invalid)"
    hvx_dispatch hyprvx:panic > "$HVX_EVID/case$i.panic.reply" 2>&1 || true
    hvx_capture "case$i-panic" c
    hvx_analyze "case$i-diff" diff "$HVX_EVID/case$i-panic.png" --reference "$HVX_EVID/stock.png" --max-pixels 0 \
        || fail "case $i: first capture after panic differs from stock"
    hvx_drift_off_probe kb c || fail "case $i: Drift still on after panic (Super+Escape consumed)"
done < "$HVX_EVID/cases.txt"
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-PANIC-DRIFT panic from Drift: first capture = stock, Drift OFF, 10/10 (V-09)" \
    "form $HVX_FORM; seed $SEED; output $SIZE scale $SCALE"
[[ $result == PASS ]]
