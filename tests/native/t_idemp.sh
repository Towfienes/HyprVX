#!/usr/bin/env bash
# T-IDEMP (Feature 002 data-model idempotence, V-10, T028). Scene: floating clients a, b, c (c focused).
#  1. hyprvx:on, then a dev camera (probe state); a second hyprvx:on must not touch it: capture on2 = on1.
#  2. hyprvx:off: capture off1 = pre; a second hyprvx:off: off2 = off1; hyprvx:panic while OFF: panic = off1; Drift OFF
#     (Super+Escape reaches c).
#  3. Special workspace: togglespecialworkspace (empty special workspace shown), hyprvx:on, capture unchanged; close the
#     special workspace: Drift is OFF (Super+Escape reaches c), i.e. on did not enter Drift.
# Replies are recorded, never judged (Constitution II).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_idemp
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }
same() { # NAME IMAGE REFERENCE
    hvx_analyze "$1" diff "$HVX_EVID/$2.png" --reference "$HVX_EVID/$3.png" --max-pixels 0 || fail "$1: $2 differs from $3"
}

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_client_start c 300x200 --app-id hyprvx-testclient-c
hvx_vkeyboard_start kb
hvx_capture pre c

# 1. on twice
hvx_dispatch hyprvx:on > "$HVX_EVID/on1.reply" 2>&1 || true
hvx_nested dev camera 150 80 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
hvx_capture on1 c
hvx_differs on1-validity "$HVX_EVID/on1.png" "$HVX_EVID/pre.png" || hvx_die_blocked "probe camera not visible (setup invalid)"
hvx_dispatch hyprvx:on > "$HVX_EVID/on2.reply" 2>&1 || true
hvx_capture on2 c
same on2-same on2 on1

# 2. off twice, panic while OFF
hvx_dispatch hyprvx:off > "$HVX_EVID/off1.reply" 2>&1 || true
hvx_capture off1 c
same off1-pre off1 pre
hvx_dispatch hyprvx:off > "$HVX_EVID/off2.reply" 2>&1 || true
hvx_capture off2 c
same off2-same off2 off1
hvx_dispatch hyprvx:panic > "$HVX_EVID/panic-off.reply" 2>&1 || true
hvx_capture panic-off c
same panic-off-same panic-off off1
hvx_drift_off_probe kb c || fail "Drift not OFF after off, off, panic (Super+Escape consumed)"

# 3. special workspace
hvx_dispatch togglespecialworkspace > "$HVX_EVID/special-open.reply" 2>&1 || true
hvx_hyprctl_form -j monitors > "$HVX_EVID/special-open.monitors.json"
python3 -I -c "
import json, sys
m = json.load(open(sys.argv[1]))[0]
sys.exit(0 if m.get('specialWorkspace', {}).get('id', 0) != 0 else 1)" "$HVX_EVID/special-open.monitors.json" \
    || hvx_die_blocked "special workspace did not open (setup invalid)"
hvx_capture special0 c
hvx_dispatch hyprvx:on > "$HVX_EVID/special-on.reply" 2>&1 || true
hvx_capture special1 c
same special-on-unchanged special1 special0
hvx_dispatch togglespecialworkspace > "$HVX_EVID/special-close.reply" 2>&1 || true
hvx_capture special-closed c
same special-closed-pre special-closed pre
hvx_drift_off_probe kb c || fail "hyprvx:on on a special workspace entered Drift (Super+Escape consumed after closing it)"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-IDEMP on/off twice, panic while OFF, special workspace keeps Drift OFF (V-10)" \
    "form $HVX_FORM; probe dev camera (150,80) z 0.5; output $SIZE scale $SCALE"
[[ $result == PASS ]]
