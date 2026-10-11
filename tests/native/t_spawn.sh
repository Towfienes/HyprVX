#!/usr/bin/env bash
# T-SPAWN (Feature 002 US5, R6, V-22, T050). Client a on workspace 1. Drift on with the camera zoomed to 0.5 at screen
# (0,0) and panned by (300,-100) (P = (-600, 200)).
#  1. Client c (rule position (520,320), 300x200) is mapped in Drift: its drawn box is centred in the viewport,
#     (640 - 75, 400 - 50, 150, 100) +-2 px, while `hyprctl clients` keeps c at (520,320) (L is not written). Without
#     placement c would be drawn at ((520 + 600) * 0.5, (320 - 200) * 0.5) = (560, 60), so the check is not vacuous.
#  2. Drift off, client d (rule position (960,560)) is mapped: drawn at its logical box +-2 px, as stock.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_spawn
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
box_at() { # NAME CLIENT LOGICAL_BOX
    local phys
    phys=$(hvx_phys_box "$3")
    hvx_capture "$1" "$2"
    hvx_analyze "$1" bbox "$HVX_EVID/$1.png" --expect-box "$phys" --tolerance "$TOL" --region "$(python3 -I "$GEOM" pad --box "$phys" --by 12)"
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
{ hvx_dispatch hyprvx:zoom 0.5; hvx_dispatch hyprvx:pan 300 -100; } > "$HVX_EVID/camera.reply" 2>&1 || true

# 1. map in Drift
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
box_at spawn-drift c 565,350,150,100 || fail "c mapped in Drift is not centred in the viewport"
hvx_hyprctl_form -j clients > "$HVX_EVID/drift.clients.json"
python3 -I -c "
import json, sys
c = next(c for c in json.load(open(sys.argv[1])) if c['class'] == 'hyprvx-testclient-c')
sys.exit(0 if c['at'] == [520, 320] else 1)" "$HVX_EVID/drift.clients.json" || fail "c's logical position was written"

# 2. map with Drift OFF
hvx_dispatch hyprvx:off > "$HVX_EVID/off.reply" 2>&1 || true
hvx_client_start d 300x200 --app-id hyprvx-testclient-b
box_at spawn-off d 960,560,300,200 || fail "d mapped with Drift OFF is not at its stock position"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-SPAWN mapped in Drift: centred in the viewport +-2 px; mapped OFF: as stock (V-22)" \
    "form $HVX_FORM; camera zoom 0.5 at (0,0) then pan (300,-100); output $SIZE scale $SCALE"
[[ $result == PASS ]]
