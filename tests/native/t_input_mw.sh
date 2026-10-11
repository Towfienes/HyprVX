#!/usr/bin/env bash
# T-INPUT-MW (Feature 002 US3.1/US3.2/US3.4 mechanism, FR-006/FR-010, V-05, T017). Window A (800x600 with an xdg_popup
# at parent-local 300,200 200x120 and a wl_subsurface at 40,380 160x100) and window B (300x200, app-id
# hyprvx-testclient-b) get distinct world offsets from the dev control (no policy). For each camera state:
#  - >= 20 pointer moves over the drawn part of A's main surface (outside its popup/subsurface), over A's popup, A's
#    subsurface and B: every enter/motion is for the right surface at surfaceLocal(S) - O = inverse(S) - (L + O) +-1;
#  - a point on the empty canvas (no drawn window) reaches no client;
#  - a click on drawn B focuses B (button and keyboard enter at B, nothing at A), a click on drawn A focuses A.
# Setup validity first: identity camera, offsets 0, a move to A's centre reaches A at (400,300) +-1, else BLOCKED.
# Pointer constraints: hyprvx-testclient cannot hold a pointer constraint, so the constraint case is recorded NOT RUN.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
TOL="${HVX_POINTER_TOLERANCE:-1}"
GEOM="$HVX_REPO/tests/native/f002_geom.py"
POINTS="$HVX_REPO/tests/native/pointer_points.py"
OA="-60,40"
OB="100,-500"
STATES=("identity 0 0 1" "z05 0 0 0.5" "z15 100 100 1.5")

hvx_init_run "$(hvx_form_label)" t_input_mw
hvx_start_form --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested session (form $HVX_FORM) did not start"; }
hvx_client_start a 800x600 --popup 300,200,200,120 --subsurface 40,380,160,100
hvx_wait_for "$HVX_EVID/a.jsonl" '"ev": "popup_configure", "x": 300, "y": 200' || hvx_die_blocked "popup not configured at 300,200"
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_vpointer_start vp
hvx_hyprctl_form -j clients > "$HVX_EVID/clients.json"
read -r ADDR_A AX AY AW AH < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" hyprvx-testclient) || hvx_die_blocked "no client A"
read -r ADDR_B BX BY BW BH < <(python3 -I "$GEOM" client "$HVX_EVID/clients.json" hyprvx-testclient-b) || hvx_die_blocked "no client B"

steps() { # NAME CLIENT... ; commands from stdin
    cat > "$HVX_EVID/$1.cmds"
    local name=$1
    shift
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" "$@"
}
add() { # X,Y,W,H OX,OY -> world box X+OX,Y+OY,W,H
    python3 -I -c "import sys; x,y,w,h = map(float, sys.argv[1].split(',')); ox,oy = map(float, sys.argv[2].split(',')); print('%r,%r,%r,%r' % (x+ox, y+oy, w, h))" "$1" "$2"
}

# Setup validity (identity, offsets 0, stock path).
hvx_nested dev panic > /dev/null || hvx_die_blocked "dev panic failed"
printf 'move %s %s\n' "$((AX + 400))" "$((AY + 300))" | steps setup a b
python3 -I - "$HVX_EVID/setup.steps.json" "$TOL" << 'PY' || hvx_die_blocked "identity setup: a move to A's centre did not reach A at (400,300)"
import json, sys
ev = [e for e in json.load(open(sys.argv[1]))["steps"][0]["events"]["a"] if e["ev"] in ("enter", "motion")]
tol = float(sys.argv[2])
sys.exit(0 if ev and abs(ev[-1]["x"] - 400) <= tol and abs(ev[-1]["y"] - 300) <= tol else 1)
PY

read -r oax oay <<< "${OA//,/ }"
read -r obx oby <<< "${OB//,/ }"
hvx_nested dev offset "$ADDR_A" "$oax" "$oay" > /dev/null || hvx_die_blocked "dev offset rejected for A"
hvx_nested dev offset "$ADDR_B" "$obx" "$oby" > /dev/null || hvx_die_blocked "dev offset rejected for B"
A_WORLD=$(add "$AX,$AY,$AW,$AH" "$OA")
B_WORLD=$(add "$BX,$BY,$BW,$BH" "$OB")
POPUP_WORLD=$(add "$((AX + 300)),$((AY + 200)),200,120" "$OA")
SUB_WORLD=$(add "$((AX + 40)),$((AY + 380)),160,100" "$OA")
A_SAMPLE=$(add "$AX,$AY,280,180" "$OA") # top-left part of A, clear of the popup and the subsurface
printf 'A %s\nB %s\npopup %s\nsub %s\n' "$A_WORLD" "$B_WORLD" "$POPUP_WORLD" "$SUB_WORLD" > "$HVX_EVID/world-boxes.txt"

