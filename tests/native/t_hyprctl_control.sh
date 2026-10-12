#!/usr/bin/env bash
# T-HYPRCTL-CONTROL (host trial): the delivered form S plugin (no dev control) loaded into the stock nested build is
# controlled through `hyprctl hyprvx on|off|toggle|panic`, the path a host with a Lua config has (its hl.dispatch cannot
# reach the hyprvx:* dispatchers). Pixel evidence: after `on`, a `hyprvx:zoom 0.5` (legacy nested config) changes the
# capture; after `off` the capture equals the reference; `toggle` twice does the same; `panic` returns identity;
# `hyprctl hyprvx bogus` is refused; `hyprctl hyprvx-dev state` is still an unknown request (FR-016). Replies are
# bookkeeping only (Constitution II); `hyprctl clients` must not change.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
HVX_FORM=S
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
SO="$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin/S/hyprvx-drift-S.so"

hvx_init_run formS t_hyprctl_control
hvx_copy_build_info stock
[[ -f $SO ]] || hvx_die_blocked "no plugin at $SO; run scripts/build-plugin.sh --form S"
hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/session.log" 2>&1 \
    || { cat "$HVX_EVID/session.log" >&2; hvx_die_blocked "nested stock session did not start"; }
HVX_OWN_SESSION=1
SESSION_DIR=$(hvx_session_dir)
hvx_client_start client 800x600
hvx_plugin_load "$SO" || hvx_die_blocked "plugin load refused (see plugin-load.log)"
hvx_capture reference client
hvx_clients_snapshot reference
result=PASS
ctl() { # LABEL ARGS...: hyprctl hyprvx ARGS, reply kept
    local label=$1
    shift
    hvx_hyprctl stock hyprvx "$@" > "$HVX_EVID/$label.reply" 2>&1 || true
}
expect_reply() { # LABEL TEXT
    grep -qxF "$2" "$HVX_EVID/$1.reply" || { echo "$1: reply '$(cat "$HVX_EVID/$1.reply")', expected '$2'" >&2; result=FAIL; }
}
same_as_reference() { # LABEL
    hvx_capture "$1" client
    hvx_analyze "$1-diff" diff "$HVX_EVID/$1.png" --reference "$HVX_EVID/reference.png" --max-pixels 0 \
        || { echo "$1: capture differs from the reference" >&2; result=FAIL; }
}
zoomed() { # LABEL: zoom out and require a changed capture
    hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/$1-zoom.reply" 2>&1 || true
    hvx_capture "$1" client
    hvx_differs "$1-differs" "$HVX_EVID/$1.png" "$HVX_EVID/reference.png" || { echo "$1: zoom left the image unchanged" >&2; result=FAIL; }
}

# Drift is off: a zoom is refused and nothing changes.
hvx_dispatch hyprvx:zoom 0.5 > "$HVX_EVID/off-zoom.reply" 2>&1 || true
same_as_reference c0-off
# on -> zoom changes the capture; off -> reference again.
ctl c1-on on
expect_reply c1-on ok
zoomed c1-zoomed
ctl c2-off off
expect_reply c2-off ok
same_as_reference c2-off
# toggle twice.
ctl c3-toggle toggle
expect_reply c3-toggle ok
zoomed c3-zoomed
ctl c4-toggle toggle
expect_reply c4-toggle ok
same_as_reference c4-toggle
# panic from DRIFT.
ctl c5-on on
zoomed c5-zoomed
ctl c5-panic panic
expect_reply c5-panic ok
same_as_reference c5-panic
# refusals.
ctl c6-bogus bogus
expect_reply c6-bogus err:invalid
hvx_hyprctl stock hyprvx-dev state > "$HVX_EVID/c7-dev.reply" 2>&1 || true
grep -q "unknown request" "$HVX_EVID/c7-dev.reply" || { echo "c7: hyprvx-dev answered: $(cat "$HVX_EVID/c7-dev.reply")" >&2; result=FAIL; }
hvx_clients_snapshot final
cmp -s "$HVX_EVID/reference.clients.json" "$HVX_EVID/final.clients.json" || { echo "window model changed" >&2; result=FAIL; }

hvx_plugin_unload "$SO" >> "$HVX_EVID/plugin-load.log" 2>&1 || { echo "unload failed" >&2; result=FAIL; }
hvx_stop >> "$HVX_EVID/session.log" 2>&1 || hvx_die_blocked "session did not stop"
cp "$SESSION_DIR/hyprland.log" "$HVX_EVID/nested-hyprland.log" 2> /dev/null || hvx_die_blocked "nested log not kept"
hvx_finish "$result" "T-HYPRCTL-CONTROL hyprctl hyprvx on/off/toggle/panic with the delivered form S plugin" "output $SIZE scale $SCALE"
[[ $result == PASS ]]
