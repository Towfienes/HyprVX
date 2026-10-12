#!/usr/bin/env bash
# T-TP-SUPER (Feature 003 US1, FR-005, V-06, T018). Scene of f003_lib.sh, Drift on, identity camera, pointer at the
# centre of a. With Super held (hyprvx-vkeyboard KEY_LEFTMETA), a finger scroll sequence over a (10 frames of (4, 5),
# stop) pans the camera by (-40, -50) +-1 px, and a logs no scroll event.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_super
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
tp_setup
tp_run over-a "move 290 200"
tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"

hvx_key kb down KEY_LEFTMETA
T=$(tp_next_time)
tp_run super "fscroll $T 10 10 4 5 stop"
hvx_key kb up KEY_LEFTMETA
read -r PX PY Z < <(python3 -I "$TP_GEOM" pan --camera 0 0 1 --delta "$(python3 -I "$TP_GEOM" scroll-delta --dx 4 --dy 5 --n 10)")
tp_capture panned
tp_scene_ok panned "$PX" "$PY" "$Z" 1 || fail "Super + finger scroll over a did not pan by (-40,-50)"
tp_axis_list super a > "$HVX_EVID/super.a.axis"
[[ ! -s $HVX_EVID/super.a.axis ]] || fail "a received scroll events although Super was held"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-SUPER Super + finger scroll over a window pans; the window gets nothing (V-06)" \
    "form $HVX_FORM; 10x(4,5) over a with Super; output $SIZE scale $SCALE"
[[ $result == PASS ]]
