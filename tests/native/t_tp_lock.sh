#!/usr/bin/env bash
# T-TP-LOCK (Feature 003 US1, FR-002, V-07, T019). Scene of f003_lib.sh, Drift on. The target of a finger scroll
# sequence is fixed when it begins:
#  A. Begun on empty canvas: 5 frames of (0, 5), the pointer moves onto a (its drawn centre after that half pan), then 5
#     more frames and the stop, all inside one sequence (event times 10 ms apart): the camera pans by (0, -50) +-1 px in
#     total and a logs no scroll event.
#  B. After hyprvx:reset, begun over a: 5 frames, the pointer moves to empty canvas, 5 more frames and the stop: the
#     camera does not move (capture byte-identical to the one before) and a logged the first 5 frames.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_lock
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
tp_setup
tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"

# A. canvas first. After the first half the content moved up by 25, so a's centre is at (290, 175).
T=$(tp_next_time)
tp_run lock-a "fscroll $T 10 5 0 5" "move 290 175" "fscroll $((T + 50)) 10 5 0 5 stop"
tp_capture after-a
tp_scene_ok after-a 0 50 1 1 || fail "A: the canvas sequence did not keep panning to (0,-50) after the pointer reached a"
tp_axis_list lock-a a > "$HVX_EVID/lock-a.a.axis"
[[ ! -s $HVX_EVID/lock-a.a.axis ]] || fail "A: a received scroll events from a sequence that began on the canvas"

# B. window first.
hvx_dispatch hyprvx:reset > "$HVX_EVID/reset.reply" 2>&1 || true
tp_run over-a "move 290 200"
tp_capture before-b
T=$(tp_next_time)
tp_run lock-b "fscroll $T 10 5 0 5" "move $TP_CANVAS_X $TP_CANVAS_Y" "fscroll $((T + 50)) 10 5 0 5 stop"
tp_capture after-b
hvx_analyze b-unchanged diff "$HVX_EVID/after-b.png" --reference "$HVX_EVID/before-b.png" --max-pixels 0 \
    || fail "B: a sequence that began over a moved the camera"
n=$(tp_axis_list lock-b a | grep '"ev": "axis"' | grep -vc '"value": 0.0' || true)
echo "B: a non-zero axis events $n" >> "$HVX_EVID/counts.txt"
((n >= 5)) || fail "B: a did not receive the first half of its sequence ($n axis events)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-LOCK the target of a finger scroll sequence is fixed at its start (V-07)" \
    "form $HVX_FORM; A canvas->a 2x5x(0,5); B a->canvas; output $SIZE scale $SCALE"
[[ $result == PASS ]]
