#!/usr/bin/env bash
# V-26 host-safety audit (Feature 002 T058; same checks as Feature 001 V-22). Read-only. For every retired nested
# session marker (/tmp/hyprvx-nested-<uid>-<agent>/session-*/session.json) newer than SINCE:
#   - its instance signature differs from the host signature it recorded;
#   - its runtime directory is a private /tmp/hvx-* directory;
#   - its compositor binary is under the agent prefix (~/.cache/hyprvx/<agent>/prefix/);
#   - every child it started is an agent tool (~/.cache/hyprvx/<agent>/...) or python3 running a repository script.
# Host side: no file under ~/.config/hypr or ~/.config/caelestia changed since SINCE; the hyprland package version is
# recorded. Repository code: every `hyprctl ... plugin load|unload` in tests/native and scripts runs through
# `hvx_nested` (scripts/nested.sh), never against the host.
#   HVX_AUDIT_SINCE="2026-10-11 19:00" (default: Feature 002 implementation start)
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/lib.sh"
SINCE="${HVX_AUDIT_SINCE:-2026-10-11 19:00}"

hvx_init_run audit v26
result=PASS
python3 -I - "$SINCE" "$HOME/.cache/hyprvx/$HYPRVX_AGENT" "$HVX_REPO" "/tmp/hyprvx-nested-$(id -u)-$HYPRVX_AGENT" \
    > "$HVX_EVID/nested_sessions.json" << 'PY' || result=FAIL
import datetime, json, sys
from pathlib import Path
since, cache, repo, root = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]), Path(sys.argv[4])
t0 = datetime.datetime.strptime(since, "%Y-%m-%d %H:%M").timestamp()
report = {"since": since, "sessions": 0, "nested_equals_host": 0, "non_private_runtime": [], "foreign_binary": [],
          "foreign_child": [], "variants": set(), "binaries": set(), "host_signatures_seen": set()}
for marker in sorted(root.glob("session-*/session.json")):
    if marker.stat().st_mtime < t0:
        continue
    d = json.loads(marker.read_text())
    report["sessions"] += 1
    report["variants"].add(d.get("variant"))
    report["binaries"].add(d.get("binary"))
    report["host_signatures_seen"].add(d.get("host_signature"))
    if d.get("signature") and d.get("signature") == d.get("host_signature"):
        report["nested_equals_host"] += 1
    rt = Path(d.get("runtime_directory", ""))
    if rt.parent != Path("/tmp") or not rt.name.startswith("hvx-"):
        report["non_private_runtime"].append(str(marker))
    if not str(d.get("binary", "")).startswith(str(cache / "prefix") + "/"):
        report["foreign_binary"].append(str(marker))
    for child in d.get("children", []):
        argv = child.get("argv", [])
        exe = argv[0] if argv else ""
        ok = exe.startswith(str(cache) + "/") or (Path(exe).name.startswith("python3") and any(a.startswith(str(repo) + "/") for a in argv))
        if not ok:
            report["foreign_child"].append({"marker": str(marker), "argv": argv[:3]})
for k in ("variants", "binaries", "host_signatures_seen"):
    report[k] = sorted(x for x in report[k] if x)
ok = (report["sessions"] > 0 and report["nested_equals_host"] == 0 and not report["non_private_runtime"]
      and not report["foreign_binary"] and not report["foreign_child"])
report["verdict"] = "PASS" if ok else "FAIL"
print(json.dumps(report, indent=1))
sys.exit(0 if ok else 1)
PY
{
    echo "since: $SINCE"
    echo "Hyprland processes of this user (host and nested): $(pgrep -u "$(id -u)" -x Hyprland | tr '\n' ' ')"
    for dir in "$HOME/.config/hypr" "$HOME/.config/caelestia"; do
        n=$(find "$dir" -newermt "$SINCE" -type f 2> /dev/null | wc -l)
        echo "$dir files changed since $SINCE: $n"
        if [[ $n != 0 ]]; then
            find "$dir" -newermt "$SINCE" -type f
            echo "HOST CONFIG CHANGED"
        fi
    done
    pacman -Qi hyprland 2> /dev/null | grep -E '^(Version|Install Date)' || echo "pacman: hyprland package info unavailable"
} > "$HVX_EVID/host_state.txt"
grep -q "HOST CONFIG CHANGED" "$HVX_EVID/host_state.txt" && result=FAIL
grep -rnE "plugin (load|unload)" "$HVX_REPO/tests/native" "$HVX_REPO/scripts" 2> /dev/null \
    | grep -vE ':[0-9]+:[[:space:]]*#' > "$HVX_EVID/plugin_load_calls.txt" || true
grep 'hyprctl' "$HVX_EVID/plugin_load_calls.txt" | grep -v 'hvx_nested' > "$HVX_EVID/plugin_calls_outside_nested.txt" || true
if [[ -s $HVX_EVID/plugin_calls_outside_nested.txt ]]; then
    echo "plugin load/unload outside hvx_nested:" >&2
    cat "$HVX_EVID/plugin_calls_outside_nested.txt" >&2
    result=FAIL
fi
cat "$HVX_EVID/host_state.txt"
python3 -I -c "import json,sys; d=json.load(open(sys.argv[1])); print('sessions', d['sessions'], 'nested=host', d['nested_equals_host'], 'verdict', d['verdict'])" "$HVX_EVID/nested_sessions.json"
hvx_finish "$result" "V-26 host-safety audit of Feature 002 nested sessions since $SINCE" "read-only; session markers, host config mtimes, hyprland package, plugin calls in the repository"
[[ $result == PASS ]]
