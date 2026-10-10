#!/usr/bin/env bash
# T-INVALID (US4.2, T042): NaN, Inf, z <= 0, z < 0.1 and z > 2.0 camera requests are rejected and change nothing:
# the capture after each request equals the capture before it (diff, 0 pixels). The dispatcher reply is recorded as
# a diagnostic only. Starts from a valid non-identity camera so a silent reset would also be caught.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run patched t_invalid
hvx_copy_build_info patched
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600
hvx_nested dev camera 50 40 0.8 > /dev/null || hvx_die_blocked "valid starting camera rejected"
hvx_capture before client
result=PASS
k=0
for request in "nan 0 1" "0 inf 1" "0 0 inf" "0 0 0" "0 0 -1" "0 0 0.05" "0 0 2.5"; do
    k=$((k + 1))
    read -r px py z <<< "$request"
    hvx_nested dev camera "$px" "$py" "$z" > "$HVX_EVID/req$k.reply" 2>&1 || true
    echo "$request" >> "$HVX_EVID/req$k.reply"
    hvx_capture "after$k" client
    hvx_analyze "req$k-diff" diff "$HVX_EVID/after$k.png" --reference "$HVX_EVID/before.png" --max-pixels 0 || result=FAIL
done
hvx_nested dev panic > /dev/null || true
hvx_nested stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-INVALID rejected camera values change nothing (US4.2)" "output $SIZE scale $SCALE"
[[ $result == PASS ]]
