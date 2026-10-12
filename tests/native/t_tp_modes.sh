#!/usr/bin/env bash
# T-TP-MODES (Feature 003 FR-013, V-15, T034). Scene of f003_lib.sh. Reference capture `off-ref` with Drift OFF at the
# identity camera. For each mode change - hyprvx:off, hyprvx:panic, removal of the owning output - in the middle of a
# consumed gesture (a Super + pinch over a, so its events would reach a if released mid-way) and of a consumed finger
# scroll (Super + scroll over a):
#  - the remaining events stay consumed: a logs no pinch and no scroll event from that sequence;
#  - after the change the capture equals off-ref (Drift is off, camera identity), the compositor still answers. For the
#    output removal the reference is Hyprland's own window model instead: the round trip through its FALLBACK output
#    moves the floating windows itself (Feature 002 T-MONITOR: (140,100) -> (139,99)), so each window must be drawn
#    exactly (0 px) at the position and size `hyprctl clients` reports - the identity camera with no offsets;
#  - the next pinch over a without Super (Drift off) reaches a (behaves as configured).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
source "$(dirname -- "${BASH_SOURCE[0]}")/f003_lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_tp_modes
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
count() { grep -cE "\"ev\": \"($1)\"" "$HVX_EVID/a.jsonl" || true; }
hl_box() { # CLASS: "x,y,w,h" of that client in Hyprland's own window model
    hvx_hyprctl_form -j clients | python3 -I -c "
import json, sys
c = next(c for c in json.load(sys.stdin) if c.get('class') == sys.argv[1])
print(','.join(str(v) for v in c['at'] + c['size']))" "$1"
}
off_picture() { # NAME MODE: the capture NAME is the Drift-off picture (see the header)
    local name=$1 mode=$2 w cls box
    if [[ $mode != output ]]; then
        hvx_analyze "$name-vs-off" diff "$HVX_EVID/$name.png" --reference "$HVX_EVID/off-ref.png" --max-pixels 0
        return
    fi
    for w in a b c; do
        cls=hyprvx-testclient; [[ $w == a ]] || cls=hyprvx-testclient-$w
        box=$(hl_box "$cls") || return 1
        echo "$name $w hyprland box $box" >> "$HVX_EVID/hl-boxes.txt"
        tp_box "$name" "$w" "$box" 0 || return 1
    done
}

tp_setup --no-drift
tp_run over-a "move 290 200"
tp_capture off-ref

change() { # MODE: off | panic | output
    case $1 in
        off) hvx_dispatch hyprvx:off > "$HVX_EVID/$1.reply" 2>&1 || true ;;
        panic) hvx_dispatch hyprvx:panic > "$HVX_EVID/$1.reply" 2>&1 || true ;;
        output)
            hvx_hyprctl_form output remove HVX-1 > "$HVX_EVID/$1-remove.reply" 2>&1 || true
            hvx_hyprctl_form version > "$HVX_EVID/$1-alive.txt" 2>&1 || fail "$1: the compositor does not answer"
            hvx_hyprctl_form output create headless HVX-1 > "$HVX_EVID/$1-readd.reply" 2>&1 || true ;;
    esac
}

for mode in off panic output; do
    # consumed pinch
    hvx_dispatch hyprvx:on > "$HVX_EVID/on-$mode-p.reply" 2>&1 || true
    tp_run "$mode-p-pointer" "move 290 200"
    before=$(count 'pinch_begin|pinch_update|pinch_end')
    hvx_key kb down KEY_LEFTMETA
    T=$(tp_next_time)
    hvx_gesture pinch begin 2 "$T"
    hvx_gesture pinch update 2 0 0 0.6 0 $((T + 10))
    hvx_key kb up KEY_LEFTMETA
    change "$mode"
    hvx_gesture pinch update 2 0 0 0.7 0 $((T + 20))
    hvx_gesture pinch end 0 $((T + 30))
    (($(count 'pinch_begin|pinch_update|pinch_end') == before)) || fail "$mode: a received events of a consumed pinch"
    tp_run "$mode-p-back" "move 290 200"
    tp_capture "$mode-pinch"
    off_picture "$mode-pinch" "$mode" \
        || fail "$mode: the capture after the change mid-pinch is not the Drift-off picture"
    # next pinch over a, Drift off: delivered
    before=$(count 'pinch_begin')
    T=$(tp_next_time)
    hvx_gesture pinch begin 2 "$T"; hvx_gesture pinch update 2 0 0 1.2 0 $((T + 10)); hvx_gesture pinch end 0 $((T + 20))
    (($(count 'pinch_begin') == before + 1)) || fail "$mode: the next pinch over a (Drift off) did not reach a"

    # consumed scroll
    hvx_dispatch hyprvx:on > "$HVX_EVID/on-$mode-s.reply" 2>&1 || true
    before=$(count 'axis')
    hvx_key kb down KEY_LEFTMETA
    T=$(tp_next_time)
    tp_run "$mode-s1" "fscroll $T 10 5 0 5"
    hvx_key kb up KEY_LEFTMETA
    change "$mode"
    tp_run "$mode-s2" "move 290 200" "fscroll $((T + 50)) 10 5 0 5 stop"
    (($(count 'axis') == before)) || fail "$mode: a received events of a consumed scroll sequence"
    tp_capture "$mode-scroll"
    off_picture "$mode-scroll" "$mode" \
        || fail "$mode: the capture after the change mid-scroll is not the Drift-off picture"
done

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-TP-MODES Drift off, panic or output loss mid-sequence: rest consumed, OFF picture, next gesture normal (V-15)" \
    "form $HVX_FORM; off, panic, output removal during a Super + pinch and a Super + scroll; output $SIZE scale $SCALE"
[[ $result == PASS ]]
