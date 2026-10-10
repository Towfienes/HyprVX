#!/usr/bin/env bash
# T-ID (US1, V-09, T023): with the identity camera, the patched build renders the reference scene pixel-identical to
# stock-from-source (diff, 0 pixels), three repetitions. passElementsAdded from `hyprvx-dev state` is recorded as a
# diagnostic only; US1.2 evidence is this pixel identity plus the source verification in T044 (Constitution II).
# Requires GATE-A to have passed for the same harness and config. T-ID FAIL is a stop condition.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
REPS="${HVX_TID_REPS:-3}"

hvx_init_run patched t_id
hvx_copy_build_info stock
hvx_copy_build_info patched
result=PASS
for i in $(seq 1 "$REPS"); do
    for variant in stock patched; do
        hvx_nested start --variant "$variant" --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/$variant$i.session.log" 2>&1 \
            || { cat "$HVX_EVID/$variant$i.session.log" >&2; hvx_die_blocked "nested $variant session $i did not start"; }
        hvx_client_start client "800x600"
        hvx_capture "$variant$i" client
        mv "$HVX_EVID/client.jsonl" "$HVX_EVID/$variant$i.client.jsonl"
        hvx_nested stop >> "$HVX_EVID/$variant$i.session.log" 2>&1 || hvx_die_blocked "nested $variant session $i did not stop"
    done
    hvx_analyze "tid$i" diff "$HVX_EVID/patched$i.png" --reference "$HVX_EVID/stock$i.png" --max-pixels 0 || result=FAIL
done
hvx_finish "$result" "T-ID stock vs patched identity x$REPS (V-09)" "output $SIZE scale $SCALE; passElementsAdded in patched*.state.json is diagnostic only"
[[ $result == PASS ]]
