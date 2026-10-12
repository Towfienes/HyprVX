#!/usr/bin/env bash
# T-PINCH (Feature 003 US2, FR-008, FR-009, SC-002, V-10, T024). Scene of f003_lib.sh, Drift on. Synthetic 2-finger
# pinches (hvxtest: begin, one update with the final scale, end) zoom absolutely from the camera at begin, anchored at
# the pointer at begin (contracts/gesture-model.md pinchCamera, limits [0.1, 2.0]). Each case starts from the identity
# camera (hyprvx:reset); expected cameras come from f003_geom.py and are judged on every fully visible window (+-2 px;
# the anchor invariant follows from the boxes; a case where no window is fully visible fails as not judged):
#  1. anchor (480,300) on empty canvas, scale 0.5                    -> z 0.5 (a, b, c judged)
#  2. same anchor, scale 2                                            -> z 2 (c judged)
#  3. same anchor, scale 0.01 (clamped to 0.1), then 10               -> identity again (z 0.1 without the clamp)
#  4. same anchor, scale 50 (clamped to 2), then 0.25                 -> z 0.5 (12.5 -> 2 without the clamp)
#  5. Super + pinch with the pointer over a (290,200), scale 0.8      -> zoomed; a logs no pinch event
#  6. pinch over a without Super, scale 1.5: a logs pinch begin/update/end; the capture does not change
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_pinch
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
pinch_count() { grep -c "\"ev\": \"$1\"" "$HVX_EVID/a.jsonl" || true; }
PX=0 PY=0 Z=1

# pinch NAME X Y SCALE: pointer to (X,Y), one pinch, expected camera updated from (PX, PY, Z).
pinch() {
    local name=$1 x=$2 y=$3 s=$4 T
    tp_run "$name-pointer" "move $x $y"
    T=$(tp_next_time)
    hvx_gesture pinch begin 2 "$T"
    hvx_gesture pinch update 2 0 0 "$s" 0 $((T + 10))
    hvx_gesture pinch end 0 $((T + 20))
    read -r PX PY Z < <(python3 -I "$TP_GEOM" pinch --camera "$PX" "$PY" "$Z" --anchor "$x,$y" --scale "$s")
    echo "$name anchor ($x,$y) scale $s -> camera $PX $PY $Z" >> "$HVX_EVID/cameras.txt"
}

tp_setup
tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"

ANCHOR_X=480 ANCHOR_Y=300
reset_camera() {
    hvx_dispatch hyprvx:reset > "$HVX_EVID/reset-$1.reply" 2>&1 || true
    PX=0 PY=0 Z=1
}

reset_camera c1
pinch c1 "$ANCHOR_X" "$ANCHOR_Y" 0.5
tp_capture c1
tp_scene_ok c1 "$PX" "$PY" "$Z" 2 || fail "1: pinch 0.5"

reset_camera c2
pinch c2 "$ANCHOR_X" "$ANCHOR_Y" 2
tp_capture c2
tp_scene_ok c2 "$PX" "$PY" "$Z" 2 || fail "2: pinch 2"

reset_camera c3
pinch c3a "$ANCHOR_X" "$ANCHOR_Y" 0.01
pinch c3b "$ANCHOR_X" "$ANCHOR_Y" 10
tp_capture c3
tp_scene_ok c3 "$PX" "$PY" "$Z" 2 || fail "3: pinch 0.01 (clamped to 0.1) then 10"

reset_camera c4
pinch c4a "$ANCHOR_X" "$ANCHOR_Y" 50
pinch c4b "$ANCHOR_X" "$ANCHOR_Y" 0.25
tp_capture c4
tp_scene_ok c4 "$PX" "$PY" "$Z" 2 || fail "4: pinch 50 (clamped to 2) then 0.25"

# 5. Super + pinch over a
reset_camera c5
b0=$(pinch_count pinch_begin)
hvx_key kb down KEY_LEFTMETA
pinch c5 290 200 0.8
hvx_key kb up KEY_LEFTMETA
tp_capture c5
tp_scene_ok c5 "$PX" "$PY" "$Z" 2 || fail "5: Super + pinch over a did not zoom as expected"
(($(pinch_count pinch_begin) == b0)) || fail "5: a received a pinch although Super was held"

# 6. pinch over a without Super: delivered to a, camera unchanged
reset_camera c6
AX=290 AY=200
tp_run p6-pointer "move $AX $AY"
tp_capture before-p6
b0=$(pinch_count pinch_begin) u0=$(pinch_count pinch_update) e0=$(pinch_count pinch_end)
T=$(tp_next_time)
hvx_gesture pinch begin 2 "$T"
hvx_gesture pinch update 2 0 0 1.5 0 $((T + 10))
hvx_gesture pinch end 0 $((T + 20))
tp_capture after-p6
hvx_analyze p6-unchanged diff "$HVX_EVID/after-p6.png" --reference "$HVX_EVID/before-p6.png" --max-pixels 0 \
    || fail "6: a pinch over a without Super changed the camera"
echo "a pinch events: begin +$(($(pinch_count pinch_begin) - b0)) update +$(($(pinch_count pinch_update) - u0)) end +$(($(pinch_count pinch_end) - e0))" \
    >> "$HVX_EVID/counts.txt"
(($(pinch_count pinch_begin) == b0 + 1 && $(pinch_count pinch_update) == u0 + 1 && $(pinch_count pinch_end) == e0 + 1)) \
    || fail "6: a did not receive the pinch (begin/update/end)"
grep '"ev": "pinch_update"' "$HVX_EVID/a.jsonl" | tail -1 | grep -q '"scale": 1.5000' || fail "6: a's pinch scale is not 1.5"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-PINCH pinch zooms from the begin camera at the begin pointer, clamped; window pinches pass through (V-10)" \
    "form $HVX_FORM; scales 0.5, 2, 0.01+10, 50+0.25, Super 0.8, window 1.5; output $SIZE scale $SCALE"
[[ $result == PASS ]]
