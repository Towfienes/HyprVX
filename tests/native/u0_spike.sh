#!/usr/bin/env bash
# U0 unified-world feasibility spike (DV3-12, master plan U3/C5). Throwaway branch v3/u0-spike, patch 0010 on top of
# 0001-0009; evidence only, never merged. Scale 1, output 1280x800, nested patched session only.
#
# Scene: A (600x400) on workspace 1, B (300x200, app-id hyprvx-testclient-b) on workspace 2, C (600x400) on
# workspace 3; workspace 1 active. Drift places workspace k at world x offset (k-1)*1280 (anchor: workspace 1).
# Camera P=(0,0), z=1/3: A is drawn at (46.7,33.3) 200x133.3, B at (746.7,186.7) 100x66.7, C at (900,33.3) 200x133.3.
#   Q1 render: without drift only A is drawn; with drift all three are drawn at those boxes (+-2 px).
#   Q3 frame callbacks of a drift-visible window: no-damage commit without/with the spike's "frames" flag; damage commit.
#   Q4 xdg_toplevel suspended state of a drift-visible window: stock vs the spike's "awake" flag.
#   Q5 wl_surface.enter/leave of hidden-workspace windows (enter/leave are per output).
#   Q2 hit-test and surface-local coordinates of drift windows through windowAt (+-1 px).
#   Q6 focus -> workspace switch: hover and click on a drift window; the image must not move (camera does not jump),
#      the cursor must not be warped.
#   Q7 workspace-switch animation (slide, 1 s) while drifting: mid-animation captures vs the static image; control:
#      the same switch without drift is visibly animated at the same capture times.
#   Q8 ownership (C5, U-3, U-4): no movewindow events while drifting; clients before/after equal in workspace.id, at,
#      size, floating, fullscreen.
# Each question gets PASS / FAIL / BLOCKED with its observations in answers.jsonl; the run is PASS only if all PASS.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
CHECK="$HVX_REPO/tests/native/u0_check.py"
Z=0.3333333333333333
TOL=2
PTOL=1

hvx_init_run patched u0_spike
hvx_copy_build_info patched
hvx_start --variant patched --size 1280x800 --scale 1 > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
hctl() { hvx_hyprctl patched "$@"; }
now_ns() { python3 -I -c 'import time; print(time.monotonic_ns())'; }
check() { python3 -I "$CHECK" "$@"; }
overall=PASS
answer() { # Q VERDICT DETAIL_JSON_FILE...
    local q=$1 verdict=$2
    shift 2
    python3 -I - "$q" "$verdict" "$@" >> "$HVX_EVID/answers.jsonl" << 'PY'
import json, sys
q, verdict, *files = sys.argv[1:]
print(json.dumps({"q": q, "verdict": verdict, "detail": {f.rsplit("/", 1)[-1]: open(f).read().strip() for f in files}}))
PY
    echo "$q: $verdict"
    [[ $verdict == PASS ]] || overall=FAIL
}

hvx_nested run --background -- python3 -I "$HVX_REPO/tests/native/u0_events.py" "$HVX_EVID/events.log" > /dev/null
hvx_client_start a 600x400
hctl dispatch workspace 2 > "$HVX_EVID/setup.reply"
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hctl dispatch workspace 3 >> "$HVX_EVID/setup.reply"
hvx_client_start c 600x400
hctl dispatch workspace 1 >> "$HVX_EVID/setup.reply"
hvx_ready a
hctl -j clients > "$HVX_EVID/clients_before.json"
hctl -j activeworkspace > "$HVX_EVID/activeworkspace_setup.json"
python3 -I - "$HVX_EVID/clients_before.json" "$HVX_EVID/activeworkspace_setup.json" << 'PY' || hvx_die_blocked "setup: windows not on workspaces 1/2/3 at the expected layout boxes"
import json, sys
clients = {c["title"]: c for c in json.load(open(sys.argv[1]))}
want = {"a": (1, [140, 100], [600, 400]), "b": (2, [960, 560], [300, 200]), "c": (3, [140, 100], [600, 400])}
ok = all(t in clients and clients[t]["workspace"]["id"] == ws and clients[t]["at"] == at and clients[t]["size"] == size
         for t, (ws, at, size) in want.items())
sys.exit(0 if ok and json.load(open(sys.argv[2]))["id"] == 1 else 1)
PY

