#!/usr/bin/env bash
# T-FR020 (Feature 003 FR-020, V-18, T040): the test-only input plugin is never part of a delivery form.
#  Static: the delivered policy plugins (plugin/P, plugin/S) contain no "hvxtest" string (exact line of `strings -a`);
#  both test plugins (plugin/testinput-P, plugin/testinput-S) do (positive controls, so the check is not vacuous).
#  Runtime: `hyprctl hvxtest frames HVX-1` gets Hyprland's "unknown request" in a stock session with only plugin/S
#  loaded and in a patched-nodev session with only plugin/P loaded, while the policy answers (`hyprctl hyprvx state`);
#  with plugin/testinput-S also loaded into the stock session the same request answers a frame count (control).
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SIZE="${HVX_OUTPUT_SIZE:-1280x800}"
SCALE="${HVX_OUTPUT_SCALE:-1}"
PLUG="$HOME/.cache/hyprvx/$HYPRVX_AGENT/plugin"
PATCHED=$(hvx_prefix_id patched)
NODEV="$HOME/.cache/hyprvx/$HYPRVX_AGENT/prefix/$PATCHED-nodev"

hvx_init_run patched t_fr020
result=PASS
fail() { echo "$*" >&2; echo "FAIL $*" >> "$HVX_EVID/checks.txt"; result=FAIL; }
ok() { echo "ok   $*" >> "$HVX_EVID/checks.txt"; }
[[ -x $NODEV/bin/Hyprland ]] || hvx_die_blocked "no patched-nodev build at $NODEV (scripts/build-hyprland.sh --variant patched-nodev)"
P_SO="$PLUG/P/hyprvx-drift-P.so"; S_SO="$PLUG/S/hyprvx-drift-S.so"
TP_SO="$PLUG/testinput-P/hyprvx-testinput-P.so"; TS_SO="$PLUG/testinput-S/hyprvx-testinput-S.so"
for f in "$P_SO" "$S_SO" "$TP_SO" "$TS_SO"; do [[ -f $f ]] || hvx_die_blocked "missing $f"; done
cp "$NODEV/build-info.json" "$HVX_EVID/build-info-patched-nodev.json"
for d in P S testinput-P testinput-S; do cp "$PLUG/$d/build-info.json" "$HVX_EVID/build-info-$d.json"; done
sha256sum "$P_SO" "$S_SO" "$TP_SO" "$TS_SO" "$NODEV/bin/Hyprland" > "$HVX_EVID/binaries.sha256"

# Static
has() { strings -a "$2" | grep -cx 'hvxtest' > "$HVX_EVID/hvxtest-$1.txt" || true; cat "$HVX_EVID/hvxtest-$1.txt"; }
[[ $(has plugin-P "$P_SO") == 0 ]] && ok "plugin P: no hvxtest string" || fail "plugin P contains hvxtest"
[[ $(has plugin-S "$S_SO") == 0 ]] && ok "plugin S: no hvxtest string" || fail "plugin S contains hvxtest"
[[ $(has testinput-P "$TP_SO") -gt 0 ]] && ok "control: testinput-P has hvxtest" || hvx_die_blocked "control: testinput-P lacks hvxtest (check vacuous)"
[[ $(has testinput-S "$TS_SO") -gt 0 ]] && ok "control: testinput-S has hvxtest" || hvx_die_blocked "control: testinput-S lacks hvxtest (check vacuous)"

# Runtime: stock + plugin S alone, then + testinput-S (control)
hvx_nested start --variant stock --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/s.session.log" 2>&1 || hvx_die_blocked "stock session did not start"
HVX_OWN_SESSION=1
hvx_plugin_load "$S_SO" || hvx_die_blocked "plugin S did not load"
hvx_hyprctl stock hvxtest frames HVX-1 > "$HVX_EVID/s.hvxtest.reply" 2>&1 || true
hvx_hyprctl stock hyprvx state > "$HVX_EVID/s.policy.reply" 2>&1 || true
[[ $(cat "$HVX_EVID/s.hvxtest.reply") == "unknown request" ]] && ok "stock + plugin S rejects hvxtest (unknown request)" \
    || fail "stock + plugin S does not reject hvxtest: $(head -c 120 "$HVX_EVID/s.hvxtest.reply")"
grep -q '"monitors"' "$HVX_EVID/s.policy.reply" && ok "stock + plugin S: policy answers" || fail "stock + plugin S: policy does not answer"
hvx_nested plugin load "$TS_SO" > "$HVX_EVID/s.testinput-load.log" 2>&1 || hvx_die_blocked "control: testinput-S did not load"
hvx_hyprctl stock hvxtest frames HVX-1 > "$HVX_EVID/s.control.reply" 2>&1 || true
grep -qxE '[0-9]+' "$HVX_EVID/s.control.reply" && ok "control: with testinput-S loaded, hvxtest answers a frame count" \
    || hvx_die_blocked "control: hvxtest does not answer with testinput-S loaded (check vacuous): $(head -c 120 "$HVX_EVID/s.control.reply")"
hvx_stop >> "$HVX_EVID/s.session.log" 2>&1 || hvx_die_blocked "session did not stop"

# Runtime: patched-nodev + plugin P (the delivered form P)
hvx_nested start --variant patched --binary "$NODEV/bin/Hyprland" --size "$SIZE" --scale "$SCALE" > "$HVX_EVID/p.session.log" 2>&1 \
    || { cat "$HVX_EVID/p.session.log" >&2; hvx_die_blocked "patched-nodev session did not start"; }
HVX_OWN_SESSION=1
hvx_plugin_load "$P_SO" || fail "plugin P did not load into the patched-nodev build"
nodevctl() { hvx_nested run -- "$NODEV/bin/hyprctl" "$@"; }
nodevctl hvxtest frames HVX-1 > "$HVX_EVID/p.hvxtest.reply" 2>&1 || true
nodevctl hyprvx state > "$HVX_EVID/p.policy.reply" 2>&1 || true
[[ $(cat "$HVX_EVID/p.hvxtest.reply") == "unknown request" ]] && ok "patched-nodev + plugin P rejects hvxtest (unknown request)" \
    || fail "patched-nodev + plugin P does not reject hvxtest: $(head -c 120 "$HVX_EVID/p.hvxtest.reply")"
grep -q '"monitors"' "$HVX_EVID/p.policy.reply" && ok "patched-nodev + plugin P: policy answers" || fail "patched-nodev + plugin P: policy does not answer"
hvx_stop >> "$HVX_EVID/p.session.log" 2>&1 || hvx_die_blocked "session did not stop"

cat "$HVX_EVID/checks.txt"
hvx_finish "$result" "FR-020 the test-only input plugin is absent from both delivery forms; present only in test builds" \
    "patchset $PATCHED; nodev prefix $PATCHED-nodev"
[[ $result == PASS ]]
