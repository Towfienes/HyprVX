#!/usr/bin/env bash
# V2-as-is capture (V-08, T033): the negative control for GATE-N. Drives V2 through its own hyprvx:* dispatchers
# (research R2: zoom clamp [0.2, 1.5], centre-anchored; pan in logical px) via `nested.sh dispatch`, since V2 has no
# hyprvx-dev control. Records S1 input (build-info of variant v2) and S2/N inputs: V2 identity, zoom 0.5, 0.25
# (clamped by V2 to 0.2), zoom-in 1.5 and a pan sequence (where RC1 clipping can show), and partial client updates
# under a camera with a full-damage reference (`hyprvx:pan 0 0` damages the monitor without moving the camera;
# where RC2 stale regions can show). Each camera state is recorded in v2_states.jsonl for the GATE-N report.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run v2 v2_capture
hvx_copy_build_info v2
hvx_nested start --variant v2 --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested v2 session did not start"; }
hvx_client_start client 800x600
states="$HVX_EVID/v2_states.jsonl"
: > "$states"
v2() { hvx_nested dispatch "$@" >> "$HVX_EVID/dispatch.log" 2>&1 || hvx_die_blocked "V2 dispatcher failed: $*"; }
record() { printf '{"capture": "%s", "zoom": %s, "pan_steps": "%s"}\n' "$1" "$2" "$3" >> "$states"; }

hvx_capture v2_identity client;            record v2_identity 1.0 ""
v2 hyprvx:enable
v2 hyprvx:zoom 0.5;  hvx_capture v2_z05 client;   record v2_z05 0.5 ""
v2 hyprvx:zoom 0.25; hvx_capture v2_z025 client;  record v2_z025 0.2 "zoom 0.25 clamped by V2 to 0.2"
v2 hyprvx:zoom 1.5;  hvx_capture v2_z15 client;   record v2_z15 1.5 ""
v2 hyprvx:reset
v2 hyprvx:pan -300 0; hvx_capture v2_pan_right client; record v2_pan_right 1.0 "pan -300 0 (content moves right)"
v2 hyprvx:pan 300 300; hvx_capture v2_pan_up client;   record v2_pan_up 1.0 "pan 0 300 net (content moves up)"
v2 hyprvx:reset
v2 hyprvx:zoom 0.5
fills=("fill 100 100 37 23 C04020" "fill 401 251 64 64 20A0C0" "fill 777 577 23 23 30C030" "fill 33 533 101 41 F08080")
for k in "${!fills[@]}"; do
    hvx_client_cmd client "${fills[$k]}"
    hvx_capture "v2_fill$k" client
    v2 hyprvx:pan 0 0
    hvx_capture "v2_fill$k-full" client
    record "v2_fill$k" 0.5 "${fills[$k]}; reference v2_fill$k-full after hyprvx:pan 0 0"
done
v2 hyprvx:reset
v2 hyprvx:disable
hvx_nested stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish CAPTURED "V2-as-is captures for GATE-N (V-08)" "output $SIZE scale $SCALE; judged by tests/native/gate_n.sh"