# ---- Q1 render ---------------------------------------------------------------------------------------------------
hvx_capture base a
hvx_nested dev camera 0 0 "$Z" > "$HVX_EVID/camera.reply" || hvx_die_blocked "dev camera rejected"
hvx_capture cam_nodrift a
q1=PASS
hvx_analyze cam_nodrift_a bbox "$HVX_EVID/cam_nodrift.png" --expect-box 47,33,200,133 --tolerance $TOL --region 0,0,640,800 || q1=FAIL
check absent "$HVX_EVID/cam_nodrift.png" 640,0,640,800 > "$HVX_EVID/q1_nodrift_absent.json" || q1=FAIL
T_DRIFT_ON=$(now_ns)
hvx_nested dev drift on > "$HVX_EVID/drift_on.reply" || hvx_die_blocked "dev drift on rejected"
hvx_capture drift a
hvx_analyze drift_a bbox "$HVX_EVID/drift.png" --expect-box 47,33,200,133 --tolerance $TOL --region 0,0,640,800 || q1=FAIL
hvx_analyze drift_b bbox "$HVX_EVID/drift.png" --expect-box 747,187,100,67 --tolerance $TOL --region 640,150,240,200 || q1=FAIL
hvx_analyze drift_c bbox "$HVX_EVID/drift.png" --expect-box 900,33,200,133 --tolerance $TOL --region 880,0,400,250 || q1=FAIL
answer Q1-render $q1 "$HVX_EVID/q1_nodrift_absent.json" "$HVX_EVID/cam_nodrift_a.json" "$HVX_EVID/drift_a.json" \
    "$HVX_EVID/drift_b.json" "$HVX_EVID/drift_c.json"

# ---- Q3 frame callbacks, Q4 suspended ------------------------------------------------------------------------------
check configure-states "$HVX_EVID/b.jsonl" > "$HVX_EVID/q4_b_states_stock.json" || true
sync_frame() { # CLIENT OUT CMD: commit and wait 1.5 s for that commit's frame_done
    local before seq
    before=$(hvx_last_seq "$HVX_EVID/$1.jsonl")
    hvx_client_cmd "$1" "$3"
    hvx_wait_for "$HVX_EVID/$1.jsonl" "\"seq\": $((before + 1)), \"ev\": \"commit\"" || hvx_die_blocked "$1 did not commit"
    check wait-frame "$HVX_EVID/$1.jsonl" $((before + 1)) 1.5 > "$HVX_EVID/$2.json" || true
}
sync_frame b q3_b_sync_noflags sync
sync_frame b q3_b_redraw_noflags redraw
hvx_ready a
hvx_nested dev drift on frames > "$HVX_EVID/drift_frames.reply" || hvx_die_blocked "dev drift on frames rejected"
sync_frame b q3_b_sync_frames sync
sync_frame c q3_c_sync_frames sync
q3=PASS
python3 -I -c "import json,sys; sys.exit(0 if all(json.load(open(f))['delivered'] for f in sys.argv[1:]) else 1)" \
    "$HVX_EVID/q3_b_sync_frames.json" "$HVX_EVID/q3_c_sync_frames.json" "$HVX_EVID/q3_b_redraw_noflags.json" || q3=FAIL
answer Q3-frame-callbacks $q3 "$HVX_EVID/q3_b_sync_noflags.json" "$HVX_EVID/q3_b_redraw_noflags.json" \
    "$HVX_EVID/q3_b_sync_frames.json" "$HVX_EVID/q3_c_sync_frames.json"

LINES_B=$(wc -l < "$HVX_EVID/b.jsonl")
hvx_nested dev drift on frames awake > "$HVX_EVID/drift_awake.reply" || hvx_die_blocked "dev drift on frames awake rejected"
q4=PASS
grep -q '"last_states": \[[^]]*\b9\b' "$HVX_EVID/q4_b_states_stock.json" || q4=FAIL
check wait-states "$HVX_EVID/b.jsonl" "$LINES_B" 0 2 > "$HVX_EVID/q4_b_awake.json" || q4=FAIL
answer Q4-suspended $q4 "$HVX_EVID/q4_b_states_stock.json" "$HVX_EVID/q4_b_awake.json"

