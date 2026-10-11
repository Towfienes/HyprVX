#!/usr/bin/env bash
# FR-016 check (Feature 002 T057): the dev control exists only in test builds.
#  Static: the dev command table's keywords ("camera-mon", "full-damage", exact strings) are absent from the delivered
#  plugin form S (plugin/S) and from the patched build without the dev control (prefix <patchset>-nodev, build variant
#  patched-nodev), and form S has no HyprVXSpike::registerDevControl symbol; all present in plugin/S-dev and the dev
#  patched build (positive controls, so the check is not vacuous). The name "hyprvx-dev" itself is not searched: the
#  compiler builds that 10-byte std::string from immediates (no literal in .rodata), as the first T057 run found.
#  Runtime: `hyprctl hyprvx-dev state` is rejected in a stock session with plugin/S loaded and in a patched-nodev
#  session with plugin/P loaded, while the policy answers (`hyprctl hyprvx state`) and Drift enters (Super+Escape is
#  consumed afterwards, the Drift-is-on probe); the dev patched session answers `hyprvx-dev state` (control).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
PLUG="$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin"
PATCHED=$(hvx_prefix_id patched)
NODEV="$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$PATCHED-nodev"
DEVBIN="$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$PATCHED/bin/Hyprland"

hvx_init_run patched t_fr016
result=PASS
fail() { echo "$*" >&2; echo "FAIL $*" >> "$HVX_EVID/checks.txt"; result=FAIL; }
ok() { echo "ok   $*" >> "$HVX_EVID/checks.txt"; }
[[ -x $NODEV/bin/Hyprland ]] || hvx_die_blocked "no patched-nodev build at $NODEV (scripts/build-hyprland.sh --variant patched-nodev)"
for f in "$PLUG/S/hyprvx-drift-S.so" "$PLUG/S-dev/hyprvx-drift-S.so" "$PLUG/P/hyprvx-drift-P.so" "$DEVBIN"; do
    [[ -f $f ]] || hvx_die_blocked "missing $f"
done
cp "$NODEV/build-info.json" "$HVX_EVID/build-info-patched-nodev.json"
hvx_copy_build_info patched
sha256sum "$PLUG/S/hyprvx-drift-S.so" "$PLUG/S-dev/hyprvx-drift-S.so" "$PLUG/P/hyprvx-drift-P.so" "$NODEV/bin/Hyprland" "$DEVBIN" > "$HVX_EVID/binaries.sha256"

# Static: number of dev command keywords (0..2) and of registerDevControl definitions in each binary
has() { strings -a "$2" | grep -cxE 'camera-mon|full-damage' > "$HVX_EVID/keywords-$1.txt" || true; cat "$HVX_EVID/keywords-$1.txt"; }
sym() { nm -C --defined-only "$2" | grep -c 'HyprVXSpike::registerDevControl()$' > "$HVX_EVID/symbol-$1.txt" || true; cat "$HVX_EVID/symbol-$1.txt"; }
[[ $(has plugin-S "$PLUG/S/hyprvx-drift-S.so") == 0 ]] && ok "plugin S: no dev command keywords" || fail "plugin S contains dev command keywords"
[[ $(sym plugin-S "$PLUG/S/hyprvx-drift-S.so") == 0 ]] && ok "plugin S: no registerDevControl" || fail "plugin S defines registerDevControl"
[[ $(has patched-nodev "$NODEV/bin/Hyprland") == 0 ]] && ok "patched-nodev: no dev command keywords" || fail "patched-nodev contains dev command keywords"
[[ $(has plugin-S-dev "$PLUG/S-dev/hyprvx-drift-S.so") == 2 ]] && ok "control: plugin S-dev has both keywords" || hvx_die_blocked "control: S-dev lacks the dev keywords (check vacuous)"
[[ $(sym plugin-S-dev "$PLUG/S-dev/hyprvx-drift-S.so") -gt 0 ]] && ok "control: plugin S-dev defines registerDevControl" || hvx_die_blocked "control: S-dev lacks registerDevControl (check vacuous)"
[[ $(has patched-dev "$DEVBIN") == 2 ]] && ok "control: dev patched build has both keywords" || hvx_die_blocked "control: dev build lacks the dev keywords (check vacuous)"

