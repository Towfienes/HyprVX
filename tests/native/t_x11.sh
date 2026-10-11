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

menu_address() { # LABEL: the override-redirect menu window (xwayland, 160x120, not the main window)
    clients "$1"
    python3 -I - "$HVX_EVID/$1.clients.json" "$ADDR_X" << 'PY'
import json, sys
c = [c for c in json.load(open(sys.argv[1])) if c.get("xwayland") and c["address"] != sys.argv[2] and c["size"] == [160, 120]]
print(c[0]["address"] if c else "")
PY
}
map_menu() { # LABEL: maps the menu (a new window each time) and prints its address
    local before
    before=$(grep -c '"ev": "menu_map"' "$HVX_EVID/x.jsonl" || true)
    hvx_client_cmd x menu
    local deadline=$(($(date +%s%N) + HVX_READY_TIMEOUT * 1000000000))
    while (($(grep -c '"ev": "menu_map"' "$HVX_EVID/x.jsonl" || true) <= before)); do
        (($(date +%s%N) < deadline)) || hvx_die_blocked "menu never mapped ($1)"
        sleep 0.02
    done
    hvx_x11_ready x w
    menu_address "$1"
}
unmap_menu() {
    hvx_client_cmd x unmenu
    hvx_x11_ready x w
}
world() { # BOX OFFSET -> BOX + OFFSET
    python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); ox,oy = map(float, sys.argv[2].split(',')); print('%r,%r,%r,%r' % (x+ox, y+oy, w, h))" "$1" "$2"
}
pointer_check() { # TAG KIND PX PY Z BOX [SAMPLE]
    local tag=$1 kind=$2 px=$3 py=$4 z=$5 box=$6 smp=()
    [[ -n ${7:-} ]] && smp=(--sample "$7")
    python3 -I "$POINTS" points --camera "$px" "$py" "$z" --surface "$box" "${smp[@]}" --n 20 --margin 3 \
        --commands "$HVX_EVID/$tag-$kind.cmds" --meta "$HVX_EVID/$tag-$kind.points.json" \
        || hvx_die_blocked "$tag: cannot sample the drawn X11 $kind"
    hvx_pointer_run "$tag-$kind.steps" vp "$HVX_EVID/$tag-$kind.cmds" x w
    python3 -I "$POINTS" check --meta "$HVX_EVID/$tag-$kind.points.json" --steps "$HVX_EVID/$tag-$kind.steps.json" \
        --client x --surface-name "$kind" --tol "$PTOL" --out "$HVX_EVID/$tag-$kind.input.json"
}

# Setup validity (identity, offset 0, menu mapped).
hvx_nested dev panic > /dev/null || hvx_die_blocked "dev panic failed"
ADDR_MENU=$(map_menu setup)
[[ -n $ADDR_MENU ]] || hvx_die_blocked "override-redirect menu window not listed"
capture setup
bbox setup-main 0 0 1 "$MAIN_L" 0,0 main || hvx_die_blocked "X11 main window not at its logical box (setup invalid)"
bbox setup-menu 0 0 1 "$MENU_L" 0,0 popup || hvx_die_blocked "X11 menu not at main + (100,80) (setup invalid)"
unmap_menu

result=PASS
read -r ox oy <<< "${O//,/ }"
MAIN_W=$(world "$MAIN_L" "$O")
MENU_W=$(world "$MENU_L" "$O")
SAMPLE_W=$(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); print('%r,%r,90,70' % (x, y))" "$MAIN_W")
# run_states PREFIX DEV_MENU_OFFSET(1|0): per camera state, the main window's input with the menu closed, then the menu
# mapped (a new window): captured, both boxes judged, and the menu's input (it holds the pointer grab).
run_states() {
    local pre=$1 dev_menu=$2 st tag px py z menu
    for st in "${STATES[@]}"; do
        read -r tag px py z <<< "$st"
        tag="$pre$tag"
        hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "$tag: dev camera rejected"
        hvx_x11_ready x w
        pointer_check "$tag" main "$px" "$py" "$z" "$MAIN_W" "$SAMPLE_W" || result=FAIL
        menu=$(map_menu "$tag")
        [[ -n $menu ]] || hvx_die_blocked "$tag: menu window not listed"
        echo "$tag menu $menu" >> "$HVX_EVID/menus.txt"
        if [[ $dev_menu == 1 ]]; then
            hvx_nested dev offset "$menu" "$ox" "$oy" > /dev/null || hvx_die_blocked "$tag: dev offset rejected for the menu"
        fi
        capture "$tag"
        bbox "$tag-main" "$px" "$py" "$z" "$MAIN_L" "$O" main || result=FAIL
        bbox "$tag-menu" "$px" "$py" "$z" "$MENU_L" "$O" popup || result=FAIL
        pointer_check "$tag" menu "$px" "$py" "$z" "$MENU_W" || result=FAIL
        unmap_menu
    done
}

hvx_nested dev offset "$ADDR_X" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset rejected for the X11 window"
run_states "" 1

if [[ ${HVX_X11_POLICY:-0} == 1 ]]; then
    # Policy case (T042): Drift on; each new menu must take its parent's world offset by itself (research R9).
    hvx_nested dev panic > /dev/null || true
    [[ $HVX_FORM == S ]] || hvx_plugin_load || hvx_die_blocked "policy plugin did not load"
    hvx_dispatch hyprvx:on > "$HVX_EVID/policy-on.reply" || true
    hvx_nested dev offset "$ADDR_X" "$ox" "$oy" > /dev/null || hvx_die_blocked "dev offset rejected (policy case)"
    run_states policy- 0
    hvx_dispatch hyprvx:off > /dev/null || true
fi

hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-X11 X11 window and override-redirect menu with a world offset (V-07)" \
    "form $HVX_FORM; O $O; policy case ${HVX_X11_POLICY:-0}; display $(cat "$HVX_EVID/xdisplay.txt"); output $SIZE scale $SCALE"
[[ $result == PASS ]]
