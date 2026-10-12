#!/usr/bin/env bash
# Copies the host's Hyprland and hyprctl binaries (read only from /usr/bin) into an agent prefix so that a nested session
# can run the exact distribution build that the delivered form S plugin will meet on the host (host trial, 2026-10-12).
# Nothing on the host is written. The copy is labelled variant "stock" (unmodified upstream efb5099, checked below) with
# origin "host-copy", the package version, both sha256 sums and the running host's `hyprctl version -j` identity.
# Output: ~/.cache/hyprvx/<agent>/prefix/host-<package version>/{bin/Hyprland,bin/hyprctl,build-info.json}; the binary
# path is printed. From fish: env HYPRVX_AGENT=claude bash scripts/mirror-host-hyprland.sh
set -euo pipefail
agent=${HYPRVX_AGENT:?set HYPRVX_AGENT}
[[ $agent =~ ^[a-z]+$ ]] || { echo "bad HYPRVX_AGENT" >&2; exit 2; }
src=/usr/bin/Hyprland
pkg=$(pacman -Q hyprland 2> /dev/null | awk '{print $2}') || true
[[ -n $pkg ]] || { echo "BLOCKED: pacman package hyprland not found" >&2; exit 3; }
host_pid=$(pgrep -x Hyprland | head -1 || true)
running_sha=""
[[ -n $host_pid ]] && running_sha=$(sha256sum "/proc/$host_pid/exe" | awk '{print $1}')
version_json=$(hyprctl version -j)
commit=$(jq -r .commit <<< "$version_json")
dirty=$(jq -r .dirty <<< "$version_json")
[[ $commit == efb50993780079460b0cbed1363e2166a2de1d9f && $dirty == false ]] \
    || { echo "BLOCKED: host is not a clean efb5099 build (commit $commit, dirty $dirty)" >&2; exit 3; }
prefix="$HOME/.cache/hyprvx/$agent/prefix/host-$pkg"
mkdir -p "$prefix/bin"
install -m 0755 "$src" "$prefix/bin/Hyprland"
install -m 0755 /usr/bin/hyprctl "$prefix/bin/hyprctl"
sha=$(sha256sum "$prefix/bin/Hyprland" | awk '{print $1}')
[[ $sha == "$(sha256sum "$src" | awk '{print $1}')" ]] || { echo "copy mismatch" >&2; exit 1; }
python3 -I - "$prefix/build-info.json" "$sha" "$running_sha" "$pkg" "$version_json" <<'PY'
import json, sys
path, sha, running, pkg, version = sys.argv[1:]
info = {"variant": "stock", "origin": "host-copy", "source": "/usr/bin/Hyprland", "host_package": "hyprland " + pkg,
        "binary_sha256": sha, "host_running_binary_sha256": running, "running_binary_is_this_copy": running == sha,
        "patchset_id": "none", "git_dirty": "clean", "host_version": json.loads(version)}
open(path, "w").write(json.dumps(info, indent=1) + "\n")
PY
echo "$prefix/bin/Hyprland"