# Runtime: stock + plugin S
hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/s.session.log" 2>&1 || hvx_die_blocked "stock session did not start"
HVX_OWN_SESSION=1
hvx_plugin_load "$PLUG/S/hyprvx-drift-S.so" || hvx_die_blocked "plugin S did not load"
hvx_hyprctl stock hyprvx-dev state > "$HVX_EVID/s.dev.reply" 2>&1 || true
hvx_hyprctl stock hyprvx state > "$HVX_EVID/s.policy.reply" 2>&1 || true
grep -q '"monitors"' "$HVX_EVID/s.dev.reply" && fail "stock + plugin S answers hyprvx-dev state" || ok "stock + plugin S rejects hyprvx-dev: $(head -c 80 "$HVX_EVID/s.dev.reply")"
grep -q '"monitors"' "$HVX_EVID/s.policy.reply" && ok "stock + plugin S: policy answers" || fail "stock + plugin S: policy does not answer"
hvx_stop >> "$HVX_EVID/s.session.log" 2>&1 || hvx_die_blocked "session did not stop"

# Runtime: patched-nodev + plugin P (the delivered form P)
hvx_nested start --variant patched --binary "$NODEV/bin/Hyprland" --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/p.session.log" 2>&1 \
    || { cat "$HVX_EVID/p.session.log" >&2; hvx_die_blocked "patched-nodev session did not start"; }
HVX_OWN_SESSION=1
hvx_plugin_load "$PLUG/P/hyprvx-drift-P.so" || fail "plugin P did not load into the patched-nodev build"
nodevctl() { hvx_nested run -- "$NODEV/bin/hyprctl" "$@"; }
nodevctl hyprvx-dev state > "$HVX_EVID/p.dev.reply" 2>&1 || true
nodevctl hyprvx state > "$HVX_EVID/p.policy.reply" 2>&1 || true
grep -q '"monitors"' "$HVX_EVID/p.dev.reply" && fail "patched-nodev answers hyprvx-dev state" || ok "patched-nodev rejects hyprvx-dev: $(head -c 80 "$HVX_EVID/p.dev.reply")"
grep -q '"monitors"' "$HVX_EVID/p.policy.reply" && ok "patched-nodev + plugin P: policy answers" || fail "patched-nodev + plugin P: policy does not answer"
hvx_client_start client 300x200
hvx_vkeyboard_start kb
nodevctl dispatch hyprvx:on > "$HVX_EVID/p.on.reply" 2>&1 || true
hvx_drift_off_probe kb client && fail "patched-nodev + plugin P: Drift did not enter (Super+Escape reached the client)" \
    || ok "patched-nodev + plugin P: Drift entered (Super+Escape consumed)"
hvx_stop >> "$HVX_EVID/p.session.log" 2>&1 || hvx_die_blocked "session did not stop"

# Control: the dev patched build answers
hvx_nested start --variant patched --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/d.session.log" 2>&1 || hvx_die_blocked "dev patched session did not start"
HVX_OWN_SESSION=1
hvx_nested dev state > "$HVX_EVID/d.dev.reply" 2>&1 || true
grep -q '"monitor"' "$HVX_EVID/d.dev.reply" && ok "control: dev patched build answers hyprvx-dev state" || hvx_die_blocked "control: dev build does not answer (check vacuous)"
hvx_stop >> "$HVX_EVID/d.session.log" 2>&1 || hvx_die_blocked "session did not stop"

cat "$HVX_EVID/checks.txt"
hvx_finish "$result" "FR-016 dev control absent from plugin S and the patched-nodev build; present in test builds" \
    "patchset $PATCHED; nodev prefix $PATCHED-nodev"
[[ $result == PASS ]]
