#!/usr/bin/env bash
# T-SCROLL (Feature 002 US3, FR-006/FR-010, V-17, T041). Floating clients a and b; Drift on, camera zoomed to 0.5 at
# screen (0,0), world offset a (100,50) (dev control). With the pointer at the centre of a's drawn box:
#  1. `scroll 0 1` (one wheel detent, no modifier) reaches a: its log gains a vertical axis event on "main"; b gets none;
#     the capture is unchanged (no zoom).
#  2. With Super held, `scroll 0 1` zooms (the capture changes) and neither a nor b gets an axis event.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_scroll
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
axis() { grep -c '"ev": "axis", "axis": "v"' "$HVX_EVID/$1.jsonl" || true; }

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_vkeyboard_start kb
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a b
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true
hvx_hyprctl_form -j clients > "$HVX_EVID/clients.json"
read -r ADDR_A AX AY AW AH < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" hyprvx-testclient) || hvx_die_blocked "no client a"
hvx_nested dev offset "$ADDR_A" 100 50 > /dev/null || hvx_die_blocked "dev offset rejected"
read -r CX CY < <(python3 -I -c "
import json, sys
b = json.loads(sys.argv[1])['logical']
print(round(b[0] + b[2] / 2), round(b[1] + b[3] / 2))" "$(python3 -I "$GEOM" expect --camera 0 0 0.5 --box "$AX,$AY,$AW,$AH" --offset 100,50)")
printf 'move %s %s\n' "$CX" "$CY" > "$HVX_EVID/over-a.cmds"
hvx_pointer_run over-a.steps vp "$HVX_EVID/over-a.cmds" a b
hvx_capture before a

# 1. plain wheel
a0=$(axis a) b0=$(axis b)
printf 'scroll 0 1\n' > "$HVX_EVID/plain.cmds"
hvx_pointer_run plain.steps vp "$HVX_EVID/plain.cmds" a b
hvx_capture plain a
a1=$(axis a) b1=$(axis b)
echo "plain: a $((a1 - a0)) b $((b1 - b0))" >> "$HVX_EVID/axis.txt"
((a1 > a0)) || fail "plain scroll over a did not reach a"
((b1 == b0)) || fail "plain scroll over a reached b"
grep '"ev": "axis"' "$HVX_EVID/a.jsonl" | tail -1 | grep -q '"surface": "main"' || fail "a's axis event is not on its main surface"
hvx_analyze plain-no-zoom diff "$HVX_EVID/plain.png" --reference "$HVX_EVID/before.png" --max-pixels 0 \
    || fail "plain scroll changed the image (zoomed without Super)"

# 2. Super + wheel
hvx_key kb down KEY_LEFTMETA
printf 'scroll 0 1\n' > "$HVX_EVID/super.cmds"
hvx_pointer_run super.steps vp "$HVX_EVID/super.cmds" a b
hvx_key kb up KEY_LEFTMETA
hvx_capture super a
a2=$(axis a) b2=$(axis b)
echo "super: a $((a2 - a1)) b $((b2 - b1))" >> "$HVX_EVID/axis.txt"
((a2 == a1 && b2 == b1)) || fail "Super + scroll reached a client"
hvx_differs super-zoomed "$HVX_EVID/super.png" "$HVX_EVID/plain.png" || fail "Super + scroll did not zoom"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-SCROLL scroll reaches the offset window; Super + scroll zooms and the client gets nothing (V-17)" \
    "form $HVX_FORM; zoom 0.5 at (0,0); offset a (100,50); output $SIZE scale $SCALE"
[[ $result == PASS ]]
