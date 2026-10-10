#!/usr/bin/env bash
# T-Z* (US2.1-2.2, V-10, T027): patched build, one client; at identity the surface box B is measured from markers,
# then for each camera (P, z) the expected box is forward(B) = M + (B - P)*z, size*z (transform contract; M = 0
# for the single nested output at 0x0). Every capture must show all four markers within +-2 px of the expected box.
# z = 0.25 and 0.5 use an 800x600 window; z = 2.0 uses a 400x300 window so the box fits the 1280x800 output.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"

expected_box() { # B_JSON PX PY Z -> X,Y,W,H (rounded); M = (0,0)
    python3 -I -c "
import json, sys
b = json.load(open(sys.argv[1]))['bbox']; px, py, z = map(float, sys.argv[2:5])
print('%d,%d,%d,%d' % (round((b['x'] - px) * z), round((b['y'] - py) * z), round(b['w'] * z), round(b['h'] * z)))" "$@"
}

hvx_init_run patched t_zoom
hvx_copy_build_info patched
result=PASS
run_case() { # NAME WINDOW P_X P_Y Z
    local name=$1 window=$2 px=$3 py=$4 z=$5 w h
    IFS=x read -r w h <<< "$window"
    hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/$name.session.log" 2>&1 \
        || { cat "$HVX_EVID/$name.session.log" >&2; hvx_die_blocked "nested patched session for $name did not start"; }
    hvx_client_start "$name" "$window"
    hvx_capture "$name-identity" "$name"
    hvx_analyze "$name-identity" bbox "$HVX_EVID/$name-identity.png" --expect-box "140,100,$w,$h" --tolerance "$TOL" \
        || hvx_die_blocked "$name: identity box not found where the window rule puts it (setup invalid)"
    hvx_nested dev camera "$px" "$py" "$z" > "$HVX_EVID/$name.camera.reply" || hvx_die_blocked "$name: dev camera rejected"
    hvx_capture "$name" "$name"
    local box
    box=$(expected_box "$HVX_EVID/$name-identity.json" "$px" "$py" "$z")
    echo "$box" > "$HVX_EVID/$name.expected"
    hvx_analyze "$name" bbox "$HVX_EVID/$name.png" --expect-box "$box" --tolerance "$TOL" || result=FAIL
    hvx_nested dev panic > /dev/null || true
    hvx_nested stop >> "$HVX_EVID/$name.session.log" 2>&1 || hvx_die_blocked "$name: session did not stop"
}
run_case z025 800x600 0 0 0.25
run_case z05 800x600 0 0 0.5
run_case z2 400x300 90 50 2.0
hvx_finish "$result" "T-Z zoom geometry z=0.25/0.5 (800x600), z=2.0 (400x300) (V-10)" "output $SIZE scale $SCALE tolerance $TOL px"
[[ $result == PASS ]]
