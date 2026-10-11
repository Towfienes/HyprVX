#!/usr/bin/env bash
# T-STALE (US2.3, V-12, T029): 50 seeded random camera states; each ready frame must equal the frame rendered after
# `hyprvx-dev full-damage` (diff, 0 pixels). The seed is recorded; HVX_STALE_SEED overrides it.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SEED="${HVX_STALE_SEED:-20261011}"
STATES="${HVX_STALE_STATES:-50}"

hvx_init_run patched t_stale
hvx_copy_build_info patched
echo "$SEED" > "$HVX_EVID/seed"
python3 -I -c "
import random, sys
rng = random.Random(int(sys.argv[1]))
for _ in range(int(sys.argv[2])):
    print('%.6f %.6f %.6f' % (rng.uniform(-1200, 1000), rng.uniform(-800, 700), rng.uniform(0.1, 2.0)))" "$SEED" "$STATES" > "$HVX_EVID/cameras.txt"
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600
result=PASS
i=0
while read -r px py z; do
    i=$((i + 1))
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "state $i: dev camera rejected ($px $py $z)"
    hvx_capture "state$i" client
    hvx_nested dev full-damage > /dev/null || hvx_die_blocked "state $i: full-damage failed"
    hvx_capture "state$i-full" client
    hvx_analyze "state$i-diff" diff "$HVX_EVID/state$i.png" --reference "$HVX_EVID/state$i-full.png" --max-pixels 0 || result=FAIL
done < "$HVX_EVID/cameras.txt"
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-STALE $STATES random camera states vs full damage (V-12)" "seed $SEED output $SIZE scale $SCALE"
[[ $result == PASS ]]
