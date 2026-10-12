#!/usr/bin/env bash
# T-TP-OFF (Feature 003 FR-001, SC-006, V-09; scroll part T021, gesture part T029). Same scene and actions in a stock
# session (no plugin) and in the form's session with the policy and test plugins loaded and Drift OFF:
#  - a finger scroll sequence on empty canvas, then one over a;
#  - (T029) a 2-finger pinch on canvas (0.5), one over a (1.5), then 3-finger swipes left (float toggle), down (close)
#    and a 4-finger horizontal swipe (workspace): the bindings act alike in both sessions;
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
    gesture pinch 2 "2 0 0 0.5 0" "$TP_CANVAS_X $TP_CANVAS_Y"
    tp_capture "$p-pinch-canvas"
    gesture pinch 2 "2 0 0 1.5 0" "290 200"
    tp_capture "$p-pinch-a"
    grep -E '"ev": "pinch_(begin|update|end)"' "$HVX_EVID/a.jsonl" | sed 's/"t_ns": [0-9]*, //' > "$HVX_EVID/$p.a.pinch" || true
    tp_run "$p-focus-a" "move 290 200" "click"
    gesture swipe 3 "3 -10 0" "$TP_CANVAS_X $TP_CANVAS_Y"
    tp_capture "$p-swipe3-left"
    gesture swipe 3 "3 0 10" "$TP_CANVAS_X $TP_CANVAS_Y"
    tp_capture "$p-swipe3-down" b   # the close binding closed a; b drives the ready rule
    gesture swipe 4 "4 -60 0" "$TP_CANVAS_X $TP_CANVAS_Y"
    # The new workspace has no client to drive a ready rule: compare the active workspace instead of a capture.
    hvx_hyprctl_form -j activeworkspace | python3 -I -c "import json, sys; print(json.load(sys.stdin)['id'])" > "$HVX_EVID/$p.ws"
}

# gesture KIND FINGERS UPDATE_ARGS POINTER: pointer to POINTER, begin, 10 updates (pinch: 1), end.
gesture() {
    local kind=$1 f=$2 upd=$3 ptr=$4 T i n=10
    [[ $kind == pinch ]] && n=1
    tp_run "g-$(tp_next_time 1)" "move $ptr"
    T=$(tp_next_time)
    hvx_gesture "$kind" begin "$f" "$T"
    for ((i = 1; i <= n; i++)); do hvx_gesture "$kind" update $upd $((T + 10 * i)); done
    hvx_gesture "$kind" end 0 $((T + 10 * n + 10))
}

tp_setup --stock
scene stock
for w in a b c; do [[ -f $HVX_EVID/$w.jsonl ]] && mv "$HVX_EVID/$w.jsonl" "$HVX_EVID/stock.$w.jsonl"; done
hvx_stop >> "$HVX_EVID/stock.session.log" 2>&1 || hvx_die_blocked "stock session did not stop"

tp_setup --no-drift
scene off
for step in start canvas window pinch-canvas pinch-a swipe3-left swipe3-down; do
    hvx_analyze "off-$step-vs-stock" diff "$HVX_EVID/off-$step.png" --reference "$HVX_EVID/stock-$step.png" --max-pixels 0 \
        || fail "Drift off: capture '$step' differs from stock"
done
cmp -s "$HVX_EVID/off.a.axis" "$HVX_EVID/stock.a.axis" || fail "Drift off: a's scroll events differ from stock"
[[ -s $HVX_EVID/stock.a.axis ]] || hvx_die_blocked "the stock session logged no scroll event for a (comparison vacuous)"
cmp -s "$HVX_EVID/off.a.pinch" "$HVX_EVID/stock.a.pinch" || fail "Drift off: a's pinch events differ from stock"
cmp -s "$HVX_EVID/off.ws" "$HVX_EVID/stock.ws" || fail "Drift off: the 4-finger swipe left a different workspace active than stock"
[[ $(cat "$HVX_EVID/stock.ws") != 1 ]] || hvx_die_blocked "the 4-finger swipe did not switch the workspace in the stock session (comparison vacuous)"
[[ -s $HVX_EVID/stock.a.pinch ]] || hvx_die_blocked "the stock session logged no pinch for a (comparison vacuous)"
hvx_differs stock-close-acted "$HVX_EVID/stock-swipe3-down.png" "$HVX_EVID/stock-swipe3-left.png" \
    || hvx_die_blocked "the 3-finger close binding did not act in the stock session (comparison vacuous)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-OFF Drift off: captures and client scroll events equal to stock (V-09)" \
    "form $HVX_FORM; scrolls, pinches, 3-finger left/down, 4-finger swipe; output $SIZE scale $SCALE"
[[ $result == PASS ]]
