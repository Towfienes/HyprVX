#!/usr/bin/env bash
# T-WS-ANIM (Feature 002 US2, R7, V-15, T036). Workspace 1 holds client a, workspace 2 client d. Workspace animations
# are enabled at runtime (`animations:enabled 1`, every other animation off, `workspaces` 1 s), so only the switch
# itself animates.
#  Control (Drift OFF): `workspace 2`, a capture taken right after the reply (no ready rule) differs from the settled
#   capture: the switch is animated at that capture time (else the check below is vacuous: BLOCKED).
#  Drift (zoom 0.7 at (0,0)): `workspace 2` and back to `workspace 1`, two captures right after each reply equal the
#   settled capture (0 px): no intermediate frame.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SETTLE="${HVX_ANIM_SETTLE:-1.6}"

hvx_init_run "$(hvx_form_label)" t_ws_anim
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
early() { hvx_nested capture "$HVX_EVID/$1.png" || hvx_die_blocked "capture $1 failed"; }
switch_checks() { # NAME WORKSPACE CLIENT
    hvx_dispatch workspace "$2" > "$HVX_EVID/$1.reply" 2>&1 || true
    early "$1-early1"
    early "$1-early2"
    sleep "$SETTLE"
    hvx_capture "$1-settled" "$3"
    for e in early1 early2; do
        hvx_analyze "$1-$e" diff "$HVX_EVID/$1-$e.png" --reference "$HVX_EVID/$1-settled.png" --max-pixels 0 \
            || fail "$1: the $e capture after the switch is an intermediate frame"
    done
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_dispatch workspace 2 > /dev/null 2>&1 || true
hvx_client_start d 300x200 --app-id hyprvx-testclient-b
hvx_dispatch workspace 1 > /dev/null 2>&1 || true
hvx_capture start a
{
    hvx_hyprctl_form keyword animations:enabled 1
    hvx_hyprctl_form keyword animation global,0
    hvx_hyprctl_form keyword animation workspaces,1,10,default
} > "$HVX_EVID/animations.reply" 2>&1 || hvx_die_blocked "could not enable workspace animations"

# Control: Drift OFF, the same switch is animated.
hvx_dispatch workspace 2 > "$HVX_EVID/control.reply" 2>&1 || true
early control-early
sleep "$SETTLE"
hvx_capture control-settled d
hvx_differs control-animated "$HVX_EVID/control-early.png" "$HVX_EVID/control-settled.png" \
    || hvx_die_blocked "control: the switch with Drift OFF was not animated at the capture time (setup invalid)"
hvx_dispatch workspace 1 > /dev/null 2>&1 || true
sleep "$SETTLE"
hvx_capture control-back a

# Drift: no intermediate frame, either direction.
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.7 > "$HVX_EVID/zoom.reply" 2>&1 || true
hvx_capture drift-start a
switch_checks to2 2 d
switch_checks to1 1 a

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-WS-ANIM no intermediate frame on a workspace switch in Drift; control animated with Drift OFF (V-15)" \
    "form $HVX_FORM; workspaces animation 1 s (speed 10), others off; settle ${SETTLE}s; output $SIZE scale $SCALE"
[[ $result == PASS ]]
