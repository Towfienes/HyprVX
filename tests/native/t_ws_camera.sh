#!/usr/bin/env bash
# T-WS-CAMERA (Feature 002 US2, R2, V-14, T035). Pointer at screen (0,0) throughout, so zoom anchors keep P = (0,0).
#  1. Workspace 1 (client a): Drift on, zoom 0.5, pan (100,50): capture ws1.
#  2. Workspace 2 (client d, rule position (960,560)): on first entry the camera is identity, not workspace 1's (d at
#     its logical box +-2 px); zoom 0.8: capture ws2 (d at (768,448,240,160) +-2 px).
#  3. Back to 1: capture = ws1 (0 px); back to 2: capture = ws2 (0 px).
#  4. Recreated id: workspace 3 with client e, zoom 0.5: capture ws3; e quits, workspace 1 (workspace 3 is destroyed,
#     checked in `hyprctl workspaces`), workspace 3 again with a new e: capture = ws3 (stored camera restored).
# Clients are mapped while Drift is OFF (d at setup; e with Drift switched off around its map), so spawn placement (R6)
# does not move them and their drawn boxes depend on the camera alone; stored cameras survive Drift off/on.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_ws_camera
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
same() { # NAME IMAGE REFERENCE
    hvx_analyze "$1" diff "$HVX_EVID/$2.png" --reference "$HVX_EVID/$3.png" --max-pixels 0 || fail "$1: $2 differs from $3"
}
box_at() { # NAME CLIENT LOGICAL_BOX
    local phys region
    phys=$(hvx_phys_box "$3")
    region=$(python3 -I "$GEOM" pad --box "$phys" --by 12)
    hvx_analyze "$1" bbox "$HVX_EVID/$1.png" --expect-box "$phys" --tolerance "$TOL" --region "$region"
}
ws() { hvx_dispatch workspace "$1" > "$HVX_EVID/ws$1-$2.reply" 2>&1 || true; }

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
ws 2 setup
hvx_client_start d 300x200 --app-id hyprvx-testclient-b
ws 1 setup
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a

# 1
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > /dev/null 2>&1 || true
hvx_dispatch hyprvx:pan 100 50 > /dev/null 2>&1 || true
hvx_capture ws1 a
box_at ws1 a 170,100,150,100 || hvx_die_blocked "workspace 1 camera not applied (setup invalid)"

# 2
ws 2 first
hvx_capture ws2-first d
box_at ws2-first d 960,560,300,200 || fail "workspace 2 does not start at identity (workspace 1's camera leaked)"
hvx_dispatch hyprvx:zoom 0.8 > /dev/null 2>&1 || true
hvx_capture ws2 d
box_at ws2 d 768,448,240,160 || fail "workspace 2 zoom 0.8 not applied"

# 3
ws 1 back
hvx_capture ws1-back a
same ws1-back ws1-back ws1
ws 2 back
hvx_capture ws2-back d
same ws2-back ws2-back ws2

# 4
hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
ws 3 first
hvx_client_start e 300x200 --app-id hyprvx-testclient-c
hvx_dispatch hyprvx:on > /dev/null 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > /dev/null 2>&1 || true
hvx_capture ws3 e
hvx_client_cmd e quit
hvx_wait_for "$HVX_EVID/e.jsonl" '"ev": "quit"' || hvx_die_blocked "client e did not quit"
mv "$HVX_EVID/e.jsonl" "$HVX_EVID/e1.jsonl"
ws 1 leave3
hvx_capture ws1-again a
hvx_hyprctl_form -j workspaces > "$HVX_EVID/workspaces.json"
python3 -I -c "
import json, sys
sys.exit(1 if any(w['id'] == 3 for w in json.load(open(sys.argv[1]))) else 0)" "$HVX_EVID/workspaces.json" \
    || hvx_die_blocked "workspace 3 was not destroyed (setup invalid)"
hvx_dispatch hyprvx:off > /dev/null 2>&1 || true
ws 3 recreated
hvx_client_start e 300x200 --app-id hyprvx-testclient-c
hvx_dispatch hyprvx:on > /dev/null 2>&1 || true
hvx_capture ws3-recreated e
same ws3-recreated ws3-recreated ws3

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-WS-CAMERA per-workspace cameras, no leak, recreated id gets its camera (V-14)" \
    "form $HVX_FORM; ws1 zoom 0.5 pan (100,50); ws2 zoom 0.8; ws3 zoom 0.5; output $SIZE scale $SCALE"
[[ $result == PASS ]]
