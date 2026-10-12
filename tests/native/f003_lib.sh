#!/usr/bin/env bash
# Shared scene and measurement helpers for the Feature 003 trackpad tests (test-tooling 1.5 section 9). Source after
# lib.sh. Scene: floating clients a (140,100), c (520,320) and b (960,560), each 300x200 logical (window rules), an
# optional TOP layer client, hyprvx-vkeyboard `kb`, hyprvx-vpointer `vp`, the form's policy plugin and the test-only
# plugin (hvxtest). Finger scrolls go through vpointer `fscroll`, gestures through `hvx_gesture`.

TP_GEOM="$HVX_REPO/tests/native/f003_geom.py"
TP_F2="$HVX_REPO/tests/native/f002_geom.py"
TP_CLIENTS=(a b c)
TP_W=300
TP_H=200
declare -A TP_POS=([a]="140 100" [b]="960 560" [c]="520 320")
# A point of empty canvas at the identity camera (no window, no layer): left of c, below a.
TP_CANVAS_X=60
TP_CANVAS_Y=420
TP_TIME=100000   # event time base for fscroll / hvxtest (ms); tp_next_time advances it

# tp_setup [--layer] [--no-drift] [--stock]: the scene above with the pointer on empty canvas and Drift on at the
# identity camera. --no-drift leaves Drift OFF (plugin loaded); --stock starts the stock build without any plugin
# (reference sessions; logs go to stock.* names chosen by the caller).
tp_setup() {
    local layer=0 drift=1 stock=0 arg
    for arg in "$@"; do
        case $arg in --layer) layer=1 ;; --no-drift) drift=0 ;; --stock) stock=1; drift=0 ;; *) hvx_die_blocked "tp_setup: $arg" ;; esac
    done
    if ((stock)); then
        hvx_copy_build_info stock
        hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/stock.session.log" 2>&1 \
            || { cat "$HVX_EVID/stock.session.log" >&2; hvx_die_blocked "stock session did not start"; }
    else
        hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
            || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
        hvx_load_testinput || hvx_die_blocked "test-only plugin did not load"
    fi
    hvx_client_start a "${TP_W}x${TP_H}"
    hvx_client_start b "${TP_W}x${TP_H}" --app-id hyprvx-testclient-b
    hvx_client_start c "${TP_W}x${TP_H}" --app-id hyprvx-testclient-c
    if ((layer)); then
        hvx_client_start layer 240x140 --layer TOP --position 20,640 --app-id hyprvx-layer
        TP_CLIENTS=(a b c layer)
    fi
    hvx_vkeyboard_start kb
    hvx_vpointer_start vp
    tp_run home "move $TP_CANVAS_X $TP_CANVAS_Y"
    ((stock)) && return 0
    # Glide is off unless the test sets TP_GLIDE (a released pan would otherwise keep moving and change the expected
    # boxes of pan tests). The keyword reply is recorded; a refused value is BLOCKED.
    local reply
    reply=$(hvx_hyprctl_form keyword plugin:hyprvx:glide "${TP_GLIDE:-0}" 2>&1)
    echo "keyword plugin:hyprvx:glide ${TP_GLIDE:-0} -> $reply" >> "$HVX_EVID/keywords.log"
    [[ $reply == ok ]] || hvx_die_blocked "keyword plugin:hyprvx:glide refused: $reply"
    ((drift)) && { hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true; }
    return 0
}

# tp_axis_list NAME CLIENT: the CLIENT's scroll events in NAME.steps (axis, axis_source, axis_stop, axis_value120), one
# per line without per-run fields, for comparisons between sessions or modes.
tp_axis_list() {
    python3 -I -c "
import json, sys
for s in json.load(open(sys.argv[1]))['steps']:
    for e in s.get('events', {}).get(sys.argv[2], []):
        if e.get('ev', '').startswith('axis'):
            print(json.dumps({k: v for k, v in e.items() if k not in ('seq', 't_ns')}, sort_keys=True))" "$HVX_EVID/$1.steps.json" "$2"
}

