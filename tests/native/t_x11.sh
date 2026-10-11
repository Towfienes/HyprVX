#!/usr/bin/env bash
# T-X11 (Feature 002 US3.5, FR-006/SC-002/SC-003, V-07, T019). An X11 (XWayland) client, hyprvx-x11client 400x300 at
# its window-rule position, with an override-redirect menu (WM_TRANSIENT_FOR = main) at main-relative 100,80 160x120.
# Mechanism part (no policy): the dev control gives the main window and the menu window the same world offset O; for
# each camera state the main window and the menu are found within +-2 px of forwardBox(L + O) (main and popup
# palettes) and >= 20 pointer moves over each reach it ("main"/"menu" events, X window-local) at inverse(S) - (L + O)
# +-1. The DISPLAY comes only from `nested.sh xdisplay` (the verified compositor's XWayland). Setup validity: with
# offset 0 at identity the main window is at L and the menu at L + (100,80), else BLOCKED.
# HVX_X11_POLICY=1 (T042) adds the policy case: with the policy loaded and Drift on, the menu takes its parent's offset
# by itself (override-redirect inheritance, research R9).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
PTOL="${HVX_POINTER_TOLERANCE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
POINTS="$HVX_REPO/tests/native/pointer_points.py"
O="300,100"
STATES=("identity 0 0 1" "z05 0 0 0.5" "z15 300 150 1.5")

hvx_init_run "$(hvx_form_label)" t_x11
hvx_start_form --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested session (form $HVX_FORM) did not start"; }
hvx_client_start w 300x200 --app-id hyprvx-testclient-b
hvx_nested xdisplay > "$HVX_EVID/xdisplay.txt" || hvx_die_blocked "nested XWayland display not resolvable through the verified PID"
hvx_x11client_start x 400x300 --menu 100,80,160,120
hvx_vpointer_start vp
hvx_x11_ready x w

clients() { hvx_hyprctl_form -j clients > "$HVX_EVID/$1.clients.json"; }
clients main
read -r ADDR_X XX XY XW XH < <(python3 -I "$GEOM" client "$HVX_EVID/main.clients.json" hyprvx-x11client) || hvx_die_blocked "no X11 client"
MAIN_L="$XX,$XY,$XW,$XH"
MENU_L="$((XX + 100)),$((XY + 80)),160,120"

capture() { # NAME
    hvx_x11_ready x w
    hvx_nested capture "$HVX_EVID/$1.png" || hvx_die_blocked "capture $1 failed"
}
bbox() { # NAME PX PY Z BOX OFFSET SET
    local exp phys
    exp=$(python3 -I "$GEOM" expect --camera "$2" "$3" "$4" --box "$5" --offset "$6" --scale "$SCALE")
    echo "$1 $exp" >> "$HVX_EVID/expected.txt"
    phys=$(python3 -I -c "import json,sys; print(json.loads(sys.argv[1])['phys'])" "$exp")
    hvx_analyze "$1" bbox "$HVX_EVID/${1%-*}.png" --marker-set "$7" --expect-box "$phys" --tolerance "$TOL" \
        --region "$(python3 -I "$GEOM" pad --box "$phys" --by 4)"
}

# Setup validity (identity, offset 0).
hvx_nested dev panic > /dev/null || hvx_die_blocked "dev panic failed"
hvx_client_cmd x menu
hvx_wait_for "$HVX_EVID/x.jsonl" '"ev": "menu_map"' || hvx_die_blocked "menu never mapped"
capture setup
bbox setup-main 0 0 1 "$MAIN_L" 0,0 main || hvx_die_blocked "X11 main window not at its logical box (setup invalid)"
bbox setup-menu 0 0 1 "$MENU_L" 0,0 popup || hvx_die_blocked "X11 menu not at main + (100,80) (setup invalid)"
clients menu
ADDR_MENU=$(python3 -I - "$HVX_EVID/menu.clients.json" "$ADDR_X" << 'PY'
import json, sys
c = [c for c in json.load(open(sys.argv[1])) if c.get("xwayland") and c["address"] != sys.argv[2] and c["size"] == [160, 120]]
print(c[0]["address"] if c else "")
PY
)
[[ -n $ADDR_MENU ]] || hvx_die_blocked "override-redirect menu window not listed"

