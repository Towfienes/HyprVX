#!/usr/bin/env bash
# GATE-A (V-23, T022): three nested stock sessions with the reference scene (one 800x600 hyprvx-testclient at the
# fixed window-rule position) must capture pixel-identical frames (analyze_frame.py diff, 0 pixels). On failure fix
# the harness; never loosen T-ID.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run stock gate_a
hvx_copy_build_info stock
for i in 1 2 3; do
    hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session$i.log" 2>&1 \
        || { cat "$HVX_EVID/session$i.log" >&2; hvx_die_blocked "nested stock session $i did not start"; }
    hvx_client_start client "800x600"
    hvx_capture "stock$i" client
    hvx_analyze "stock$i-bbox" bbox "$HVX_EVID/stock$i.png" --expect-box 140,100,800,600 --tolerance 2 || true
    mv "$HVX_EVID/client.jsonl" "$HVX_EVID/client$i.jsonl"
    hvx_nested stop >> "$HVX_EVID/session$i.log" 2>&1 || hvx_die_blocked "nested stock session $i did not stop cleanly"
done

result=PASS
for i in 2 3; do
    if ! hvx_analyze "diff1-$i" diff "$HVX_EVID/stock$i.png" --reference "$HVX_EVID/stock1.png" --max-pixels 0; then
        result=FAIL
    fi
done
hvx_finish "$result" "GATE-A stock x3 pixel identity (V-23)" "output $SIZE scale $SCALE"
[[ $result == PASS ]]
