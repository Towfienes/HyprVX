#!/usr/bin/env bash
# T-DRAG-ABORT (Feature 002 data-model Drag end conditions, V-21, T047). Floating clients a and b; Drift on (identity
# camera). For each abort, a Super + left drag is started and, with the button still held, interrupted:
#   panic       `hyprvx:panic` during a move drag of a
#   off         `hyprvx:off` during a move drag of a
#   workspace   `workspace 2` during a move drag of a (then back to 1)
#   pan-panic   `hyprvx:panic` during a pan drag on empty canvas
#   quit        client a quits during its move drag
# then the pointer moves on and the button is released. Afterwards the compositor is alive (hyprctl answers) and there
# is no stuck grab: a plain click on b reaches b (its log gains a button press).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_drag_abort
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
presses() { grep -c '"ev": "button", "surface": "main", "button": 272, "state": "pressed"' "$HVX_EVID/$1.jsonl" || true; }
run() { # NAME COMMANDS
    printf "$2" > "$HVX_EVID/$1.cmds"
    hvx_pointer_run "$1.steps" vp "$HVX_EVID/$1.cmds" a b
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
# c2 on workspace 2: the frame barrier of the pointer driver while workspace 2 is shown (a and b are hidden then).
hvx_dispatch workspace 2 > /dev/null 2>&1 || true
hvx_client_start c2 300x200 --app-id hyprvx-testclient-c
hvx_dispatch workspace 1 > /dev/null 2>&1 || true
hvx_vkeyboard_start kb
hvx_vpointer_start vp
# a's centre (140,100)+(150,100); b's centre (960,560)+(150,100); empty canvas at (700,100).
A="290 200" B="1110 660" EMPTY="700 100"
: > "$HVX_EVID/cases.txt"
for abort in panic off workspace pan-panic quit; do
    hvx_dispatch hyprvx:on > "$HVX_EVID/$abort.on.reply" 2>&1 || true
    start=$A
    [[ $abort == pan-panic ]] && start=$EMPTY
    read -r sx sy <<< "$start"
    hvx_key kb down KEY_LEFTMETA
    run "$abort-press" "move $sx $sy\ndown\nmove $((sx + 30)) $((sy + 20))\n"
    case $abort in
        panic|pan-panic) hvx_dispatch hyprvx:panic > "$HVX_EVID/$abort.reply" 2>&1 || true ;;
        off) hvx_dispatch hyprvx:off > "$HVX_EVID/$abort.reply" 2>&1 || true ;;
        workspace) hvx_dispatch workspace 2 > "$HVX_EVID/$abort.reply" 2>&1 || true ;;
        quit)
            hvx_client_cmd a quit
            hvx_wait_for "$HVX_EVID/a.jsonl" '"ev": "quit"' || hvx_die_blocked "client a did not quit"
            ;;
    esac
    if [[ $abort == quit || $abort == workspace ]]; then
        barrier=b
        [[ $abort == workspace ]] && barrier=c2
        printf "move %s %s\nup\n" $((sx + 60)) $((sy + 40)) > "$HVX_EVID/$abort-release.cmds"
        hvx_pointer_run "$abort-release.steps" vp "$HVX_EVID/$abort-release.cmds" "$barrier"
    else
        run "$abort-release" "move $((sx + 60)) $((sy + 40))\nup\n"
    fi
    hvx_key kb up KEY_LEFTMETA
    [[ $abort == workspace ]] && { hvx_dispatch workspace 1 > /dev/null 2>&1 || true; }
    alive=yes
    hvx_hyprctl_form version > "$HVX_EVID/$abort.version.txt" 2>&1 || { alive=no; fail "$abort: compositor not answering"; }
    before=$(presses b)
    read -r bx by <<< "$B"
    printf 'move %s %s\nclick\n' "$bx" "$by" > "$HVX_EVID/$abort-click.cmds"
    hvx_pointer_run "$abort-click.steps" vp "$HVX_EVID/$abort-click.cmds" b
    hvx_ready b
    reached=$(( $(presses b) - before ))
    echo "$abort alive=$alive click_reached_b=$reached" >> "$HVX_EVID/cases.txt"
    ((reached == 1)) || fail "$abort: the next click did not reach b (stuck grab?)"
    hvx_capture "$abort-after" b
done
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-DRAG-ABORT panic/off/workspace/quit during a drag: alive, no stuck grab (V-21)" \
    "form $HVX_FORM; aborts panic off workspace pan-panic quit; output $SIZE scale $SCALE"
[[ $result == PASS ]]
