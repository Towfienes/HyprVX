#!/usr/bin/env bash
# T-RESIZE (Feature 002 US4.3, FR-009, V-20, T020) mechanism part. Camera z = 0.5, P = (0, 0) and a world offset
# O = (100, 50) on a --resizable 300x200 floating client A (dev control, no policy); A is drawn at
# forwardBox(L + O).
#  1. A border drag pressed just outside A's drawn bottom-right corner and moved by a screen delta d resizes A for
#     real: its last configure is (300, 200) + d / z +-1 (worldDelta, transform contract 1.2).
#  2. Hyprland's own resize bind (the nested config's no-modifier BTN_EXTRA resizewindow) pressed in A's drawn
#     bottom-right quadrant and moved by d' resizes A by d' / z +-1 from its new size.
#  3. The same resize bind on a tiled client T changes nothing (no new configure of T, `hyprctl clients` size unchanged)
#     and the nested log has one "blocked ... resize drag of tiled window" line for it.
# Setup validity (identity control, run first): at identity and offset 0 the border drag changes A's size by d, else
# the criteria above are vacuous and the run is BLOCKED.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_POINTER_TOLERANCE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"

hvx_init_run "$(hvx_form_label)" t_resize
hvx_start_form --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested session (form $HVX_FORM) did not start"; }
SESSION_DIR=$(hvx_session_dir)
hvx_client_start a 300x200 --resizable
hvx_vpointer_start vp
steps() { # NAME CLIENT... ; commands from stdin
    cat > "$HVX_EVID/$1.cmds"
    local name=$1
    shift
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" "$@"
}
last_configure() { # CLIENT -> "w h" of its last configure
    python3 -I -c "
import json, sys
c = [json.loads(l) for l in open(sys.argv[1]) if '\"ev\": \"configure\"' in l]
print(c[-1]['w'], c[-1]['h']) if c else print('0 0')" "$HVX_EVID/$1.jsonl"
}
hvx_hyprctl_form -j clients > "$HVX_EVID/clients0.json"
read -r ADDR_A AX AY AW AH < <(python3 -I "$GEOM" client "$HVX_EVID/clients0.json" hyprvx-testclient) || hvx_die_blocked "no client A"

# Setup validity: identity, offset 0; border drag from just outside the bottom-right corner by (40, 30).
hvx_nested dev panic > /dev/null || hvx_die_blocked "dev panic failed"
read -r w0 h0 <<< "$(last_configure a)"
printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$((AX + AW + 3))" "$((AY + AH + 3))" "$((AX + AW + 23))" "$((AY + AH + 18))" \
    "$((AX + AW + 43))" "$((AY + AH + 33))" | steps setup a
read -r w1 h1 <<< "$(last_configure a)"
echo "setup: ${w0}x${h0} -> ${w1}x${h1}" > "$HVX_EVID/setup.txt"
(( w1 == w0 + 40 && h1 == h0 + 30 )) || hvx_die_blocked "identity border drag did not resize by (40,30): ${w0}x${h0} -> ${w1}x${h1} (setup invalid)"
hvx_hyprctl_form -j clients > "$HVX_EVID/clients1.json"
read -r _ AX AY AW AH < <(python3 -I "$GEOM" client "$HVX_EVID/clients1.json" hyprvx-testclient)

