#!/usr/bin/env bash
# T-DRIFT-OFF (Feature 002 US1.2-US1.4, FR-003, FR-004, V-08, T026), three repetitions. Scene: floating test clients a,
# b and c (config rules place them; c is mapped last and has keyboard focus).
#  Reference: a stock session of the same scene, capture stock<i>.
#  Part A (exit = stock), in the form's session with the policy plugin:
#   A1. hyprvx:on, a camera and window offsets (dev control: mechanism state the policy must clear on exit), then
#       hyprvx:off: the capture equals stock<i> (0 pixels), `hyprctl clients` equals the snapshot before Drift.
#   A2. hyprvx:toggle, camera and offsets again, then Super+Escape (vkeyboard): same checks, and the Escape press is
#       consumed (c receives none).
#  Part B (re-entry restores, US1.4): camera and offsets set through the policy only (pointer at screen (0,0), then
#   hyprvx:zoom 0.5, hyprvx:pan 120 -40 and a Super + left drag that moves window a), capture B-drift; hyprvx:off,
#   `hyprctl clients` unchanged; hyprvx:on: the capture equals B-drift (windows back at their world positions, camera
#   restored). If those controls are not implemented yet (dispatcher refused, or the drag left the image unchanged),
#   Part B is recorded NOT RUN with the reason.
#  Whole session: no movewindow/movewindowv2 event on socket2.
# Setup validity: each Drift capture before an exit must differ from stock<i>, else the exit check is vacuous (BLOCKED).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
REPS="${HVX_DRIFT_OFF_REPS:-3}"

hvx_init_run "$(hvx_form_label)" t_drift_off
hvx_copy_build_info stock
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
scene() {
    hvx_client_start a 300x200
    hvx_client_start b 300x200 --app-id hyprvx-testclient-b
    hvx_client_start c 300x200 --app-id hyprvx-testclient-c
}
dev_state() { # camera P z, offsets for a and c
    hvx_nested dev camera 200 100 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
    hvx_nested dev offset "$ADDR_A" 300 150 > /dev/null || hvx_die_blocked "dev offset a rejected"
    hvx_nested dev offset "$ADDR_C" -200 100 > /dev/null || hvx_die_blocked "dev offset c rejected"
}
exit_checks() { # NAME REP
    hvx_capture "$1" c
    hvx_analyze "$1-vs-stock" diff "$HVX_EVID/$1.png" --reference "$HVX_EVID/stock$2.png" --max-pixels 0 \
        || fail "$1: capture after leaving Drift differs from stock"
    hvx_clients_snapshot "$1"
    cmp -s "$HVX_EVID/pre$2.clients.json" "$HVX_EVID/$1.clients.json" || fail "$1: hyprctl clients changed"
}

