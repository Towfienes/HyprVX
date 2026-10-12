#!/usr/bin/env bash
# T-FORM-S-GUARD (host trial regression): the delivered form S plugin must not run Drift on a build that is not
# unmodified upstream efb5099. The HyprVX patched build (GIT_DIRTY "hyprvx-<patchset>", commit of the patched tree,
# another plugin ABI hash) is such a build: either Hyprland refuses the load (ABI hash) or the plugin refuses to enter
# Drift. Pixel evidence: after `hyprctl hyprvx on` and `hyprvx:zoom 0.5` the capture equals the reference.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SO="$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin/S/hyprvx-drift-S.so"

hvx_init_run patched t_form_s_guard
hvx_copy_build_info patched
[[ -f $SO ]] || hvx_die_blocked "no plugin at $SO; run scripts/build-plugin.sh --form S"
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested patched session did not start"; }
HVX_OWN_SESSION=1
SESSION_DIR=$(hvx_session_dir)
hvx_client_start client 800x600
hvx_capture reference client
result=PASS
loaded=yes
hvx_plugin_load "$SO" > "$HVX_EVID/load.out" 2>&1 || loaded=no
echo "plugin loaded: $loaded" | tee "$HVX_EVID/load-result.txt"
hvx_hyprctl patched hyprvx on > "$HVX_EVID/on.reply" 2>&1 || true
[[ $(cat "$HVX_EVID/on.reply") != ok ]] || { echo "hyprvx on answered ok on a patched build" >&2; result=FAIL; }
hvx_hyprctl patched dispatch hyprvx:zoom 0.5 > "$HVX_EVID/zoom.reply" 2>&1 || true
hvx_capture after client
hvx_analyze after-diff diff "$HVX_EVID/after.png" --reference "$HVX_EVID/reference.png" --max-pixels 0 \
    || { echo "capture changed: Drift ran on a patched build" >&2; result=FAIL; }
[[ $loaded == yes ]] && { hvx_plugin_unload "$SO" >> "$HVX_EVID/plugin-load.log" 2>&1 || true; }
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
grep -E "\[hyprvx\].*(unavailable|refus)|plugin.*(hash|version)" "$HVX_EVID/nested-hyprland.log" > "$HVX_EVID/refusal-lines.txt" || true
hvx_finish "$result" "T-FORM-S-GUARD form S refused on the HyprVX patched build" "plugin loaded: $loaded; output $SIZE scale $SCALE"
[[ $result == PASS ]]
