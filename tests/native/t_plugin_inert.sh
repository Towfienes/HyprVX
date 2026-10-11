#!/usr/bin/env bash
# T-PLUGIN-INERT (Feature 002 T015): each plugin build of the form loads into its nested compositor and is inert while
# Drift is OFF. Form P: plugin/P into the patched build. Form S: plugin/S (delivered, no dev control) and plugin/S-dev
# (nested tests) into the stock build. For each plugin, in one session:
#  1. capture and `hyprctl clients` with no plugin loaded (reference);
#  2. load: capture byte-identical to the reference (diff, 0 pixels), window model unchanged;
#  3. unload (acts as panic): capture byte-identical again, window model unchanged, compositor alive.
# `hyprctl hyprvx state` is kept for bookkeeping only (Constitution II), never as evidence.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
PLUGDIR="$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin"

hvx_init_run "$(hvx_form_label)" t_plugin_inert
VARIANT=$(hvx_form_variant)
hvx_copy_build_info "$VARIANT"
if [[ $HVX_FORM == P ]]; then
    PLUGINS=("$PLUGDIR/P/hyprvx-drift-P.so")
else
    PLUGINS=("$PLUGDIR/S/hyprvx-drift-S.so" "$PLUGDIR/S-dev/hyprvx-drift-S.so")
fi
for so in "${PLUGINS[@]}"; do
    [[ -f $so ]] || hvx_die_blocked "no plugin at $so; run scripts/build-plugin.sh"
done

# The plugin is loaded by this test, not by hvx_start_form (form S would load S-dev at start).
hvx_nested start --variant "$VARIANT" --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested $VARIANT session did not start"; }
HVX_OWN_SESSION=1
SESSION_DIR=$(hvx_session_dir)
hvx_client_start client 800x600
hvx_client_start top 240x140 --layer TOP --position 20,640 --app-id hyprvx-layer
hvx_capture reference client
hvx_clients_snapshot reference
result=PASS
same_model() { # NAME
    cmp -s "$HVX_EVID/reference.clients.json" "$HVX_EVID/$1.clients.json" \
        || { echo "$1: window model changed" >&2; result=FAIL; }
}

i=0
for so in "${PLUGINS[@]}"; do
    i=$((i + 1))
    label="p$i-$(basename -- "$(dirname -- "$so")")"
    hvx_plugin_load "$so" || hvx_die_blocked "$label: plugin load refused (see plugin-load.log)"
    mv "$HVX_EVID/plugin.sha256" "$HVX_EVID/$label.plugin.sha256"
    [[ -f $HVX_EVID/build-info-plugin.json ]] && mv "$HVX_EVID/build-info-plugin.json" "$HVX_EVID/$label.build-info.json"
    hvx_hyprctl_form hyprvx state > "$HVX_EVID/$label.hyprvx-state.txt" 2>&1 || true
    hvx_capture "$label-loaded" client
    hvx_clients_snapshot "$label-loaded"
    hvx_analyze "$label-loaded-diff" diff "$HVX_EVID/$label-loaded.png" --reference "$HVX_EVID/reference.png" \
        --max-pixels 0 || { echo "$label: capture changed after load" >&2; result=FAIL; }
    same_model "$label-loaded"
    hvx_plugin_unload "$so" >> "$HVX_EVID/plugin-load.log" 2>&1 || { echo "$label: unload failed" >&2; result=FAIL; }
    hvx_capture "$label-unloaded" client
    hvx_clients_snapshot "$label-unloaded"
    hvx_analyze "$label-unloaded-diff" diff "$HVX_EVID/$label-unloaded.png" --reference "$HVX_EVID/reference.png" \
        --max-pixels 0 || { echo "$label: capture changed after unload" >&2; result=FAIL; }
    same_model "$label-unloaded"
done

hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
loads=$(grep -c "\[hyprvx\] Drift policy loaded" "$HVX_EVID/nested-hyprland.log" || true)
unloads=$(grep -c "\[hyprvx\] Drift policy unloaded" "$HVX_EVID/nested-hyprland.log" || true)
echo "policy loaded: $loads; unloaded: $unloads; expected ${#PLUGINS[@]} each" | tee "$HVX_EVID/plugin-lifecycle.txt"
[[ $loads == "${#PLUGINS[@]}" && $unloads == "${#PLUGINS[@]}" ]] || result=FAIL
hvx_finish "$result" "T-PLUGIN-INERT plugin load/unload inert with Drift OFF (T015)" \
    "form $HVX_FORM; plugins: ${PLUGINS[*]##*/plugin/}; output $SIZE scale $SCALE"
[[ $result == PASS ]]
