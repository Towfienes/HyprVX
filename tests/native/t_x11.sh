#!/usr/bin/env bash
# T-X11 (Feature 002 US3.5, FR-006/SC-002/SC-003, V-07, T019). An X11 (XWayland) client, hyprvx-x11client 400x300 at
# its window-rule position, with an override-redirect menu (WM_TRANSIENT_FOR = main) at main-relative 100,80 160x120.
# Mechanism part (no policy): the dev control gives the main window and the menu window the same world offset O; for
# each camera state the main window and the menu are found within +-2 px of forwardBox(L + O) (main and popup
# palettes) and >= 20 pointer moves over each reach it ("main"/"menu" events, X window-local) at inverse(S) - (L + O)
# +-1. The DISPLAY comes only from `nested.sh xdisplay` (the verified compositor's XWayland). Setup validity: with
# offset 0 at identity the main window is at L and the menu at L + (100,80), else BLOCKED.
# HVX_X11_POLICY=1 (default; 0 skips it) adds the policy case (T042) (research R9): the policy is loaded, Drift is on, the X11 window gets
# O = (300,100) through the policy itself (Super + left drag by O at z = 1; the move drag is T048) and each camera
# state through the Drift controls (pointer at (0,0), hyprvx:reset, zoom z, pan -P z). Every new menu must then take
# its parent's offset by itself (WM_TRANSIENT_FOR). A second X11 client whose menu has no parent (--menu-parent no)
# checks the fallback: the menu takes the offset of the focused X11 window of its client. If the policy move drag is not
# available yet (the drag leaves the image unchanged), the policy case is recorded NOT RUN with the reason.
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
CAMERA_MODE=dev
set_camera() { # TAG PX PY Z
    if [[ $CAMERA_MODE == dev ]]; then
        hvx_nested dev camera "$2" "$3" "$4" > /dev/null || hvx_die_blocked "$1: dev camera rejected"
        return
    fi
    printf 'move 0 0\n' > "$HVX_EVID/$1-home.cmds"
    hvx_pointer_run "$1-home.steps" vp "$HVX_EVID/$1-home.cmds" x w
    {
        hvx_dispatch hyprvx:reset
        hvx_dispatch hyprvx:zoom "$4"
        hvx_dispatch hyprvx:pan "$(python3 -I -c "print(-$2 * $4)")" "$(python3 -I -c "print(-$3 * $4)")"
    } > "$HVX_EVID/$1.controls.reply" 2>&1 || true
}
run_states() {
    local pre=$1 dev_menu=$2 st tag px py z menu
    for st in "${STATES[@]}"; do
        read -r tag px py z <<< "$st"
        tag="$pre$tag"
        set_camera "$tag" "$px" "$py" "$z"
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

if [[ ${HVX_X11_POLICY:-1} == 1 ]]; then
    hvx_nested dev panic > /dev/null || true
    [[ $HVX_FORM == S ]] || hvx_plugin_load || hvx_die_blocked "policy plugin did not load"
    hvx_vkeyboard_start kb
    hvx_dispatch hyprvx:on > "$HVX_EVID/policy-on.reply" 2>&1 || true
    capture policy-before
    # O through the policy: Super + left drag from the X11 window's centre by O at identity (O += d / z, z = 1).
    read -r MCX MCY < <(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); print(round(x + w/2), round(y + h/2))" "$MAIN_L")
    printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$MCX" "$MCY" $((MCX + ox / 2)) $((MCY + oy / 2)) \
        $((MCX + ox)) $((MCY + oy)) > "$HVX_EVID/policy-move.cmds"
    hvx_key kb down KEY_LEFTMETA
    hvx_pointer_run policy-move.steps vp "$HVX_EVID/policy-move.cmds" x w
    hvx_key kb up KEY_LEFTMETA
    capture policy-moved
    if hvx_differs policy-move-validity "$HVX_EVID/policy-moved.png" "$HVX_EVID/policy-before.png"; then
        CAMERA_MODE=policy
        run_states policy- 0
        # Fallback: a second X11 client whose menu has no WM_TRANSIENT_FOR; it is focused (mapped last) and gets
        # O = (300,100) the same way; its menu must take the focused window's offset.
        # y is mapped with Drift OFF, so spawn placement (R6) leaves it at L; x keeps its stored offset across off/on.
        set_camera policy-fb-cam 0 0 1
        hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
        hvx_x11client_start y 400x300 --menu 100,80,160,120 --menu-parent no --class hyprvx-x11client-y
        hvx_dispatch hyprvx:on > /dev/null 2>&1 || true
        hvx_x11_ready y w
        clients fb
        read -r ADDR_Y YX YY YW YH < <(python3 -I "$GEOM" client "$HVX_EVID/fb.clients.json" hyprvx-x11client-y) || hvx_die_blocked "no second X11 client"
        Y_L="$YX,$YY,$YW,$YH"
        read -r YCX YCY < <(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); print(round(x + w/2), round(y + h/2))" "$Y_L")
        printf 'move %s %s\ndown\nmove %s %s\nup\n' "$YCX" "$YCY" $((YCX + ox)) $((YCY + oy)) > "$HVX_EVID/fb-move.cmds"
        hvx_key kb down KEY_LEFTMETA
        hvx_pointer_run fb-move.steps vp "$HVX_EVID/fb-move.cmds" y w
        hvx_key kb up KEY_LEFTMETA
        hvx_client_cmd y menu
        hvx_wait_for "$HVX_EVID/y.jsonl" '"ev": "menu_map"' || hvx_die_blocked "parentless menu never mapped"
        hvx_x11_ready y w
        hvx_nested capture "$HVX_EVID/fb.png" || hvx_die_blocked "capture fb failed"
        bbox fb-main 0 0 1 "$Y_L" "$O" main || result=FAIL
        bbox fb-menu 0 0 1 "$((YX + 100)),$((YY + 80)),160,120" "$O" popup || result=FAIL
        hvx_client_cmd y unmenu
    else
        HVX_NOT_RUN+=("policy case (T042, R9 inheritance): the policy move drag is not available yet (T048); Super + drag left the image unchanged")
    fi
    hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
fi

hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-X11 X11 window and override-redirect menu with a world offset (V-07)" \
    "form $HVX_FORM; O $O; policy case ${HVX_X11_POLICY:-1}; display $(cat "$HVX_EVID/xdisplay.txt"); output $SIZE scale $SCALE"
[[ $result == PASS ]]
