#!/usr/bin/env bash
# Builds the delivery spike plugin (platform/plugin-spike, T066) against the agent's stock-from-source prefix in a fresh
# build directory, under the O5 rules (MemAvailable >= 6.5 GiB, heavy lock, systemd scope MemoryMax=6G, -j4).
# Output: ~/.cache/hyprvx/<agent>/prefix/plugin-spike-<so sha256[:12]>/{hyprvx-plugin-spike.so,build-info.json}; the
# .so path is printed last. Nested-only: the plugin is loaded by tests/native (HVX_SPIKE_PLUGIN), never into the host.
set -euo pipefail

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly cache="$HOME/.cache/hyprvx/$HYPRVX_AGENT"
readonly stock="$cache/prefix/stock"
[[ -x $stock/bin/Hyprland && -f $stock/share/pkgconfig/hyprland.pc ]] || {
    echo "BLOCKED: no stock prefix at $stock; run scripts/build-hyprland.sh --variant stock" >&2; exit 1;
}

available=$(awk '$1 == "MemAvailable:" {print $2}' /proc/meminfo)
[[ $available =~ ^[0-9]+$ ]] && (( available >= 6815744 )) || {
    echo "BLOCKED: MemAvailable=${available:-unknown} KiB; need 6815744 KiB (6.5 GiB)." >&2; exit 1;
}
exec 9< "$HOME/.cache/hyprvx/locks/heavy.lock"
flock -w 3600 9 || { echo 'BLOCKED: heavy lock timeout.' >&2; exit 1; }

build=$(mktemp -d "$cache/build/plugin-spike-XXXXXXXX")
log="$build.log"
PKG_CONFIG_PATH="$stock/share/pkgconfig" systemd-run --user --scope -q -p MemoryMax=6G -p MemorySwapMax=0 --setenv=PKG_CONFIG_PATH \
    bash -c 'cmake -S "$1/platform/plugin-spike" -B "$2" -DCMAKE_CXX_COMPILER=c++ && cmake --build "$2" -j4' _ "$repo" "$build" > "$log" 2>&1 \
    || { echo "plugin build failed; log: $log" >&2; exit 1; }

so="$build/hyprvx-plugin-spike.so"
sha=$(sha256sum "$so" | cut -d' ' -f1)
out="$cache/prefix/plugin-spike-${sha:0:12}"
mkdir -p "$out"
cp "$so" "$out/hyprvx-plugin-spike.so"
python3 -I - "$repo" "$out" "$sha" "$build" "$stock" << 'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
repo, out, sha, build, stock = sys.argv[1:]
src = Path(repo, "platform/plugin-spike")
digest = hashlib.sha256()
for f in sorted(p for p in src.rglob("*") if p.is_file()):
    digest.update(str(f.relative_to(src)).encode() + b"\0" + f.read_bytes())
git = lambda *a: subprocess.run(["git", "-C", repo, *a], capture_output=True, text=True).stdout.strip()
json.dump({
    "artifact": "hyprvx-plugin-spike.so", "so_sha256": sha, "git_commit": git("rev-parse", "HEAD"),
    "plugin_sources_dirty": bool(git("status", "--porcelain", "--", "platform/plugin-spike", "core/include")),
    "plugin_sources_sha256": digest.hexdigest(), "build_directory": build,
    "compiler": subprocess.run(["c++", "--version"], capture_output=True, text=True).stdout.splitlines()[0],
    "stock_build_info": json.load(open(Path(stock, "build-info.json"))),
}, open(Path(out, "build-info.json"), "w"), indent=1)
PY
echo "$out/hyprvx-plugin-spike.so"
