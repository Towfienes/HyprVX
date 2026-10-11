#!/usr/bin/env bash
# Delivery spike stability checks (T067, V-21 input). Requires HVX_SPIKE_PLUGIN (scripts/build-plugin-spike.sh output).
#  A. Load refusal: the plugin loaded into another Hyprland build (the V2 prefix) must be refused and the compositor
#     must stay alive and verified.
#  B. Unload under a camera: with the camera at z = 0.5 the plugin is unloaded; the compositor must stay alive and the
#     next capture must equal the identity capture taken before the camera was set (hooks removed, stock path).
#  C. Informational: loading into the patched build (same upstream commit) is recorded, not judged.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
[[ -n ${HVX_SPIKE_PLUGIN:-} && -f ${HVX_SPIKE_PLUGIN:-} ]] || { echo "BLOCKED: set HVX_SPIKE_PLUGIN to the built plugin" >&2; exit 2; }
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
PREFIX="$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix"

hvx_init_run patched spike_stability
hvx_copy_build_info patched
result=PASS

# A. Refusal by another build.
hvx_nested start --variant v2 --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/a-session.log" 2>&1 || hvx_die_blocked "v2 session did not start"
HVX_OWN_SESSION=1
v2ctl=$(ls "$PREFIX"/v2-*/bin/hyprctl | head -1)
hvx_nested run -- "$v2ctl" plugin load "$HVX_SPIKE_PLUGIN" > "$HVX_EVID/a-load.reply" 2>&1 || true
hvx_nested status > "$HVX_EVID/a-status.log" 2>&1 || { echo "A: compositor not alive after the refused load" >&2; result=FAIL; }
grep -q '^ok$' "$HVX_EVID/a-load.reply" && { echo "A: plugin was loaded into the V2 build" >&2; result=FAIL; }
hvx_stop >> "$HVX_EVID/a-session.log" 2>&1 || hvx_die_blocked "v2 session did not stop"
echo "A refusal: $(head -c 300 "$HVX_EVID/a-load.reply")" | tee "$HVX_EVID/a.result"

# B. Unload under a camera.
hvx_start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/b-session.log" 2>&1 \
    || { cat "$HVX_EVID/b-session.log" >&2; hvx_die_blocked "spike session did not start"; }
hvx_client_start client 800x600
hvx_nested dev panic > /dev/null || hvx_die_blocked "panic failed"
hvx_capture b-identity client
hvx_nested dev camera 0 0 0.5 > /dev/null || hvx_die_blocked "dev camera rejected"
hvx_capture b-camera client
hvx_nested run -- "$PREFIX/stock/bin/hyprctl" plugin unload "$HVX_SPIKE_PLUGIN" > "$HVX_EVID/b-unload.reply" 2>&1 || true
hvx_nested status > "$HVX_EVID/b-status.log" 2>&1 || { echo "B: compositor not alive after unload" >&2; result=FAIL; }
hvx_capture b-after-unload client
cmp -s "$HVX_EVID/b-identity.png" "$HVX_EVID/b-camera.png" && { echo "B: camera capture equals identity (setup invalid)" >&2; hvx_die_blocked "camera had no effect"; }
cmp -s "$HVX_EVID/b-identity.png" "$HVX_EVID/b-after-unload.png" || { echo "B: capture after unload differs from identity" >&2; result=FAIL; }
hvx_stop >> "$HVX_EVID/b-session.log" 2>&1 || { echo "B: session did not stop cleanly" >&2; result=FAIL; }
echo "B unload: reply=$(cat "$HVX_EVID/b-unload.reply") after-unload==identity: $(cmp -s "$HVX_EVID/b-identity.png" "$HVX_EVID/b-after-unload.png" && echo yes || echo no)" \
    | tee "$HVX_EVID/b.result"

# C. Informational: the patched build.
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/c-session.log" 2>&1 || hvx_die_blocked "patched session did not start"
HVX_OWN_SESSION=1
hvx_nested run -- "$PREFIX/$(HVX_SPIKE_PLUGIN='' hvx_prefix_id patched)/bin/hyprctl" plugin load "$HVX_SPIKE_PLUGIN" > "$HVX_EVID/c-load.reply" 2>&1 || true
hvx_nested status > "$HVX_EVID/c-status.log" 2>&1 || echo "C: compositor not alive after the load" >&2
hvx_stop >> "$HVX_EVID/c-session.log" 2>&1 || true
echo "C patched build (informational): $(head -c 300 "$HVX_EVID/c-load.reply")" | tee "$HVX_EVID/c.result"

hvx_finish "$result" "Delivery spike stability: refusal by another build, unload under a camera (T067)" \
    "plugin $(cut -c1-12 "$HVX_EVID/plugin.sha256"); output $SIZE scale $SCALE"
[[ $result == PASS ]]
