#!/usr/bin/env bash
# T-TP-SURFACES (Feature 003 FR-003, FR-010, V-08; scroll part T020, 3-finger part T029). Scene of f003_lib.sh with a
# TOP layer client (240x140 at (20,640)), Drift on:
#  1. A finger scroll sequence over the layer surface (10 frames of (4, 5): 20 non-zero axis events) is delivered to
#     the layer client and the camera does not move.
#  2. With a fullscreen (click on a, then the `fullscreen` dispatcher), a finger scroll sequence is delivered to a and
#     the capture does not change.
#  3-finger part (T029): a 3-finger swipe down (bound to close) over the layer, and one while a is fullscreen, are
#  consumed without effect: the capture does not change and `hyprctl -j clients` (class, floating, fullscreen) neither.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_surfaces
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
nonzero_axis() { tp_axis_list "$1" "$2" | grep '"ev": "axis"' | grep -vc '"value": 0.0' || true; }
clients_state() {
    hvx_hyprctl_form -j clients | python3 -I -c "
import json, sys
for c in sorted(json.load(sys.stdin), key=lambda c: c['class']):
    print(c['class'], c['floating'], c['fullscreen'])" > "$HVX_EVID/$1.state"
}
swipe3_down() {
    local T i
    T=$(tp_next_time)
    hvx_gesture swipe begin 3 "$T"
    for i in 1 2 3 4 5 6 7 8 9 10; do hvx_gesture swipe update 3 0 10 $((T + 10 * i)); done
    hvx_gesture swipe end 0 $((T + 110))
}
tp_setup --layer

# 1. layer surface
tp_run over-layer "move 100 700"
tp_capture before-layer
T=$(tp_next_time)
tp_run layer-scroll "fscroll $T 10 10 4 5 stop"
tp_capture after-layer
hvx_analyze layer-unchanged diff "$HVX_EVID/after-layer.png" --reference "$HVX_EVID/before-layer.png" --max-pixels 0 \
    || fail "a finger scroll over the layer surface moved the camera"
n=$(nonzero_axis layer-scroll layer)
echo "layer: non-zero axis events $n" >> "$HVX_EVID/counts.txt"
((n == 20)) || fail "the layer client did not receive the finger scroll ($n axis events)"
# 3-finger swipe down over the layer: ignored (a is focused by the click below only later; c may be focused here,
# so any focused window would be closed if the swipe passed through)
clients_state layer3-before
swipe3_down
tp_capture after-layer3
hvx_analyze layer3-unchanged diff "$HVX_EVID/after-layer3.png" --reference "$HVX_EVID/after-layer.png" --max-pixels 0 \
    || fail "a 3-finger swipe over the layer changed the capture"
clients_state layer3-after
cmp -s "$HVX_EVID/layer3-before.state" "$HVX_EVID/layer3-after.state" || fail "a 3-finger swipe over the layer changed a window"

# 2. fullscreen window
tp_run focus-a "move 290 200" "click"
hvx_dispatch fullscreen > "$HVX_EVID/fullscreen.reply" 2>&1 || true
# While a is fullscreen the other clients are not drawn (no frame_done), so the pointer driver syncs on a alone.
TP_CLIENTS=(a)
tp_run fs-pointer "move $TP_CANVAS_X $TP_CANVAS_Y"
tp_capture before-fs
T=$(tp_next_time)
tp_run fs-scroll "fscroll $T 10 10 4 5 stop"
tp_capture after-fs
hvx_analyze fs-unchanged diff "$HVX_EVID/after-fs.png" --reference "$HVX_EVID/before-fs.png" --max-pixels 0 \
    || fail "a finger scroll with a fullscreen window changed the capture"
n=$(nonzero_axis fs-scroll a)
echo "fullscreen a: non-zero axis events $n" >> "$HVX_EVID/counts.txt"
((n == 20)) || fail "the fullscreen window did not receive the finger scroll ($n axis events)"
clients_state fs3-before
swipe3_down
tp_capture after-fs3
hvx_analyze fs3-unchanged diff "$HVX_EVID/after-fs3.png" --reference "$HVX_EVID/after-fs.png" --max-pixels 0 \
    || fail "a 3-finger swipe with a fullscreen window changed the capture"
clients_state fs3-after
cmp -s "$HVX_EVID/fs3-before.state" "$HVX_EVID/fs3-after.state" || fail "a 3-finger swipe with a fullscreen window changed a window"
hvx_dispatch fullscreen > "$HVX_EVID/fullscreen-off.reply" 2>&1 || true
TP_CLIENTS=(a b c layer)


hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-SURFACES layer and fullscreen get stock delivery; nothing moves (V-08)" \
    "form $HVX_FORM; layer TOP (20,640); fullscreen a; output $SIZE scale $SCALE"
[[ $result == PASS ]]