for i in $(seq 1 "$REPS"); do
    hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/stock$i.session.log" 2>&1 \
        || { cat "$HVX_EVID/stock$i.session.log" >&2; hvx_die_blocked "stock session $i did not start"; }
    HVX_OWN_SESSION=1
    scene
    hvx_capture "stock$i" c
    for w in a b c; do mv "$HVX_EVID/$w.jsonl" "$HVX_EVID/stock$i.$w.jsonl"; done
    hvx_stop >> "$HVX_EVID/stock$i.session.log" 2>&1 || hvx_die_blocked "stock session $i did not stop"

    hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/form$i.session.log" 2>&1 \
        || { cat "$HVX_EVID/form$i.session.log" >&2; hvx_die_blocked "form $HVX_FORM session $i did not start"; }
    SESSION_DIR=$(hvx_session_dir)
    hvx_events_start
    scene
    hvx_vkeyboard_start kb
    ADDR_A=$(hvx_window_address hyprvx-testclient)
    ADDR_C=$(hvx_window_address hyprvx-testclient-c)
    hvx_capture "pre$i" c
    hvx_clients_snapshot "pre$i"

    # A1: hyprvx:on / hyprvx:off
    hvx_dispatch hyprvx:on > "$HVX_EVID/a1-$i.on.reply" 2>&1 || true
    dev_state
    hvx_capture "a1-$i-drift" c
    hvx_differs "a1-$i-validity" "$HVX_EVID/a1-$i-drift.png" "$HVX_EVID/stock$i.png" \
        || hvx_die_blocked "A1 rep $i: Drift state looks like stock (setup invalid)"
    hvx_dispatch hyprvx:off > "$HVX_EVID/a1-$i.off.reply" 2>&1 || true
    exit_checks "a1-$i-off" "$i"

    # A2: hyprvx:toggle / Super+Escape
    hvx_dispatch hyprvx:toggle > "$HVX_EVID/a2-$i.toggle.reply" 2>&1 || true
    dev_state
    hvx_capture "a2-$i-drift" c
    hvx_differs "a2-$i-validity" "$HVX_EVID/a2-$i-drift.png" "$HVX_EVID/stock$i.png" \
        || hvx_die_blocked "A2 rep $i: Drift state looks like stock (setup invalid)"
    esc_before=$(hvx_escape_count c)
    hvx_super_escape kb
    exit_checks "a2-$i-off" "$i"
    (($(hvx_escape_count c) == esc_before)) || fail "a2-$i: the Super+Escape press reached the client in Drift"

    # B: re-entry through policy-held state only. Pointer at screen (0,0) first, so the zoom anchor is known.
    hvx_vpointer_start vp
    printf 'move 0 0\n' > "$HVX_EVID/b-$i.home.cmds"
    hvx_pointer_run "b-$i.home.steps" vp "$HVX_EVID/b-$i.home.cmds" c
    hvx_dispatch hyprvx:on > "$HVX_EVID/b-$i.on.reply" 2>&1 || true
    zoom=$(hvx_dispatch hyprvx:zoom 0.5 2>&1 || true)
    pan=$(hvx_dispatch hyprvx:pan 120 -40 2>&1 || true)
    printf 'zoom: %s\npan: %s\n' "$zoom" "$pan" > "$HVX_EVID/b-$i.controls.reply"
    hvx_capture "b-$i-camera" c
    # a's centre on screen: zoom 0.5 anchored at world (0,0) keeps P = (0,0); pan (120,-40) gives P = (-240, 80);
    # S = (W - P) * z.
    read -r ax ay < <(python3 -I - "$HVX_EVID/pre$i.clients.json" << 'PY'
import json, sys
a = next(c for c in json.load(open(sys.argv[1])) if c["class"] == "hyprvx-testclient")
cx, cy = a["at"][0] + a["size"][0] / 2, a["at"][1] + a["size"][1] / 2
print(round((cx + 240) * 0.5), round((cy - 80) * 0.5))
PY
)
    printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$ax" "$ay" $((ax + 40)) $((ay + 20)) $((ax + 80)) $((ay + 40)) \
        > "$HVX_EVID/b-$i.drag.cmds"
    hvx_key kb down KEY_LEFTMETA
    hvx_pointer_run "b-$i.drag.steps" vp "$HVX_EVID/b-$i.drag.cmds" c
    hvx_key kb up KEY_LEFTMETA
    hvx_capture "b-$i-drift" c
    moved=0
    hvx_differs "b-$i-moved" "$HVX_EVID/b-$i-drift.png" "$HVX_EVID/b-$i-camera.png" && moved=1
    if [[ $zoom == ok && $pan == ok && $moved == 1 ]]; then
        hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
        hvx_capture "b-$i-off" c
        hvx_clients_snapshot "b-$i-off"
        cmp -s "$HVX_EVID/pre$i.clients.json" "$HVX_EVID/b-$i-off.clients.json" || fail "b-$i: hyprctl clients changed"
        hvx_dispatch hyprvx:on > /dev/null 2>&1 || true
        hvx_capture "b-$i-reentry" c
        hvx_analyze "b-$i-reentry-diff" diff "$HVX_EVID/b-$i-reentry.png" --reference "$HVX_EVID/b-$i-drift.png" \
            --max-pixels 0 || fail "b-$i: re-entry did not restore the windows and the camera"
    elif [[ $i == 1 ]]; then
        HVX_NOT_RUN+=("Part B re-entry (US1.4): policy camera/move controls not available (zoom: $zoom; pan: $pan; move drag changed the image: $moved)")
    fi
    hvx_dispatch hyprvx:off > /dev/null 2>&1 || true

    hvx_stop >> "$HVX_EVID/form$i.session.log" 2>&1 || hvx_die_blocked "form session $i did not stop"
    cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/form$i.nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
    mv "$HVX_EVID/events.log" "$HVX_EVID/form$i.events.log"
    for f in a b c kb vp; do [[ -f $HVX_EVID/$f.jsonl ]] && mv "$HVX_EVID/$f.jsonl" "$HVX_EVID/form$i.$f.jsonl"; done
    moves=$(grep -cE ' movewindow(v2)?>>' "$HVX_EVID/form$i.events.log" || true)
    echo "rep $i: movewindow events $moves" >> "$HVX_EVID/movewindow.txt"
    [[ $moves == 0 ]] || fail "rep $i: $moves movewindow events on socket2"
done

hvx_finish "$result" "T-DRIFT-OFF leave Drift: capture = stock, clients unchanged, no movewindow, re-entry restores x$REPS (V-08)" \
    "form $HVX_FORM; dev camera (200,100) z 0.5, dev offsets a (300,150) c (-200,100); part B zoom 0.5, pan (120,-40), Super-drag a by (80,40); output $SIZE scale $SCALE"
[[ $result == PASS ]]
