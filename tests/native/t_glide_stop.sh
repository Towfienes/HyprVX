#!/usr/bin/env bash
# T-GLIDE-STOP (Feature 003 US4, FR-013, FR-016, SC-005, V-14, T033). Scene of f003_lib.sh, Drift on, glide 0.5. Each
# case starts a glide (10 frames of (0,10) on canvas: v 1000 px/s, about 1 s of glide) and interrupts it at once:
#   a slow finger scroll (below glide_min_speed, so it starts no glide of its own), a click on canvas, a key press,
#   hyprvx:off, hyprvx:panic, a workspace switch, and finally removal of the output.
# Setup validity: right after the start the frame count must still be growing (the glide is running), else BLOCKED.
# After each interrupt the frame count of HVX-1 stops growing within 1 s and two captures 300 ms apart are identical
# (the workspace case takes them after returning to workspace 1, where the scene clients are visible).
# After the output removal the compositor still answers (hyprctl) and Drift is off once the output is re-added.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_glide_stop
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

start_glide() { # NAME: reset, Drift on, glide started and seen running
    hvx_dispatch hyprvx:on > "$HVX_EVID/on-$1.reply" 2>&1 || true
    hvx_dispatch hyprvx:reset > "$HVX_EVID/reset-$1.reply" 2>&1 || true
    tp_run "$1-pointer" "move $TP_CANVAS_X $TP_CANVAS_Y"
    T=$(tp_next_time)
    tp_run "$1-start" "fscroll $T 10 10 0 10 stop"
    local f0 f1
    f0=$(hvx_frames HVX-1)
    sleep 0.05
    f1=$(hvx_frames HVX-1)
    ((f1 > f0)) || hvx_die_blocked "$1: no glide running after the release (check vacuous)"
}
stopped() { # NAME [back]: frames stop and the picture is still; back = return to workspace 1 before the captures
    tp_wait_still "$1-still" 1 || { fail "$1: frames kept being produced after the interrupt"; return; }
    # The scene clients live on workspace 1; on another workspace they are hidden and get no frame callbacks, so the
    # captures (ready rule) are taken after returning. The glide must not resume there.
    [[ ${2:-} == back ]] && hvx_dispatch workspace 1 > "$HVX_EVID/$1-back.reply" 2>&1
    tp_capture "$1-a"
    sleep 0.3
    tp_capture "$1-b"
    hvx_analyze "$1-still" diff "$HVX_EVID/$1-b.png" --reference "$HVX_EVID/$1-a.png" --max-pixels 0 \
        || fail "$1: the content still moved after the interrupt"
}

TP_GLIDE=0.5 tp_setup

start_glide scroll
T=$(tp_next_time)
tp_run scroll-int "fscroll $T 10 3 0 0.2 stop"
stopped scroll

start_glide click
tp_run click-int "click"
stopped click

start_glide key
hvx_key kb tap KEY_A
stopped key

start_glide off
hvx_dispatch hyprvx:off > "$HVX_EVID/off-int.reply" 2>&1 || true
stopped off

start_glide panic
hvx_dispatch hyprvx:panic > "$HVX_EVID/panic-int.reply" 2>&1 || true
stopped panic

start_glide workspace
hvx_dispatch workspace 2 > "$HVX_EVID/ws2.reply" 2>&1 || true
stopped workspace back

start_glide output
hvx_hyprctl_form output remove HVX-1 > "$HVX_EVID/output-remove.reply" 2>&1 || true
hvx_hyprctl_form version > "$HVX_EVID/alive.txt" 2>&1 || fail "output removal during a glide: the compositor does not answer"
hvx_hyprctl_form output create headless HVX-1 > "$HVX_EVID/output-readd.reply" 2>&1 || true
tp_run focus-a "move 290 200" "click"
hvx_drift_off_probe kb a || fail "output removal during a glide: Drift is not off after the output came back"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-GLIDE-STOP any input or mode change stops a glide at once (V-14)" \
    "form $HVX_FORM; scroll, click, key, off, panic, workspace, output removal; output $SIZE scale $SCALE"
[[ $result == PASS ]]
