#!/usr/bin/env bash
# T-LAYER (US2.4, V-14, T031): a TOP layer-shell surface (hyprvx-testclient --layer TOP) must look identical with the
# identity camera and with non-identity cameras (layers are never transformed, FR-004), and the window border must
# enclose the transformed window surface (decorations follow the camera): the pixel just outside each edge of the
# transformed surface box has the configured border color.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
LAYER_BOX="980,620,240,140"

hvx_init_run patched t_layer
hvx_copy_build_info patched
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hvx_client_start window 800x600
hvx_client_start layer 240x140 --layer TOP --position 980,620 --app-id hyprvx-layer
hvx_capture identity window
# The layer client draws the same markers; measure the window outside the layer box.
hvx_analyze identity bbox "$HVX_EVID/identity.png" --expect-box 140,100,800,600 --tolerance 2 --region 0,0,970,800 \
    || hvx_die_blocked "identity window box not found (setup invalid)"
result=PASS
for cam in "0 0 0.5" "100 50 1.5" "-400 -300 0.25"; do
    read -r px py z <<< "$cam"
    name="cam_${px}_${py}_${z}"
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "$name: dev camera rejected"
    hvx_capture "$name" window
    python3 -I - "$HVX_EVID" "$name" "$LAYER_BOX" "$px" "$py" "$z" > "$HVX_EVID/$name.check" << 'PY' || result=FAIL
import json, sys
from pathlib import Path
from PIL import Image
evid, name, layer_box, px, py, z = sys.argv[1], sys.argv[2], sys.argv[3], *map(float, sys.argv[4:7])
x, y, w, h = map(int, layer_box.split(","))
ident = Image.open(Path(evid, "identity.png")).convert("RGB")
shot = Image.open(Path(evid, f"{name}.png")).convert("RGB")
layer_ok = list(ident.crop((x, y, x + w, y + h)).getdata()) == list(shot.crop((x, y, x + w, y + h)).getdata())
b = json.load(open(Path(evid, "identity.json")))["bbox"]
bx, by = (b["x"] - px) * z, (b["y"] - py) * z
bw, bh = b["w"] * z, b["h"] * z
border = (0x88, 0x88, 0x88)
def is_border(p):
    return all(abs(c - e) <= 24 for c, e in zip(p, border))
samples = {"left": (round(bx) - 1, round(by + bh / 2)), "right": (round(bx + bw), round(by + bh / 2)),
           "top": (round(bx + bw / 2), round(by) - 1), "bottom": (round(bx + bw / 2), round(by + bh))}
visible = {k: v for k, v in samples.items() if 0 <= v[0] < shot.width and 0 <= v[1] < shot.height
           and not (x <= v[0] < x + w and y <= v[1] < y + h)}
border_ok = all(is_border(shot.getpixel(v)) for v in visible.values())
print(json.dumps({"layer_identical": layer_ok, "border_samples": {k: [v, shot.getpixel(v)] for k, v in visible.items()},
                  "border_ok": border_ok}))
sys.exit(0 if layer_ok and border_ok else 1)
PY
done
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-LAYER layer untransformed, border encloses transformed surface (V-14)" "layer box $LAYER_BOX; output $SIZE scale $SCALE"
[[ $result == PASS ]]
