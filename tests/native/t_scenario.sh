#!/usr/bin/env bash
# T-SCENARIO (Feature 002 SC-008, V-25 input, T055): the user's 12-step scenario with test clients standing in for the
# applications: a = "VS Code" (rule (140,100)), b = "Firefox" (rule (960,560)), c = a third window (rule (520,320)), all
# 300x200. Every step is captured (stepNN-*.png) for the user's review; where a step has an objective outcome it is
# also checked. Cameras are computed exactly (transform contract), so pointer actions hit the drawn windows:
#   1  work in "VS Code": click a                                   (Drift OFF)
#   2  hyprvx:on
#   3  zoom out: hyprvx:zoom 0.5 with the pointer at (640,400)      P = (-640,-400), z = 0.5
#   4  several windows visible: a, b, c each found at forwardBox(L) +-2 px
#   5  move across the canvas: Super + left drag on empty canvas (1000,150) -> (800,100)      P = (-240,-300)
#   6  approach "Firefox": hyprvx:pan -100 -50                       P = (-40,-200); b drawn at (500,380,150,100)
#   7  zoom toward "Firefox": 3 Super + wheel-up detents at b's centre (575,430), z = 0.5 * 1.1^3; b's centre stays
#   8  work in "Firefox": click, KEY_A, one wheel detent at b's centre: b receives button, key and axis
#   9  move "Firefox": Super + left drag from b's centre by (-150,-100)
#   10 pin to the viewport: skipped (Phase 5), recorded in steps.txt
#   11 Super+Escape: Drift OFF (Escape consumed)
#   12 back to ordinary Hyprland: the capture equals a stock session after the same clicks (a, then b), and
#      `hyprctl clients` equals the snapshot before Drift
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
declare -A RULE=([a]="140,100,300,200" [b]="960,560,300,200" [c]="520,320,300,200")

hvx_init_run "$(hvx_form_label)" t_scenario
hvx_copy_build_info stock
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
note() { echo "$*" >> "$HVX_EVID/steps.txt"; }
scene() {
    hvx_client_start a 300x200
    hvx_client_start b 300x200 --app-id hyprvx-testclient-b
    hvx_client_start c 300x200 --app-id hyprvx-testclient-c
}
pointer() { # NAME COMMANDS CLIENTS...
    local name=$1 cmds=$2
    shift 2
    printf "$cmds" > "$HVX_EVID/$name.cmds"
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" "$@"
}
box_at() { # NAME IMAGE LOGICAL_BOX
    local phys
    phys=$(hvx_phys_box "$3")
    hvx_analyze "$1" bbox "$HVX_EVID/$2.png" --expect-box "$phys" --tolerance "$TOL" --region "$(python3 -I "$GEOM" pad --box "$phys" --by 12)"
}
count() { grep -c "$2" "$HVX_EVID/$1.jsonl" || true; }

# Reference for step 12: stock session, same scene, same clicks.
hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/stock.session.log" 2>&1 \
    || { cat "$HVX_EVID/stock.session.log" >&2; hvx_die_blocked "stock session did not start"; }
HVX_OWN_SESSION=1
scene
hvx_vpointer_start vp
pointer stock-clicks 'move 290 200\nclick\nmove 1110 660\nclick\n' a b c
hvx_capture stock b
for f in a b c vp; do mv "$HVX_EVID/$f.jsonl" "$HVX_EVID/stock.$f.jsonl"; done
hvx_stop >> "$HVX_EVID/stock.session.log" 2>&1 || hvx_die_blocked "stock session did not stop"

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
scene
hvx_vkeyboard_start kb
hvx_vpointer_start vp
: > "$HVX_EVID/steps.txt"

# 1
pointer step01 'move 290 200\nclick\n' a b c
hvx_clients_snapshot pre
hvx_capture step01-work a
note "1 work in VS Code (a clicked)"
# 2
hvx_dispatch hyprvx:on > "$HVX_EVID/step02.reply" 2>&1 || true
hvx_capture step02-drift-on a
note "2 hyprvx:on"
# 3
pointer step03 'move 640 400\n' a b c
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/step03.reply" 2>&1 || true
hvx_capture step03-zoom-out a
note "3 hyprvx:zoom 0.5 at (640,400)"
# 4
cp "$HVX_EVID/step03-zoom-out.png" "$HVX_EVID/step04-overview.png"
for w in a b c; do
    exp=$(python3 -I "$GEOM" expect --camera -640 -400 0.5 --box "${RULE[$w]}")
    box=$(python3 -I -c "import json,sys; print(','.join('%g' % v for v in json.loads(sys.argv[1])['logical']))" "$exp")
    box_at "step04-$w" step04-overview "$box" || fail "step 4: window $w not visible at its zoomed-out box"