result=PASS
run_states() { # TAG-PREFIX
    local pre=$1 st tag px py z
    for st in "${STATES[@]}"; do
        read -r tag px py z <<< "$st"
        tag="$pre$tag"
        hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "$tag: dev camera rejected"
        capture "$tag"
        bbox "$tag-main" "$px" "$py" "$z" "$MAIN_L" "$O" main || result=FAIL
        bbox "$tag-menu" "$px" "$py" "$z" "$MENU_L" "$O" popup || result=FAIL
        local main_w menu_w sample_w
        main_w=$(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); ox,oy = map(float, sys.argv[2].split(',')); print('%r,%r,%r,%r' % (x+ox, y+oy, w, h))" "$MAIN_L" "$O")
        menu_w=$(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); ox,oy = map(float, sys.argv[2].split(',')); print('%r,%r,%r,%r' % (x+ox, y+oy, w, h))" "$MENU_L" "$O")
        sample_w=$(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); print('%r,%r,90,70' % (x, y))" "$main_w")
        for kind in main menu; do
            local box smp=()
            box=$([[ $kind == main ]] && echo "$main_w" || echo "$menu_w")
            [[ $kind == main ]] && smp=(--sample "$sample_w")
            python3 -I "$POINTS" points --camera "$px" "$py" "$z" --surface "$box" "${smp[@]}" --n 20 --margin 3 \
                --commands "$HVX_EVID/$tag-$kind.cmds" --meta "$HVX_EVID/$tag-$kind.points.json" \
                || hvx_die_blocked "$tag: cannot sample the drawn X11 $kind"
            hvx_pointer_run "$tag-$kind.steps" vp "$HVX_EVID/$tag-$kind.cmds" x w
            python3 -I "$POINTS" check --meta "$HVX_EVID/$tag-$kind.points.json" --steps "$HVX_EVID/$tag-$kind.steps.json" \
                --client x --surface-name "$kind" --tol "$PTOL" --out "$HVX_EVID/$tag-$kind.input.json" || result=FAIL
        done
    done
}

read -r ox oy <<< "${O//,/ }"
hvx_nested dev offset "$ADDR_X" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset rejected for the X11 window"
hvx_nested dev offset "$ADDR_MENU" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset rejected for the menu window"
run_states ""

if [[ ${HVX_X11_POLICY:-0} == 1 ]]; then
    # Policy case (T042): Drift on; the menu is re-mapped and must take the parent's world offset by inheritance.
    hvx_nested dev panic > /dev/null || true
    hvx_client_cmd x unmenu
    hvx_plugin_load "$(hvx_plugin_path)" 2> /dev/null || [[ $HVX_FORM == S ]] || hvx_die_blocked "policy plugin did not load"
    hvx_dispatch hyprvx:on > "$HVX_EVID/policy-on.reply" || true
    hvx_nested dev offset "$ADDR_X" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset rejected (policy case)"
    hvx_client_cmd x menu
    hvx_wait_for "$HVX_EVID/x.jsonl" '"ev": "menu_map"' || hvx_die_blocked "menu never re-mapped"
    run_states policy-
    hvx_dispatch hyprvx:off > /dev/null || true
fi

hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-X11 X11 window and override-redirect menu with a world offset (V-07)" \
    "form $HVX_FORM; O $O; policy case ${HVX_X11_POLICY:-0}; display $(cat "$HVX_EVID/xdisplay.txt"); output $SIZE scale $SCALE"
[[ $result == PASS ]]
