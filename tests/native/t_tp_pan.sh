#!/usr/bin/env bash
# T-TP-PAN (Feature 003 US1, FR-004, SC-001, V-04, T016). Scene of f003_lib.sh (a, b, c floating), Drift on.
# Finger-source scroll sequences (vpointer fscroll, with stops) that begin on empty canvas pan the camera by
# -(sum of deltas) * f, f = input:touchpad:scroll_factor (1.0 in the test config) * plugin:hyprvx:pan_factor (1.0):
#  1. z = 1: 10 frames of (4, 5): every fully visible window moves by (-40, -50) +-1 px.
#  2. z = 0.5 (hyprvx:reset, pointer at (0,0), hyprvx:zoom 0.5): 10 frames of (-4, -5): windows move by (40, 50) +-1 px.
# In both, no client logs any pointer event (the canvas sequence is consumed).
# Setup validity: the scene is at its identity boxes before step 1 and at the zoomed boxes before step 2, else BLOCKED.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_pan
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
tp_setup

tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"

# 1. z = 1
T=$(tp_next_time)
tp_run pan1 "fscroll $T 10 10 4 5 stop"
read -r PX PY Z < <(python3 -I "$TP_GEOM" pan --camera 0 0 1 --delta "$(python3 -I "$TP_GEOM" scroll-delta --dx 4 --dy 5 --n 10)")
echo "pan1 camera $PX $PY $Z" >> "$HVX_EVID/cameras.txt"
tp_capture pan1
tp_scene_ok pan1 "$PX" "$PY" "$Z" 1 || fail "z 1: windows did not move by (-40,-50)"
(($(tp_events pan1) == 0)) || fail "z 1: a client received pointer events from a canvas finger scroll"

# 2. z = 0.5
hvx_dispatch hyprvx:reset > "$HVX_EVID/reset.reply" 2>&1 || true
tp_run origin "move 0 0"
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true
tp_run back "move $TP_CANVAS_X $TP_CANVAS_Y"
tp_capture zoomed
tp_scene_ok zoomed 0 0 0.5 2 || hvx_die_blocked "scene not at the zoom 0.5 boxes"
T=$(tp_next_time)
tp_run pan2 "fscroll $T 10 10 -4 -5 stop"
read -r PX PY Z < <(python3 -I "$TP_GEOM" pan --camera 0 0 0.5 --delta "$(python3 -I "$TP_GEOM" scroll-delta --dx -4 --dy -5 --n 10)")
echo "pan2 camera $PX $PY $Z" >> "$HVX_EVID/cameras.txt"
tp_capture pan2
tp_scene_ok pan2 "$PX" "$PY" "$Z" 1 || fail "z 0.5: windows did not move by (40,50)"
(($(tp_events pan2) == 0)) || fail "z 0.5: a client received pointer events from a canvas finger scroll"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-PAN finger scroll on canvas pans by -(sum)*f +-1 px; no client event (V-04)" \
    "form $HVX_FORM; z 1: 10x(4,5); z 0.5: 10x(-4,-5); output $SIZE scale $SCALE"
[[ $result == PASS ]]