done
note "4 overview: a, b, c visible"
# 5
hvx_key kb down KEY_LEFTMETA
pointer step05 'move 1000 150\ndown\nmove 900 125\nmove 800 100\nup\n' a b c
hvx_key kb up KEY_LEFTMETA
hvx_capture step05-pan-drag a
note "5 Super + canvas drag by (-200,-50)"
# 6
hvx_dispatch hyprvx:pan -100 -50 > "$HVX_EVID/step06.reply" 2>&1 || true
hvx_capture step06-approach b
box_at step06-b step06-approach 500,380,150,100 || fail "step 6: Firefox not where the camera puts it"
note "6 hyprvx:pan -100 -50: Firefox at (500,380)"
# 7
hvx_key kb down KEY_LEFTMETA
pointer step07 'move 575 430\nscroll 0 -1\nscroll 0 -1\nscroll 0 -1\n' a b c
hvx_key kb up KEY_LEFTMETA
hvx_capture step07-zoom-in b
Z7=$(python3 -I -c "print(0.5 * 1.1 ** 3)")
box_at step07-b step07-zoom-in "$(python3 -I -c "print('%g,%g,%g,%g' % (575 - 150 * $Z7, 430 - 100 * $Z7, 300 * $Z7, 200 * $Z7))")" \
    || fail "step 7: Super + wheel did not zoom toward Firefox around its centre"
note "7 3 Super+wheel detents at (575,430): z $Z7"
# 8
b0=$(count b '"ev": "button", "surface": "main", "button": 272, "state": "pressed"') k0=$(count b '"key": 30, "state": "pressed"') x0=$(count b '"ev": "axis"')
pointer step08-click 'move 575 430\nclick\n' a b c
hvx_key kb tap KEY_A
pointer step08-scroll 'scroll 0 1\n' a b c
hvx_ready b
hvx_capture step08-work b
(($(count b '"ev": "button", "surface": "main", "button": 272, "state": "pressed"') > b0)) || fail "step 8: the click did not reach Firefox"
(($(count b '"key": 30, "state": "pressed"') > k0)) || fail "step 8: KEY_A did not reach Firefox"
(($(count b '"ev": "axis"') > x0)) || fail "step 8: the wheel did not reach Firefox"
note "8 click, KEY_A, wheel reached Firefox"
# 9
hvx_key kb down KEY_LEFTMETA
pointer step09 'move 575 430\ndown\nmove 500 380\nmove 425 330\nup\n' a b c
hvx_key kb up KEY_LEFTMETA
hvx_capture step09-move b
box_at step09-b step09-move "$(python3 -I -c "print('%g,%g,%g,%g' % (425 - 150 * $Z7, 330 - 100 * $Z7, 300 * $Z7, 200 * $Z7))")" \
    || fail "step 9: Firefox did not follow the move drag"
note "9 Super + drag Firefox by (-150,-100)"
# 10
note "10 pin to the viewport: skipped (Phase 5)"
# 11
esc0=$(hvx_escape_count b)
hvx_super_escape kb
hvx_capture step11-drift-off b
(($(hvx_escape_count b) == esc0)) || fail "step 11: the Super+Escape press reached a client"
note "11 Super+Escape"
# 12
cp "$HVX_EVID/step11-drift-off.png" "$HVX_EVID/step12-ordinary.png"
hvx_analyze step12-vs-stock diff "$HVX_EVID/step12-ordinary.png" --reference "$HVX_EVID/stock.png" --max-pixels 0 \
    || fail "step 12: the capture after leaving Drift differs from stock"
hvx_clients_snapshot step12
cmp -s "$HVX_EVID/pre.clients.json" "$HVX_EVID/step12.clients.json" || fail "step 12: hyprctl clients changed"
note "12 ordinary Hyprland: capture = stock, clients unchanged"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-SCENARIO 12-step scenario captures (step 10 skipped, Phase 5) for human review (SC-008, V-25)" \
    "form $HVX_FORM; stand-ins a=VS Code b=Firefox c; output $SIZE scale $SCALE"
[[ $result == PASS ]]
