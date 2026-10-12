#!/usr/bin/env bash
# T-GLIDE (Feature 003 US4, FR-015, FR-016, FR-017, SC-005, V-13, T032). Scene of f003_lib.sh, Drift on, pointer on
# empty canvas. Each case starts from the identity camera (hyprvx:reset). Release velocities come from the event times
# (contracts/gesture-model.md ReleaseVelocity); the expected total = the scroll/swipe pan + expectedGlide (f003_geom.py).
# After the glide ends (frame count still for 300 ms) every fully visible window is at its expected box (+-2 px), and the
# frame count does not grow over the next 500 ms:
#  1. glide 0.5: 10 frames of (0,10)    -> v (0,-1000) px/s, glide (0,-142.7)
#  2. glide 0.5: 4 frames of (30,0)     -> v (-3000,0),       glide (-428.4,0)
#  3. glide 1.0: 10 frames of (10,0)    -> v (-1000,0),       glide (-499.5,0)
#  4. glide 0:   10 frames of (10,0)    -> no glide
#  5. glide 0.5: 10 frames of (0.5,0)   -> v (-50,0) < glide_min_speed 100: no glide
#  6. glide 0.5: 10 frames of (10,0), stop 60 ms after the last frame (fstop) -> rest > 50 ms: no glide
#  7. glide 0.5: 3-finger swipe, 10 updates of (10,0) -> v (1000,0) (content follows the fingers), glide (142.7,0)
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_glide
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

# judge NAME SCROLL_X SCROLL_Y VX VY STRENGTH: expected camera from the pan plus expectedGlide, after the glide ended.
judge() {
    local name=$1 sx=$2 sy=$3 vx=$4 vy=$5 strength=$6 gx gy f1 f2
    IFS=, read -r gx gy < <(python3 -I "$TP_GEOM" glide --velocity "$vx,$vy" --strength "$strength")
    tp_wait_still "$name" || fail "$name: frames did not stop within 5 s"
    f1=$(hvx_frames HVX-1)
    sleep 0.5
    f2=$(hvx_frames HVX-1)
    echo "$name: pan ($sx,$sy) glide ($gx,$gy); frames after end $f1 -> $f2" >> "$HVX_EVID/glides.txt"
    [[ $f1 == "$f2" ]] || fail "$name: frames kept being produced after the glide ended ($f1 -> $f2)"
    tp_capture "$name"
    tp_scene_ok "$name" "$(python3 -I -c "print(-($sx) - ($gx))")" "$(python3 -I -c "print(-($sy) - ($gy))")" 1 2 \
        || fail "$name: content is not at pan + expected glide"
}
case_start() {
    hvx_dispatch hyprvx:reset > "$HVX_EVID/reset-$1.reply" 2>&1 || true
    tp_glide_setting "$2"
    tp_run "$1-pointer" "move $TP_CANVAS_X $TP_CANVAS_Y"
}

TP_GLIDE=0.5 tp_setup

case_start c1 0.5; T=$(tp_next_time)
tp_run c1 "fscroll $T 10 10 0 10 stop"
judge c1 0 -100 0 -1000 0.5

case_start c2 0.5; T=$(tp_next_time)
tp_run c2 "fscroll $T 10 4 30 0 stop"
judge c2 -120 0 -3000 0 0.5

case_start c3 1.0; T=$(tp_next_time)
tp_run c3 "fscroll $T 10 10 10 0 stop"
judge c3 -100 0 -1000 0 1.0

case_start c4 0; T=$(tp_next_time)
tp_run c4 "fscroll $T 10 10 10 0 stop"
judge c4 -100 0 0 0 0.5

case_start c5 0.5; T=$(tp_next_time)
tp_run c5 "fscroll $T 10 10 0.5 0 stop"
judge c5 -5 0 0 0 0.5

case_start c6 0.5; T=$(tp_next_time)
tp_run c6 "fscroll $T 10 10 10 0" "fstop $((T + 150)) h"
judge c6 -100 0 0 0 0.5

case_start c7 0.5; T=$(tp_next_time)
hvx_gesture swipe begin 3 "$T"
for i in 1 2 3 4 5 6 7 8 9 10; do hvx_gesture swipe update 3 10 0 $((T + 10 * i)); done
hvx_gesture swipe end 0 $((T + 110))
judge c7 100 0 1000 0 0.5

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-GLIDE released pans glide by the closed-form rule; no glide when off, slow or rested; no frames at rest (V-13)" \
    "form $HVX_FORM; 7 cases; output $SIZE scale $SCALE"
[[ $result == PASS ]]