result=PASS
sample() { # STATE PX PY Z TAG CLIENT SURFACE-NAME SURFACE-BOX [SAMPLE-BOX]
    local st=$1 px=$2 py=$3 z=$4 tag=$5 who=$6 sname=$7 box=$8 smp=${9:-}
    local args=(--camera "$px" "$py" "$z" --surface "$box" --n 20 --margin 3 --commands "$HVX_EVID/$st-$tag.cmds"
                --meta "$HVX_EVID/$st-$tag.points.json")
    [[ -n $smp ]] && args+=(--sample "$smp")
    python3 -I "$POINTS" points "${args[@]}" || { echo "$st-$tag: not visible enough, not sampled" >> "$HVX_EVID/skipped.txt"; return 0; }
    hvx_pointer_run "$st-$tag.steps" vp "$HVX_EVID/$st-$tag.cmds" a b
    python3 -I "$POINTS" check --meta "$HVX_EVID/$st-$tag.points.json" --steps "$HVX_EVID/$st-$tag.steps.json" --client "$who" \
        --surface-name "$sname" --tol "$TOL" --out "$HVX_EVID/$st-$tag.result.json"
}
for st in "${STATES[@]}"; do
    read -r tag px py z <<< "$st"
    hvx_nested dev camera "$px" "$py" "$z" > /dev/null || hvx_die_blocked "$tag: dev camera rejected"
    hvx_capture "$tag" a
    sample "$tag" "$px" "$py" "$z" main a main "$A_WORLD" "$A_SAMPLE" || result=FAIL
    sample "$tag" "$px" "$py" "$z" popup a popup "$POPUP_WORLD" || result=FAIL
    sample "$tag" "$px" "$py" "$z" sub a sub "$SUB_WORLD" || result=FAIL
    sample "$tag" "$px" "$py" "$z" b b main "$B_WORLD" || result=FAIL
done

# Empty canvas and click focus at z = 0.5, P = (0, 0): A drawn at (A_WORLD)/2, B at (B_WORLD)/2.
hvx_nested dev camera 0 0 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
read -r empty_x empty_y b_x b_y a_x a_y < <(python3 -I - "$A_WORLD" "$B_WORLD" << 'PY'
import sys
ax, ay, aw, ah = map(float, sys.argv[1].split(",")); bx, by, bw, bh = map(float, sys.argv[2].split(","))
# Empty: below drawn A and left of drawn B, inside A's logical box (where an untransformed hit-test would find A).
print(int((ax + aw) / 2 + 40), int((ay + ah) / 2 + 40), int((bx + bw / 2) / 2), int((by + bh / 2) / 2), int((ax + 40) / 2), int((ay + 40) / 2))
PY
)
printf 'move %s %s\nmove %s %s\n' "$a_x" "$a_y" "$empty_x" "$empty_y" | steps empty a b
python3 -I - "$HVX_EVID/empty.steps.json" "$HVX_EVID/empty.result.json" << 'PY' || result=FAIL
import json, sys
steps = json.load(open(sys.argv[1]))["steps"]
reasons = []
if not [e for e in steps[0]["events"]["a"] if e["ev"] in ("enter", "motion")]:
    reasons.append("no_event_on_drawn_a")
for who in ("a", "b"):
    if [e for e in steps[1]["events"][who] if e["ev"] in ("enter", "motion")]:
        reasons.append(f"motion_on_empty_canvas:{who}")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons}, open(sys.argv[2], "w"), indent=1)
print("empty canvas:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY

click_check() { # NAME X Y WHO OTHER EXPECT_CLASS
    local name=$1 x=$2 y=$3 who=$4 other=$5 cls=$6
    local kb_before
    kb_before=$(grep -c '"ev": "kb_enter"' "$HVX_EVID/$who.jsonl" || true)
    printf 'move %s %s\nclick\n' "$x" "$y" | steps "$name" a b
    hvx_hyprctl_form -j activewindow > "$HVX_EVID/$name.activewindow.json" || hvx_die_blocked "$name: activewindow query failed"
    python3 -I - "$HVX_EVID/$name.steps.json" "$HVX_EVID/$name.activewindow.json" "$who" "$other" "$cls" "$kb_before" \
        "$HVX_EVID/$who.jsonl" "$HVX_EVID/$name.result.json" << 'PY'
import json, sys
steps_path, active_path, who, other, cls, kb_before, log, out = sys.argv[1:]
steps = json.load(open(steps_path))["steps"]
active = json.load(open(active_path))
reasons = []
if active.get("class") != cls:
    reasons.append(f"active_window:{active.get('class')!r}")
if [b["state"] for b in steps[1]["events"][who] if b["ev"] == "button"] != ["pressed", "released"]:
    reasons.append("buttons_missing")
if [e for e in steps[1]["events"][other] if e["ev"] == "button"]:
    reasons.append("button_reached_other_window")
kb = sum(1 for line in open(log) if '"ev": "kb_enter"' in line)
if kb <= int(kb_before) and active.get("class") != cls:
    reasons.append("no_keyboard_enter")
json.dump({"verdict": "FAIL" if reasons else "PASS", "reasons": reasons, "active": active.get("class")}, open(out, "w"), indent=1)
print(f"{out.rsplit('/', 1)[-1]}:", "FAIL " + " ".join(reasons) if reasons else "PASS")
sys.exit(1 if reasons else 0)
PY
}
click_check click_b "$b_x" "$b_y" b a hyprvx-testclient-b || result=FAIL
click_check click_a "$a_x" "$a_y" a b hyprvx-testclient || result=FAIL

echo "pointer constraint across a camera/offset change: NOT RUN (hyprvx-testclient cannot hold a pointer constraint)" \
    | tee "$HVX_EVID/constraint.notrun"
HVX_NOT_RUN+=("pointer constraint case: hyprvx-testclient cannot hold a pointer constraint")
hvx_nested dev panic > /dev/null || true
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-INPUT-MW pointer local, popup, subsurface, empty canvas, click focus with offsets (V-05)" \
    "form $HVX_FORM; O(A) $OA O(B) $OB; states ${STATES[*]}; output $SIZE scale $SCALE"
[[ $result == PASS ]]
