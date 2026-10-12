#!/usr/bin/env bash
# T-TP-DRAG (Feature 003 FR-014, V-16, T035). Scene of f003_lib.sh, Drift on. Trackpad input during a Feature 002
# Super + left drag changes neither the camera nor the dragged window; a drag that begins during a finger scroll
# sequence ends that sequence's camera effect:
#  1. Super + drag on canvas by (-100, 0) with a finger scroll, a 2-finger pinch and a 3-finger swipe in the middle: the
#     content moves by (-100, 0) only (+-1 px).
#  2. Super + drag on a by (100, 0) with the same trackpad input in the middle: a moves by (100, 0), c stays (+-1).
#  3. A finger scroll begins on canvas (5 frames of (0,5): pan (0,-25)); a Super + drag by (-50, 0) starts; the scroll
#     continues with 5 more frames and its stop: the content moves by (-50, -25) in total (+-1 px).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_drag
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

trackpad_noise() {
    local T i
    T=$(tp_next_time)
    tp_run "noise-$T" "fscroll $T 10 5 0 10 stop"
    T=$(tp_next_time)
    hvx_gesture pinch begin 2 "$T"; hvx_gesture pinch update 2 0 0 0.5 0 $((T + 10)); hvx_gesture pinch end 0 $((T + 20))
    T=$(tp_next_time)
    hvx_gesture swipe begin 3 "$T"
    for i in 1 2 3 4 5; do hvx_gesture swipe update 3 20 20 $((T + 10 * i)); done
    hvx_gesture swipe end 0 $((T + 60))
}

tp_setup
tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"

# 1. pan drag
hvx_key kb down KEY_LEFTMETA
tp_run pan-a "move 480 300" "down" "move 430 300"
trackpad_noise
tp_run pan-b "move 380 300" "up"
hvx_key kb up KEY_LEFTMETA
tp_capture pan
tp_scene_ok pan 100 0 1 1 || fail "1: trackpad input changed the camera during a pan drag"

# 2. move drag on a
hvx_dispatch hyprvx:reset > "$HVX_EVID/reset2.reply" 2>&1 || true
hvx_key kb down KEY_LEFTMETA
tp_run move-a "move 290 200" "down" "move 340 200"
trackpad_noise
tp_run move-b "move 390 200" "up"
hvx_key kb up KEY_LEFTMETA
tp_capture move
tp_box move a "240,100,300,200" 1 || fail "2: a did not move by exactly (100,0) during the move drag"
tp_box move c "520,320,300,200" 1 || fail "2: c moved during a's move drag"

# 3. drag begins during a scroll sequence (a's offset from step 2 stays; judge c and b)
hvx_dispatch hyprvx:reset > "$HVX_EVID/reset3.reply" 2>&1 || true
T=$(tp_next_time)
tp_run s3a "move 480 300" "fscroll $T 10 5 0 5"
hvx_key kb down KEY_LEFTMETA
tp_run s3b "down" "move 430 300"
tp_run s3c "fscroll $((T + 50)) 10 5 0 5 stop"
tp_run s3d "up"
hvx_key kb up KEY_LEFTMETA
tp_capture s3
tp_box s3 c "470,295,300,200" 1 || fail "3: the scroll kept panning after the drag began (c)"
tp_box s3 b "910,535,300,200" 1 || fail "3: the scroll kept panning after the drag began (b)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-DRAG trackpad input never changes a Super + drag; a drag ends a scroll's camera effect (V-16)" \
    "form $HVX_FORM; pan drag, move drag, drag during scroll; output $SIZE scale $SCALE"
[[ $result == PASS ]]
