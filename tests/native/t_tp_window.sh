#!/usr/bin/env bash
# T-TP-WINDOW (Feature 003 US1, FR-006, SC-004, V-05, T017). Scene of f003_lib.sh, Drift on, identity camera, pointer at
# the centre of a. A finger scroll sequence (10 frames of (0, 5), stop) over a without Super:
#  - a logs 10 non-zero vertical axis events of value 5 (the stop arrives as value 0 + axis_stop), finger axis
#    sources and a vertical axis stop; b and c log nothing;
#  - the capture is byte-identical to the one before (the camera did not move);
#  - the same sequence after hyprvx:off gives a the same scroll events (same values as in Normal Mode).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_window
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
tp_setup
tp_run over-a "move 290 200"
tp_capture before

T=$(tp_next_time)
tp_run drift "fscroll $T 10 10 0 5 stop"
tp_capture after
hvx_analyze unchanged diff "$HVX_EVID/after.png" --reference "$HVX_EVID/before.png" --max-pixels 0 \
    || fail "a finger scroll over a window moved the camera"
tp_axis_list drift a > "$HVX_EVID/drift.a.axis"
# Hyprland forwards the stop as an axis value 0 followed by axis_stop, so only non-zero axis events are counted.
n_axis=$(grep '"ev": "axis"' "$HVX_EVID/drift.a.axis" | grep -vc '"value": 0.0' || true)
n_five=$(grep -c '"value": 5.0' "$HVX_EVID/drift.a.axis" || true)
n_finger=$(grep -c '"source": "finger"' "$HVX_EVID/drift.a.axis" || true)
n_stop=$(grep -c '"ev": "axis_stop"' "$HVX_EVID/drift.a.axis" || true)
echo "a: axis $n_axis value5 $n_five finger-source $n_finger stops $n_stop" >> "$HVX_EVID/counts.txt"
((n_axis == 10 && n_five == 10)) || fail "a did not get 10 axis events of value 5 ($n_axis, $n_five)"
((n_finger >= 1)) || fail "a got no finger axis source"
((n_stop >= 1)) || fail "a got no axis stop"
(($(tp_events drift b c) == 0)) || fail "b or c received pointer events"

hvx_dispatch hyprvx:off > "$HVX_EVID/off.reply" 2>&1 || true
T=$(tp_next_time)
tp_run normal "fscroll $T 10 10 0 5 stop"
tp_axis_list normal a > "$HVX_EVID/normal.a.axis"
cmp -s "$HVX_EVID/drift.a.axis" "$HVX_EVID/normal.a.axis" || fail "a's scroll events in Drift differ from Normal Mode"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-WINDOW finger scroll over a window reaches it unchanged; camera still (V-05)" \
    "form $HVX_FORM; 10x(0,5) over a; output $SIZE scale $SCALE"
[[ $result == PASS ]]
