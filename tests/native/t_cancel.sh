#!/usr/bin/env bash
# T-CANCEL (Feature 003 FR-012, V-11; pinch part T025, 3-finger swipe part T029). Scene of f003_lib.sh, Drift on.
#  Pinch: on empty canvas, begin and update to scale 0.5 (the capture mid-gesture must differ from the one before, else
#  the check is vacuous: BLOCKED), then end with cancelled = 1: the capture equals the one before the gesture (0 px) and
#  the frame count of HVX-1 does not grow over 500 ms afterwards (no glide, no camera motion).
#  3-finger swipe (T029): the same with one update of (30, 0) (a pan) ended with cancelled = 1.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_cancel
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
tp_setup
tp_run anchor "move 480 300"

# cancelled KIND NAME BEGIN UPDATE: one gesture ended with cancelled = 1; judged against the capture before it.
cancelled() {
    local kind=$1 name=$2 begin=$3 update=$4 T f1 f2
    tp_capture "$name-before"
    T=$(tp_next_time)
    hvx_gesture "$kind" begin $begin "$T"
    hvx_gesture "$kind" update $update $((T + 10))
    tp_capture "$name-mid"
    hvx_differs "$name-moved" "$HVX_EVID/$name-mid.png" "$HVX_EVID/$name-before.png" \
        || hvx_die_blocked "$name: the gesture did not change the camera before the cancel (check vacuous)"
    hvx_gesture "$kind" end 1 $((T + 20))
    tp_capture "$name-after"
    hvx_analyze "$name-restored" diff "$HVX_EVID/$name-after.png" --reference "$HVX_EVID/$name-before.png" --max-pixels 0 \
        || fail "$name: the cancelled gesture did not restore the camera"
    f1=$(hvx_frames HVX-1)
    sleep 0.5
    f2=$(hvx_frames HVX-1)
    echo "$name frames after cancel: $f1 -> $f2" >> "$HVX_EVID/frames.txt"
    [[ $f1 == "$f2" ]] || fail "$name: frames kept being produced after the cancel ($f1 -> $f2)"
}

cancelled pinch pinch "2" "2 0 0 0.5 0"
cancelled swipe swipe3 "3" "3 30 0"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-CANCEL a cancelled gesture restores the camera at begin; no motion afterwards (V-11)" \
    "form $HVX_FORM; pinch 0.5 and 3-finger swipe (30,0) cancelled; output $SIZE scale $SCALE"
[[ $result == PASS ]]