# ---- Q2 hit-test, Q6 focus -----------------------------------------------------------------------------------------
hvx_vpointer_start vp
steps() { # NAME ; commands from stdin, events of a, b, c
    cat > "$HVX_EVID/$1.cmds"
    hvx_pointer_run "$1.steps" vp "$HVX_EVID/$1.cmds" a b c
}
hvx_capture ref a
hctl -j activeworkspace > "$HVX_EVID/q6_before_hover.activeworkspace.json"
# Screen (796,220) -> anchor world (2388,660) -> minus R(2)=(1280,0) -> B layout (1108,660) -> B local (148,100).
printf 'move 796 220\n' | steps hover_b
check local "$HVX_EVID/hover_b.steps.json" 0 b 148 100 $PTOL > "$HVX_EVID/q2_hover_b_local.json" && q2=PASS || q2=FAIL
hctl -j activeworkspace > "$HVX_EVID/q6_after_hover.activeworkspace.json"
hctl -j activewindow > "$HVX_EVID/q6_after_hover.activewindow.json"
hctl -j cursorpos > "$HVX_EVID/q6_after_hover.cursorpos.json"
hvx_capture after_hover b
hvx_analyze q6_after_hover_diff diff "$HVX_EVID/after_hover.png" --reference "$HVX_EVID/ref.png" --max-pixels 0 && q6=PASS || q6=FAIL
# Screen (1000,100) -> anchor world (3000,300) -> minus R(3)=(2560,0) -> C local (300,200).
printf 'move 1000 100\nclick\n' | steps click_c
check local "$HVX_EVID/click_c.steps.json" 0 c 300 200 $PTOL > "$HVX_EVID/q2_click_c_local.json" || q2=FAIL
hctl -j activeworkspace > "$HVX_EVID/q6_after_click_c.activeworkspace.json"
hctl -j activewindow > "$HVX_EVID/q6_after_click_c.activewindow.json"
hctl -j cursorpos > "$HVX_EVID/q6_after_click_c.cursorpos.json"
hvx_capture after_click_c c
hvx_analyze q6_after_click_c_diff diff "$HVX_EVID/after_click_c.png" --reference "$HVX_EVID/ref.png" --max-pixels 0 || q6=FAIL
# Screen (146,100) -> world (438,300) -> A local (298,200).
printf 'move 146 100\nclick\n' | steps click_a
check local "$HVX_EVID/click_a.steps.json" 0 a 298 200 $PTOL > "$HVX_EVID/q2_click_a_local.json" || q2=FAIL
hctl -j activeworkspace > "$HVX_EVID/q6_after_click_a.activeworkspace.json"
hctl -j cursorpos > "$HVX_EVID/q6_after_click_a.cursorpos.json"
hvx_capture after_click_a a
hvx_analyze q6_after_click_a_diff diff "$HVX_EVID/after_click_a.png" --reference "$HVX_EVID/ref.png" --max-pixels 0 || q6=FAIL
python3 -I - "$HVX_EVID" > "$HVX_EVID/q6_focus.json" << 'PY' || q6=FAIL
import json, sys
e = sys.argv[1]
j = lambda n: json.load(open(f"{e}/{n}"))
res = {"hover_b_workspace": j("q6_after_hover.activeworkspace.json")["id"], "hover_b_window": j("q6_after_hover.activewindow.json").get("title"),
       "click_c_workspace": j("q6_after_click_c.activeworkspace.json")["id"], "click_c_window": j("q6_after_click_c.activewindow.json").get("title"),
       "click_a_workspace": j("q6_after_click_a.activeworkspace.json")["id"],
       "cursor": {k: j(f"q6_after_{k}.cursorpos.json") for k in ("hover", "click_c", "click_a")}}
want_cursor = {"hover": (796, 220), "click_c": (1000, 100), "click_a": (146, 100)}
warped = {k: v for k, v in res["cursor"].items() if (round(v["x"]), round(v["y"])) != want_cursor[k]}
res["cursor_warped"] = warped
ok = res["click_c_workspace"] == 3 and res["click_c_window"] == "c" and res["click_a_workspace"] == 1 and not warped
print(json.dumps(res))
sys.exit(0 if ok else 1)
PY
answer Q2-hit-test $q2 "$HVX_EVID/q2_hover_b_local.json" "$HVX_EVID/q2_click_c_local.json" "$HVX_EVID/q2_click_a_local.json"
answer Q6-focus-workspace $q6 "$HVX_EVID/q6_focus.json" "$HVX_EVID/q6_after_hover_diff.json" "$HVX_EVID/q6_after_click_c_diff.json" \
    "$HVX_EVID/q6_after_click_a_diff.json"

# ---- Q7 workspace animation ----------------------------------------------------------------------------------------
hctl keyword animations:enabled 1 > "$HVX_EVID/q7_anim.reply"
hctl keyword animation workspaces,1,10,default,slide >> "$HVX_EVID/q7_anim.reply"
anim_captures() { # PREFIX: three captures right after a workspace switch; elapsed ms per capture
    local t0 i
    t0=$(now_ns)
    for i in 1 2 3; do
        hvx_nested capture "$HVX_EVID/$1_$i.png" || hvx_die_blocked "capture $1_$i failed"
        echo "$1_$i $(( ($(now_ns) - t0) / 1000000 ))" >> "$HVX_EVID/q7_times.txt"
    done
}
q7=PASS
printf 'move 796 220\nclick\n' | steps anim_click_b
anim_captures q7_drift
for i in 1 2 3; do
    hvx_analyze "q7_drift_${i}_diff" diff "$HVX_EVID/q7_drift_$i.png" --reference "$HVX_EVID/ref.png" --max-pixels 0 || q7=FAIL
