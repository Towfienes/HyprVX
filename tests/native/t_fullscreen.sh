#!/usr/bin/env bash
# T-FULLSCREEN (Feature 002 US3, R7, V-18, T043). Scene: floating clients a and b (b mapped last, focused).
#  Reference: a stock session of the scene, `dispatch fullscreen` (b fullscreen): capture stock-fs.
#  Form session: Drift on, zoom 0.5 at (0,0), world offset b (150,80) (dev control): capture drift. `fullscreen`: the
#  capture equals stock-fs (0 px; camera and offsets ignored while the workspace has a fullscreen window). `fullscreen`
#  again: the capture equals drift (0 px; the Drift view is restored).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"

hvx_init_run "$(hvx_form_label)" t_fullscreen
hvx_copy_build_info stock
result=PASS
fail() { echo "$*" >&2; echo "$*" >> "$HVX_EVID/failures.txt"; result=FAIL; }

hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/stock.session.log" 2>&1 \
    || { cat "$HVX_EVID/stock.session.log" >&2; hvx_die_blocked "stock session did not start"; }
HVX_OWN_SESSION=1
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_hyprctl stock dispatch fullscreen > "$HVX_EVID/stock-fs.reply" 2>&1 || true
hvx_capture stock-fs b
hvx_hyprctl stock -j clients > "$HVX_EVID/stock-fs.clients.json"
for w in a b; do mv "$HVX_EVID/$w.jsonl" "$HVX_EVID/stock.$w.jsonl"; done
hvx_stop >> "$HVX_EVID/stock.session.log" 2>&1 || hvx_die_blocked "stock session did not stop"

hvx_start_form --policy --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "form $HVX_FORM session did not start"; }
hvx_client_start a 300x200
hvx_client_start b 300x200 --app-id hyprvx-testclient-b
hvx_vpointer_start vp
printf 'move 0 0\n' > "$HVX_EVID/home.cmds"
hvx_pointer_run home.steps vp "$HVX_EVID/home.cmds" a b
hvx_dispatch hyprvx:on > "$HVX_EVID/on.reply" 2>&1 || true
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true
hvx_nested dev offset "$(hvx_window_address hyprvx-testclient-b)" 150 80 > /dev/null || hvx_die_blocked "dev offset rejected"
hvx_capture drift b
hvx_differs drift-validity "$HVX_EVID/drift.png" "$HVX_EVID/stock-fs.png" || hvx_die_blocked "Drift view equals the fullscreen image (setup invalid)"
hvx_dispatch fullscreen > "$HVX_EVID/fs.reply" 2>&1 || true
hvx_capture fs b
hvx_hyprctl_form -j clients > "$HVX_EVID/fs.clients.json"
hvx_analyze fs-vs-stock diff "$HVX_EVID/fs.png" --reference "$HVX_EVID/stock-fs.png" --max-pixels 0 \
    || fail "fullscreen in Drift is not drawn as stock fullscreen"
hvx_dispatch fullscreen > "$HVX_EVID/unfs.reply" 2>&1 || true
hvx_capture unfs b
hvx_analyze unfs-vs-drift diff "$HVX_EVID/unfs.png" --reference "$HVX_EVID/drift.png" --max-pixels 0 \
    || fail "leaving fullscreen did not restore the Drift view"

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
hvx_finish "$result" "T-FULLSCREEN fullscreen in Drift = stock fullscreen; leaving restores the Drift view (V-18)" \
    "form $HVX_FORM; zoom 0.5 at (0,0); offset b (150,80); output $SIZE scale $SCALE"
[[ $result == PASS ]]
