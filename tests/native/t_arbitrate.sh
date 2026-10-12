#!/usr/bin/env bash
# T-ARBITRATE (Feature 003 US3, FR-007, FR-010, FR-011, SC-003, V-12, T028). Scene of f003_lib.sh with a TOP layer
# client; the test config binds `gesture = 3, down, close`, `3, up, fullscreen`, `3, left, float`, `4, horizontal,
# workspace` (test-tooling 1.5 section 5). a is focused by a click first, so every bound action would act on a.
#  Drift on:
#   1. 3-finger swipe down, 10 updates of (0,10) on canvas: content moves down by 100 (+-1 px, a and c judged).
#   2. 24 3-finger swipes in all four directions (symmetric, net zero; some over a, some over the layer, which are
#      ignored) and two 3-finger pinches (0.8 then 1.25 at one anchor): no window closes, none changes floating or
#      fullscreen state (`hyprctl -j clients`), no fullscreen/close/float/special event (socket2), and the final capture
#      equals the start capture (the camera returned exactly).
#   3. A 4-finger horizontal swipe switches the workspace and the opposite one comes back; workspace 1 keeps its camera
#      (capture equals the one before the switch).
#  Control, Drift off (else the checks above are vacuous: BLOCKED): 3 up makes a fullscreen, 3 left changes a's floating
#  state, 3 down closes the focused window, and the 4-finger swipe switches the workspace.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_arbitrate
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

# swipe FINGERS DX DY: one swipe of 10 updates of (DX, DY), 10 ms apart.
swipe() {
    local f=$1 dx=$2 dy=$3 T i
    T=$(tp_next_time)
    hvx_gesture swipe begin "$f" "$T"
    for i in 1 2 3 4 5 6 7 8 9 10; do
        hvx_gesture swipe update "$f" "$dx" "$dy" $((T + 10 * i))
    done
    hvx_gesture swipe end 0 $((T + 110))
}
pinch3() {
    local T
    T=$(tp_next_time)
    hvx_gesture pinch begin 3 "$T"
    hvx_gesture pinch update 3 0 0 "$1" 0 $((T + 10))
    hvx_gesture pinch end 0 $((T + 20))
}
clients_state() { # NAME: class, floating, fullscreen of every client, sorted
    hvx_hyprctl_form -j clients > "$HVX_EVID/$1.clients.json"
    python3 -I -c "
import json, sys
for c in sorted(json.load(open(sys.argv[1])), key=lambda c: c['class']):
    print(c['class'], c['floating'], c['fullscreen'])" "$HVX_EVID/$1.clients.json" > "$HVX_EVID/$1.state"
}
active_ws() { hvx_hyprctl_form -j activeworkspace | python3 -I -c "import json, sys; print(json.load(sys.stdin)['id'])"; }

tp_setup --layer
hvx_events_start
tp_run focus-a "move 290 200" "click" "move 480 300"
tp_capture start
tp_scene_ok start 0 0 1 2 || hvx_die_blocked "scene not at its identity boxes"
clients_state before

# 1. one swipe down: content follows the fingers
swipe 3 0 10
tp_capture down1
tp_scene_ok down1 0 -100 1 1 || fail "1: a 3-finger swipe down did not pan the content down by 100"
swipe 3 0 -10

# 2. 24 swipes and 2 pinches, symmetric
for round in 1 2 3; do
    for pos in "480 300" "290 200" "100 700"; do  # canvas, over a, over the layer (ignored)
        tp_run "pos-$round-${pos// /_}" "move $pos"
        swipe 3 0 10; swipe 3 0 -10; swipe 3 -10 0; swipe 3 10 0
    done
done
tp_run anchor "move 480 300"
pinch3 0.8
pinch3 1.25
tp_capture end
hvx_analyze end-vs-start diff "$HVX_EVID/end.png" --reference "$HVX_EVID/start.png" --max-pixels 0 \
    || fail "2: the camera did not return exactly after the symmetric 3-finger gestures"
clients_state after
cmp -s "$HVX_EVID/before.state" "$HVX_EVID/after.state" || fail "2: a 3-finger gesture in Drift changed a window (close, float or fullscreen)"
# The socket2 logger writes asynchronously: give any action event of the Drift phase time to arrive before cutting.
sleep 1
drift_lines=$(wc -l < "$HVX_EVID/events.log")
head -n "$drift_lines" "$HVX_EVID/events.log" | grep -E '^(fullscreen|closewindow|changefloatingmode|activespecial|activespecialv2)>>' \
    > "$HVX_EVID/drift-actions.log" || true
[[ ! -s $HVX_EVID/drift-actions.log ]] || fail "2: a 3-finger gesture in Drift triggered a compositor action (drift-actions.log)"

# 3. 4-finger horizontal swipe: workspace switch, camera per workspace kept
hvx_dispatch hyprvx:pan 100 0 > "$HVX_EVID/pan.reply" 2>&1 || true
tp_capture ws1-before
dir=-60
swipe 4 $dir 0
if [[ $(active_ws) == 1 ]]; then dir=60; swipe 4 $dir 0; fi
ws_after=$(active_ws)
echo "4-finger direction $dir: workspace 1 -> $ws_after" >> "$HVX_EVID/workspace.txt"
[[ $ws_after != 1 ]] || fail "3: the 4-finger swipe did not switch the workspace in Drift"
swipe 4 $((-dir)) 0
[[ $(active_ws) == 1 ]] || fail "3: the opposite 4-finger swipe did not return to workspace 1"
tp_capture ws1-after
hvx_analyze ws1-camera-kept diff "$HVX_EVID/ws1-after.png" --reference "$HVX_EVID/ws1-before.png" --max-pixels 0 \
    || fail "3: workspace 1 did not keep its camera across the 4-finger switch"

# Control: Drift off, the same bindings act (judged on socket2 events after this point, and on the window count).
hvx_dispatch hyprvx:off > "$HVX_EVID/off.reply" 2>&1 || true
tp_run focus-a2 "move 290 200" "click" "move 480 300"
ctl_from=$(wc -l < "$HVX_EVID/events.log")
ctl_events() { tail -n +"$((ctl_from + 1))" "$HVX_EVID/events.log" | grep -c "^$1>>" || true; }
# ctl_seen EV: the socket2 logger writes asynchronously, so wait up to 3 s for the event after the control gesture.
ctl_seen() {
    local i
    for i in $(seq 1 30); do
        (($(ctl_events "$1") >= 1)) && return 0
        sleep 0.1
    done
    return 1
}
swipe 3 0 -10
ctl_seen fullscreen || hvx_die_blocked "control: 3 up made no fullscreen event with Drift off (check vacuous)"
hvx_dispatch fullscreen > "$HVX_EVID/ctl-fs-off.reply" 2>&1 || true
swipe 3 -10 0
ctl_seen changefloatingmode || hvx_die_blocked "control: 3 left made no floating change with Drift off (check vacuous)"
swipe 3 0 10
clients_state ctl-down
(($(wc -l < "$HVX_EVID/ctl-down.state") == $(wc -l < "$HVX_EVID/before.state") - 1)) \
    || hvx_die_blocked "control: 3 down did not close the focused window with Drift off (check vacuous)"
swipe 4 $dir 0
[[ $(active_ws) != 1 ]] || hvx_die_blocked "control: the 4-finger swipe did not switch the workspace with Drift off"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-ARBITRATE 3-finger gestures never trigger bindings in Drift; 4-finger keeps workspaces (V-12)" \
    "form $HVX_FORM; 26 3-finger gestures in Drift; 4-finger dir $dir; control with Drift off; output $SIZE scale $SCALE"
[[ $result == PASS ]]