# tp_next_time [GAP_MS]: advances the event time base by 10 s (or GAP_MS) and prints it, so a sequence never falls
# inside the 200 ms idle window of the previous one.
tp_next_time() {
    TP_TIME=$((TP_TIME + ${1:-10000}))
    echo "$TP_TIME"
}

# tp_run NAME LINE...: vpointer commands (one per argument) through pointer_driver; events attributed per client.
tp_run() {
    local name=$1
    shift
    printf '%s\n' "$@" > "$HVX_EVID/$name.cmds"
    hvx_pointer_run "$name.steps" vp "$HVX_EVID/$name.cmds" "${TP_CLIENTS[@]}"
}

# tp_events NAME [CLIENT...]: number of pointer events (any kind) attributed to the clients (default all) in NAME.steps.
tp_events() {
    python3 -I -c "
import json, sys
steps = json.load(open(sys.argv[1]))['steps']
who = sys.argv[2:]
print(sum(len(v) for s in steps for k, v in s.get('events', {}).items() if not who or k in who))" "$HVX_EVID/$1.steps.json" "${@:2}"
}

# tp_events_of NAME CLIENT EV: the CLIENT events of kind EV in NAME.steps, one JSON object per line.
tp_events_of() {
    python3 -I -c "
import json, sys
for s in json.load(open(sys.argv[1]))['steps']:
    for e in s.get('events', {}).get(sys.argv[2], []):
        if e.get('ev') == sys.argv[3]:
            print(json.dumps(e))" "$HVX_EVID/$1.steps.json" "$2" "$3"
}

# tp_capture NAME: one capture of the scene (ready rule on client a).
tp_capture() {
    hvx_capture "$1" a
}

# tp_box NAME WIN BOX TOL: measures window WIN in capture NAME against the logical drawn box BOX ("X,Y,W,H"); the
# analyzer region is the expected physical box padded by 12 px, so only that window's markers count. Exit 1 on mismatch.
tp_box() {
    local name=$1 win=$2 box=$3 tol=$4 phys region
    phys=$(hvx_phys_box "$box")
    region=$(python3 -I "$TP_F2" pad --box "$phys" --by 12)
    echo "$name $win expect $box phys $phys tol $tol" >> "$HVX_EVID/boxes.txt"
    hvx_analyze "$name-$win" bbox "$HVX_EVID/$name.png" --expect-box "$phys" --tolerance "$tol" --region "$region"
}

# tp_drawn WIN PX PY Z: logical drawn box of WIN under the camera (P, z), M = (0, 0): ((L - P)·z, size·z).
tp_drawn() {
    local x y
    read -r x y <<< "${TP_POS[$1]}"
    python3 -I -c "
import sys
x, y, px, py, z, w, h = map(float, sys.argv[1:])
print('%.6g,%.6g,%.6g,%.6g' % ((x - px) * z, (y - py) * z, w * z, h * z))" "$x" "$y" "$2" "$3" "$4" "$TP_W" "$TP_H"
}

# tp_scene_ok NAME PX PY Z TOL: every window that lies fully on the logical output (SIZE / SCALE) is at its drawn box
# under (P, z); windows
# partly off the output are recorded as not judged. Exit 1 if a judged window is off.
tp_scene_ok() {
    local name=$1 px=$2 py=$3 z=$4 tol=$5 win box ok=0 vis judged=0
    for win in a b c; do
        box=$(tp_drawn "$win" "$px" "$py" "$z")
        vis=$(python3 -I -c "
import sys
x, y, w, h = map(float, sys.argv[1].split(',')); W, H = (float(v) / float(sys.argv[3]) for v in sys.argv[2].split('x'))
print(1 if x >= 2 and y >= 2 and x + w <= W - 2 and y + h <= H - 2 else 0)" "$box" "$SIZE" "$SCALE")
        if [[ $vis == 1 ]]; then
            judged=$((judged + 1))
            tp_box "$name" "$win" "$box" "$tol" || ok=1
        else
            echo "$name $win not fully visible at $box (not judged)" >> "$HVX_EVID/boxes.txt"
        fi
    done
    # A step where no window is fully visible would pass without judging anything.
    if ((judged == 0)); then
        echo "$name: no window fully visible; nothing judged" >> "$HVX_EVID/boxes.txt"
        return 1
    fi
    return $ok
}
