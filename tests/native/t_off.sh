#!/usr/bin/env bash
# T-OFF (US4.1, V-13, SC-004, T041): from 10 random non-identity cameras, `hyprvx-dev panic` must return the next
# ready frame to the identity image of the same session (diff, 0 pixels), 10/10.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SEED="${HVX_OFF_SEED:-424242}"

hvx_init_run patched t_off
hvx_copy_build_info patched
echo "$SEED" > "$HVX_EVID/seed"
python3 -I -c "
import random, sys
rng = random.Random(int(sys.argv[1]))
for _ in range(10):
    print('%.6f %.6f %.6f' % (rng.uniform(-1200, 1000), rng.uniform(-800, 700), rng.uniform(0.1, 2.0)))" "$SEED" > "$HVX_EVID/cameras.txt"
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600
hvx_capture identity client
result=PASS
i=0
while read -r px py z; do
    i=$((i + 1))
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "case $i: dev camera rejected"
    hvx_capture "case$i-camera" client
    hvx_nested dev panic > /dev/null || hvx_die_blocked "case $i: panic failed"
    hvx_capture "case$i-off" client
    hvx_analyze "case$i-diff" diff "$HVX_EVID/case$i-off.png" --reference "$HVX_EVID/identity.png" --max-pixels 0 || result=FAIL
done < "$HVX_EVID/cameras.txt"
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-OFF panic returns to the identity image 10/10 (V-13)" "seed $SEED output $SIZE scale $SCALE"
[[ $result == PASS ]]
