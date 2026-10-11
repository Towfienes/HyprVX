#!/usr/bin/env bash
# T-MONITOR (Feature 002 data-model "monitor removed", V-11, T029). Scene: floating clients a, b, c (c focused).
#  1. In Drift with a non-identity camera and offsets (dev control), the scene output HVX-1 is removed
#     (`hyprctl output remove`): the compositor stays alive (hyprctl answers, session verified).
#  2. HVX-1 is re-added (`hyprctl output create headless HVX-1`): Drift stays OFF (Super+Escape reaches c) and the
#     capture equals the capture before Drift (identity camera, offsets 0).
#  3. Plugin unload while in Drift (camera and offsets set) acts as panic: compositor alive, capture = pre.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_monitor
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
alive() { # NAME
    hvx_hyprctl_form version > "$HVX_EVID/$1.version.txt" 2>&1 && hvx_nested status > "$HVX_EVID/$1.status.txt" 2>&1
}
drift_state() {
    hvx_dispatch hyprvx:on > /dev/null 2>&1 || true
    hvx_nested dev camera -150 60 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
    hvx_nested dev offset "$ADDR_A" 250 120 > /dev/null || hvx_die_blocked "dev offset rejected"
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
SESSION_DIR=$(hvx_session_dir)
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_vkeyboard_start kb
ADDR_A=$(hvx_window_address hyprvx-testclient)
hvx_capture pre c

# 1. remove the scene output while in Drift
drift_state
hvx_capture drift c
hvx_differs drift-validity "$HVX_EVID/drift.png" "$HVX_EVID/pre.png" || hvx_die_blocked "Drift state not visible (setup invalid)"
hvx_hyprctl_form output remove HVX-1 > "$HVX_EVID/remove.reply" 2>&1 || true
hvx_hyprctl_form -j monitors > "$HVX_EVID/removed.monitors.json" 2>&1 || true
alive removed || fail "compositor not alive after the scene output was removed"
python3 -I -c "
import json, sys
sys.exit(0 if all(m['name'] != 'HVX-1' for m in json.load(open(sys.argv[1]))) else 1)" "$HVX_EVID/removed.monitors.json" \
    || hvx_die_blocked "HVX-1 still listed after output remove (setup invalid)"

# 2. re-add: Drift stays OFF, identity image
hvx_hyprctl_form output create headless HVX-1 > "$HVX_EVID/readd.reply" 2>&1 || true
deadline=$(($(date +%s) + 10))
until hvx_hyprctl_form -j monitors 2> /dev/null | grep -q '"name": "HVX-1"'; do
    (($(date +%s) < deadline)) || hvx_die_blocked "HVX-1 did not come back"
    sleep 0.1
done
hvx_hyprctl_form -j monitors > "$HVX_EVID/readded.monitors.json"
hvx_hyprctl_form -j clients > "$HVX_EVID/readded.clients.json"
hvx_capture readded c
hvx_analyze readded-pre diff "$HVX_EVID/readded.png" --reference "$HVX_EVID/pre.png" --max-pixels 0 \
    || fail "after re-adding HVX-1 the image is not the identity image before Drift"
hvx_drift_off_probe kb c || fail "Drift on after the output came back (Super+Escape consumed)"
alive readded || fail "compositor not alive after re-adding HVX-1"

# 3. plugin unload while in Drift
drift_state
hvx_capture drift2 c
hvx_differs drift2-validity "$HVX_EVID/drift2.png" "$HVX_EVID/pre.png" || hvx_die_blocked "Drift state 2 not visible (setup invalid)"
hvx_plugin_unload > "$HVX_EVID/unload.reply" 2>&1 || fail "plugin unload failed"
alive unloaded || fail "compositor not alive after plugin unload in Drift"
hvx_capture unloaded c
hvx_analyze unloaded-pre diff "$HVX_EVID/unloaded.png" --reference "$HVX_EVID/pre.png" --max-pixels 0 \
    || fail "plugin unload in Drift did not act as panic (image differs from identity)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
hvx_finish "$result" "T-MONITOR output removed in Drift: alive, OFF; re-add stays OFF; unload in Drift acts as panic (V-11)" \
    "form $HVX_FORM; dev camera (-150,60) z 0.5, dev offset a (250,120); output $SIZE scale $SCALE"
[[ $result == PASS ]]
