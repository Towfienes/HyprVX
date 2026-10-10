#!/usr/bin/env bash
# T-STALE-STATIC (US2.5, V-12b, T030): fixed non-identity camera; the client paints small rectangles in a fixed
# order through its control FIFO (partial damage only). Each partial-damage frame must equal a forced full-damage
# frame of the same client state (diff, 0 pixels). A camera change always damages the whole output, so this is the
# test that exercises the camera's partial-damage path.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run patched t_stale_static
hvx_copy_build_info patched
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 800x600
hvx_nested dev camera -100 -60 0.75 > /dev/null || hvx_die_blocked "dev camera rejected"
hvx_capture base client
result=PASS
fills=("fill 100 100 37 23 C04020" "fill 401 251 64 64 20A0C0" "fill 0 300 13 7 E0E020" "fill 777 577 23 23 30C030"
       "fill 250 0 150 3 A040E0" "fill 33 533 101 41 F08080" "fill 600 120 1 200 FFFFFF" "fill 120 420 333 1 808080")
for k in "${!fills[@]}"; do
    hvx_client_cmd client "${fills[$k]}"
    hvx_capture "fill$k" client
    hvx_nested dev full-damage > /dev/null || hvx_die_blocked "fill $k: full-damage failed"
    hvx_capture "fill$k-full" client
    hvx_analyze "fill$k-diff" diff "$HVX_EVID/fill$k.png" --reference "$HVX_EVID/fill$k-full.png" --max-pixels 0 || result=FAIL
done
printf '%s\n' "${fills[@]}" > "$HVX_EVID/fills.txt"
hvx_nested dev panic > /dev/null || true
hvx_nested stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-STALE-STATIC partial damage under a fixed camera (V-12b)" "camera P=(-100,-60) z=0.75; output $SIZE scale $SCALE"
[[ $result == PASS ]]
