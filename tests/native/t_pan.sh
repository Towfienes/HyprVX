#!/usr/bin/env bash
# T-PAN (US2.2, V-11, T028): pans of +-300 px on both axes at z in {0.5, 1.0}. The camera base P0 centers the
# window on the output; each pan applies panByScreen (P' = P0 - d/z, transform contract), so the window must move
# by exactly d on screen. A 400x160 window keeps every panned box fully on the 1280x800 output (+-300 px vertically
# needs a box at most 200 px tall). All four markers within +-2 px of forward(B) for every state.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_TOLERANCE:-2}"
OUT_W=${SIZE%x*}
OUT_H=${SIZE#*x}

camera_for() { # B_JSON Z DX DY -> "PX PY" (base centers the window, then panByScreen by d)
    python3 -I -c "
import json, sys
b = json.load(open(sys.argv[1]))['bbox']; z, dx, dy, ow, oh = map(float, sys.argv[2:7])
cx, cy = b['x'] + b['w'] / 2, b['y'] + b['h'] / 2
p0x, p0y = cx - (ow / 2) / z, cy - (oh / 2) / z
print(repr(p0x - dx / z), repr(p0y - dy / z))" "$@"
}
expected_box() { # B_JSON PX PY Z -> X,Y,W,H
    python3 -I -c "
import json, sys
b = json.load(open(sys.argv[1]))['bbox']; px, py, z = map(float, sys.argv[2:5])
print('%d,%d,%d,%d' % (round((b['x'] - px) * z), round((b['y'] - py) * z), round(b['w'] * z), round(b['h'] * z)))" "$@"
}

hvx_init_run patched t_pan
hvx_copy_build_info patched
result=PASS
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start client 400x160
hvx_capture identity client
hvx_analyze identity bbox "$HVX_EVID/identity.png" --expect-box 140,100,400,160 --tolerance "$TOL" \
    || hvx_die_blocked "identity box not found where the window rule puts it (setup invalid)"
for z in 0.5 1.0; do
    for d in "0 0" "300 0" "-300 0" "0 300" "0 -300"; do
        read -r dx dy <<< "$d"
        read -r px py <<< "$(camera_for "$HVX_EVID/identity.json" "$z" "$dx" "$dy" "$OUT_W" "$OUT_H")"
        name="pan_z${z}_${dx}_${dy}"
        hvx_nested dev camera "$px" "$py" "$z" > "$HVX_EVID/$name.reply" || hvx_die_blocked "$name: dev camera rejected"
        hvx_capture "$name" client
        box=$(expected_box "$HVX_EVID/identity.json" "$px" "$py" "$z")
        echo "$px $py $z -> $box" > "$HVX_EVID/$name.expected"
        hvx_analyze "$name" bbox "$HVX_EVID/$name.png" --expect-box "$box" --tolerance "$TOL" || result=FAIL
    done
done
hvx_nested dev panic > /dev/null || true
hvx_nested stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-PAN +-300 px both axes at z=0.5 and 1.0 (V-11)" "output $SIZE scale $SCALE tolerance $TOL px"
[[ $result == PASS ]]