done
sleep 1.5
printf 'move 146 100\nclick\n' | steps anim_click_a
sleep 1.5
# Control without drift: the same switch (dispatch, workspace 1 -> 2) is animated at the same capture times.
T_DRIFT_OFF=$(now_ns)
hvx_nested dev drift off > "$HVX_EVID/drift_off.reply" || hvx_die_blocked "dev drift off rejected"
sleep 0.3
hvx_nested capture "$HVX_EVID/q7_ctl_static_ws1.png"
hctl dispatch workspace 2 > "$HVX_EVID/q7_ctl.reply"
anim_captures q7_ctl
sleep 1.5
hvx_nested capture "$HVX_EVID/q7_ctl_static_ws2.png"
ctl_moving=0
for i in 1 2 3; do
    if ! hvx_analyze "q7_ctl_${i}_vs_ws1" diff "$HVX_EVID/q7_ctl_$i.png" --reference "$HVX_EVID/q7_ctl_static_ws1.png" --max-pixels 0 \
        && ! hvx_analyze "q7_ctl_${i}_vs_ws2" diff "$HVX_EVID/q7_ctl_$i.png" --reference "$HVX_EVID/q7_ctl_static_ws2.png" --max-pixels 0; then
        ctl_moving=$((ctl_moving + 1))
    fi
done
echo "{\"control_captures_mid_animation\": $ctl_moving}" > "$HVX_EVID/q7_control.json"
((ctl_moving > 0)) || q7=BLOCKED
hctl dispatch workspace 1 >> "$HVX_EVID/q7_ctl.reply"
hctl keyword animations:enabled 0 >> "$HVX_EVID/q7_anim.reply"
answer Q7-animation $q7 "$HVX_EVID/q7_times.txt" "$HVX_EVID/q7_control.json" "$HVX_EVID/q7_drift_1_diff.json" \
    "$HVX_EVID/q7_drift_2_diff.json" "$HVX_EVID/q7_drift_3_diff.json"

# ---- Q5 output enter/leave, Q8 ownership ---------------------------------------------------------------------------
q5=PASS
for c in a b c; do
    check outputs "$HVX_EVID/$c.jsonl" > "$HVX_EVID/q5_$c.json"
    python3 -I -c "import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if d['output_enter'] >= 1 and d['output_leave'] == 0 else 1)" \
        "$HVX_EVID/q5_$c.json" || q5=FAIL
done
answer Q5-surface-enter $q5 "$HVX_EVID/q5_a.json" "$HVX_EVID/q5_b.json" "$HVX_EVID/q5_c.json"

sleep 1.5
hvx_ready a
hctl -j clients > "$HVX_EVID/clients_after.json"
q8=PASS
check clients "$HVX_EVID/clients_before.json" "$HVX_EVID/clients_after.json" > "$HVX_EVID/q8_clients.json" || q8=FAIL
check events "$HVX_EVID/events.log" "$T_DRIFT_ON" "$T_DRIFT_OFF" > "$HVX_EVID/q8_events.json" || q8=FAIL
hvx_capture off a
hvx_analyze q8_off_vs_nodrift diff "$HVX_EVID/off.png" --reference "$HVX_EVID/cam_nodrift.png" --max-pixels 0 || q8=FAIL
hvx_nested dev panic > /dev/null
hvx_capture panic a
hvx_analyze q8_panic_vs_base diff "$HVX_EVID/panic.png" --reference "$HVX_EVID/base.png" --max-pixels 0 || q8=FAIL
answer Q8-ownership $q8 "$HVX_EVID/q8_clients.json" "$HVX_EVID/q8_events.json" "$HVX_EVID/q8_off_vs_nodrift.json" \
    "$HVX_EVID/q8_panic_vs_base.json"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
HVX_NOT_RUN=("Caelestia active-workspace display (no shell in nested; socket2 workspace events recorded instead)"
    "U-5 stale pixels while panning (the spike damages the whole output in drift)" "popups/subsurfaces of drift windows"
    "render cost with many windows" "XWayland windows" "multi-monitor")
hvx_finish "$overall" "U0 unified-world feasibility spike (DV3-12, throwaway)" "patch 0010 on 0001-0009; scale 1; camera P=(0,0) z=1/3"
[[ $overall == PASS ]]