# Camera and offset.
hvx_nested dev offset "$ADDR_A" 100 50 > /dev/null || hvx_die_blocked "dev offset rejected"
hvx_nested dev camera 0 0 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
hvx_capture camera a
result=PASS
check_size() { # NAME EXPECT_W EXPECT_H
    local w h
    read -r w h <<< "$(last_configure a)"
    python3 -I - "$1" "$w" "$h" "$2" "$3" "$TOL" "$HVX_EVID/$1.result.json" << 'PY'
import json, sys
name, w, h, ew, eh, tol, out = sys.argv[1], *map(float, sys.argv[2:7]), sys.argv[7]
ok = abs(w - ew) <= tol and abs(h - eh) <= tol
json.dump({"configure": [w, h], "expect": [ew, eh], "tolerance": tol, "verdict": "PASS" if ok else "FAIL"}, open(out, "w"), indent=1)
print(f"{name}: configure {w}x{h} expect {ew}x{eh}", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PY
}
# 1. Border drag under the camera: A's drawn box is ((AX + 100) / 2, (AY + 50) / 2, AW / 2, AH / 2); press 2 px outside
#    its drawn bottom-right corner (4 world units, inside the border grab area) and move by d = (40, 30).
read -r bx by < <(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1:5]); print(int((x + 100 + w) / 2 + 2), int((y + 50 + h) / 2 + 2))" "$AX" "$AY" "$AW" "$AH")
printf 'move %s %s\ndown\nmove %s %s\nmove %s %s\nup\n' "$bx" "$by" "$((bx + 20))" "$((by + 15))" "$((bx + 40))" "$((by + 30))" | steps border a
check_size border "$((AW + 80))" "$((AH + 60))" || result=FAIL
read -r w2 h2 <<< "$(last_configure a)"
hvx_capture after-border a
# 2. The resize bind (BTN_EXTRA = 276) in A's drawn bottom-right quadrant, d' = (20, 10).
read -r qx qy < <(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1:5]); print(int((x + 100 + 0.8 * w) / 2), int((y + 50 + 0.8 * h) / 2))" "$AX" "$AY" "$w2" "$h2")
printf 'move %s %s\ndown 276\nmove %s %s\nmove %s %s\nup 276\n' "$qx" "$qy" "$((qx + 10))" "$((qy + 5))" "$((qx + 20))" "$((qy + 10))" | steps bind a
check_size bind "$((w2 + 40))" "$((h2 + 20))" || result=FAIL
hvx_capture after-bind a

# 3. A tiled client and the resize bind on it: nothing changes, one blocked line.
hvx_client_start t 400x300 --resizable --app-id hyprvx-tiled
hvx_hyprctl_form -j clients > "$HVX_EVID/clients-tiled-before.json"
read -r _ TX TY TW TH < <(python3 -I "$GEOM" client "$HVX_EVID/clients-tiled-before.json" hyprvx-tiled) || hvx_die_blocked "no tiled client"
tiled_cfg_before=$(grep -c '"ev": "configure"' "$HVX_EVID/t.jsonl" || true)
# A point on drawn T that is not on drawn A: T's drawn box is (TX/2, TY/2, TW/2, TH/2); A's drawn box is in its top-left.
read -r tx ty < <(python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1:5]); print(int((x + 0.9 * w) / 2), int((y + 0.9 * h) / 2))" "$TX" "$TY" "$TW" "$TH")
printf 'move %s %s\ndown 276\nmove %s %s\nmove %s %s\nup 276\n' "$tx" "$ty" "$((tx - 20))" "$((ty - 10))" "$((tx - 40))" "$((ty - 20))" | steps tiled a t
hvx_hyprctl_form -j clients > "$HVX_EVID/clients-tiled-after.json"
tiled_cfg_after=$(grep -c '"ev": "configure"' "$HVX_EVID/t.jsonl" || true)
python3 -I - "$HVX_EVID/clients-tiled-before.json" "$HVX_EVID/clients-tiled-after.json" "$tiled_cfg_before" "$tiled_cfg_after" \
    "$HVX_EVID/tiled.result.json" << 'PY' || result=FAIL
import json, sys
before, after, cb, ca, out = sys.argv[1:]
def box(path):
    return [(c["at"], c["size"]) for c in json.load(open(path)) if c.get("class") == "hyprvx-tiled"]
reasons = []
if box(before) != box(after):
    reasons.append(f"tiled_geometry_changed:{box(before)}->{box(after)}")
if int(ca) != int(cb):
    reasons.append(f"tiled_configures:{cb}->{ca}")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons}, open(out, "w"), indent=1)
print("tiled:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY

hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
blocked_tiled=$(grep -c "HyprVX: blocked a compositor resize drag of tiled window" "$HVX_EVID/nested-hyprland.log" || true)
echo "blocked tiled resize lines: $blocked_tiled" | tee "$HVX_EVID/log-check.txt"
[[ $blocked_tiled == 1 ]] || result=FAIL
hvx_finish "$result" "T-RESIZE floating resize through the camera, tiled resize blocked (V-20 mechanism part)" \
    "form $HVX_FORM; camera z 0.5, O(A) (100,50); border d (40,30), bind d (20,10); output $SIZE scale $SCALE"
[[ $result == PASS ]]
