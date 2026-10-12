#!/usr/bin/env bash
# T-TP-OFF (Feature 003 FR-001, SC-006, V-09; scroll part T021, gesture part T029). Same scene and actions in a stock
# session (no plugin) and in the form's session with the policy and test plugins loaded and Drift OFF:
#  - a finger scroll sequence on empty canvas, then one over a;
#  - every capture of the form session is byte-identical to the stock capture of the same step, and a's scroll events
#    are identical in both sessions (nothing consumed while Drift is off).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_off
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

# scene SESSION_PREFIX: the scroll actions; captures and step files are named PREFIX-*.
scene() {
    local p=$1 T
    tp_capture "$p-start"
    T=$(tp_next_time)
    tp_run "$p-canvas" "fscroll $T 10 10 4 5 stop"
    tp_capture "$p-canvas"
    tp_run "$p-over-a" "move 290 200"
    T=$(tp_next_time)
    tp_run "$p-window" "fscroll $T 10 10 0 5 stop"
    tp_capture "$p-window"
    tp_axis_list "$p-window" a > "$HVX_EVID/$p.a.axis"
    tp_axis_list "$p-canvas" a >> "$HVX_EVID/$p.a.axis"
}

tp_setup --stock
scene stock
for w in a b c; do mv "$HVX_EVID/$w.jsonl" "$HVX_EVID/stock.$w.jsonl"; done
hvx_stop >> "$HVX_EVID/stock.session.log" 2>&1 || hvx_die_blocked "stock session did not stop"

tp_setup --no-drift
scene off
for step in start canvas window; do
    hvx_analyze "off-$step-vs-stock" diff "$HVX_EVID/off-$step.png" --reference "$HVX_EVID/stock-$step.png" --max-pixels 0 \
        || fail "Drift off: capture '$step' differs from stock"
done
cmp -s "$HVX_EVID/off.a.axis" "$HVX_EVID/stock.a.axis" || fail "Drift off: a's scroll events differ from stock"
[[ -s $HVX_EVID/stock.a.axis ]] || hvx_die_blocked "the stock session logged no scroll event for a (comparison vacuous)"

# Gesture part: T029.

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-OFF Drift off: captures and client scroll events equal to stock (V-09)" \
    "form $HVX_FORM; canvas 10x(4,5); over a 10x(0,5); output $SIZE scale $SCALE"
[[ $result == PASS ]]
