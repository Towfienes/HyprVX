#!/usr/bin/env bash
# T-PAN (Feature 002 US2, FR-006, FR-007, V-13, T034). Floating client a (300x200 at (140,100)) and a TOP layer client
# (240x140 at (20,640)). Pointer at screen (0,0), Drift on, `hyprvx:zoom 0.5` (P stays (0,0)): a is drawn at
# (70,50,150,100). Then, each measured on a's corner markers (region around a, so the layer's markers are not used):
#  1. `hyprvx:pan 100 60`: a moves by (100,60) +-1 px.
#  2. Super + left drag on empty canvas from (600,300) to (480,380): a moves by (-120,80) +-1 px.
#  3. Super + left drag starting on the TOP layer surface, (100,700) to (160,650): not a pan, the capture is unchanged.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_pan_drift
hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start layer 240x140 --layer TOP --position 20,640 --app-id hyprvx-layer
hvx_vkeyboard_start kb
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true

result=PASS
: > "$HVX_EVID/boxes.txt"
measure() { # NAME LOGICAL_BOX TOLERANCE -> FAIL on mismatch; the measured box is in NAME.json
    local phys region
    phys=$(hvx_phys_box "$2")
    region=$(python3 -I "$GEOM" pad --box "$phys" --by 12)
    hvx_capture "$1" a
    echo "$1 expect $2 phys $phys" >> "$HVX_EVID/boxes.txt"
    hvx_analyze "$1" bbox "$HVX_EVID/$1.png" --expect-box "$phys" --tolerance "$3" --region "$region"
}
measure zoomed 70,50,150,100 2 || hvx_die_blocked "a not at the zoomed position (zoom 0.5 at (0,0) did not apply)"
# The next expectations start from the measured box (logical = physical / scale), so only the pan itself is judged.
read -r X0 Y0 < <(python3 -I -c "
import json, sys
b = json.load(open(sys.argv[1]))['bbox']; s = float(sys.argv[2])
print(b['x'] / s, b['y'] / s)" "$HVX_EVID/zoomed.json" "$SCALE")

hvx_dispatch hyprvx:pan 100 60 > "$HVX_EVID/pan.reply" 2>&1 || true
measure pan "$(python3 -I -c "print('%g,%g,150,100' % ($X0 + 100, $Y0 + 60))")" 1 \
    || { echo "hyprvx:pan did not move content by (100,60)" >&2; result=FAIL; }

printf 'move 600 300\ndown\nmove 560 330\nmove 520 355\nmove 480 380\nup\n' > "$HVX_EVID/canvas.cmds"
hvx_key kb down KEY_LEFTMETA
hvx_pointer_run canvas.steps vp "$HVX_EVID/canvas.cmds" a layer
hvx_key kb up KEY_LEFTMETA
measure canvas "$(python3 -I -c "print('%g,%g,150,100' % ($X0 + 100 - 120, $Y0 + 60 + 80))")" 1 \
    || { echo "Super + canvas drag did not move content by (-120,80)" >&2; result=FAIL; }

hvx_capture before-layer a
printf 'move 100 700\ndown\nmove 130 675\nmove 160 650\nup\n' > "$HVX_EVID/layer.cmds"
hvx_key kb down KEY_LEFTMETA
hvx_pointer_run layer.steps vp "$HVX_EVID/layer.cmds" a layer
hvx_key kb up KEY_LEFTMETA
hvx_capture after-layer a
hvx_analyze layer-not-pan diff "$HVX_EVID/after-layer.png" --reference "$HVX_EVID/before-layer.png" --max-pixels 0 \
    || { echo "a Super + left drag over the TOP layer panned the canvas" >&2; result=FAIL; }

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-PAN dispatcher pan and Super + canvas drag +-1 px; drag over a top layer is not a pan (V-13)" \
    "form $HVX_FORM; zoom 0.5 at (0,0); pan (100,60); canvas drag (-120,80); layer drag (60,-50); output $SIZE scale $SCALE"
[[ $result == PASS ]]
