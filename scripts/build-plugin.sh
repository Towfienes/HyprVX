#!/usr/bin/env bash
# Builds the Drift policy plugin hyprvx-drift (platform/plugin, Feature 002 T003) in a fresh build directory under the
# O5 rules (MemAvailable >= 6.5 GiB, heavy lock, systemd scope MemoryMax=6G, -j4).
#   --form P   against the patched prefix of this checkout's patch series (policy only; mechanism in the binary)
#   --form S   against the stock prefix (policy + mechanism hooks, DV3-16)
#   --dev      form S only: include the dev control for nested tests (output directory S-dev)
#   --testinput  build the test-only plugin hyprvx-testinput (tools/testinput, Feature 003 test-tooling 1.5 §8) for the
#              given form instead of hyprvx-drift; output directory testinput-<form>. Never part of a delivery form.
#   --dry-run  print the plan; build and lock nothing
# Output: ~/.cache/hyprvx/<agent>/plugin/<form>[-dev]/{hyprvx-drift-<form>.so,build-info.json} (or
# plugin/testinput-<form>/hyprvx-testinput-<form>.so with --testinput); the .so path is printed
# last. Nested-only: tests load it with `scripts/nested.sh plugin load`, never into the host compositor.
# From fish: env HYPRVX_AGENT=claude bash scripts/build-plugin.sh --form P
set -euo pipefail

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
usage() { echo 'Usage: build-plugin.sh --form P|S [--dev | --testinput] [--dry-run]' >&2; exit 2; }
form=
dev=false
testinput=false
dry_run=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --form) [[ $# -ge 2 ]] || usage; form=$2; shift 2 ;;
        --dev) dev=true; shift ;;
        --testinput) testinput=true; shift ;;
        --dry-run) dry_run=true; shift ;;
        *) usage ;;
    esac
done
case $form in P|S) ;; *) usage ;; esac
if "$dev" && [[ $form != S ]]; then
    echo 'BLOCKED: --dev applies to form S only (form P uses the HYPRVX_DEV patched build).' >&2
    exit 2
fi
if "$dev" && "$testinput"; then
    echo 'BLOCKED: --dev and --testinput are exclusive (the test plugin has no dev control).' >&2
    exit 2
fi
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly cache="$HOME/.cache/hyprvx/$HYPRVX_AGENT"
variant=$([[ $form == P ]] && echo patched || echo stock)
read -r prefix_id patchset_id < <(python3 -I -c "
import importlib.util, sys; from pathlib import Path
spec = importlib.util.spec_from_file_location('b', sys.argv[1] + '/tools/hyprvx_build.py'); b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b); i, p, _ = b.variant_identity(Path(sys.argv[1]), sys.argv[2]); print(i, p)" "$repo" "$variant")
readonly prefix="$cache/prefix/$prefix_id"
label=$form
"$dev" && label=$form-dev
if "$testinput"; then
    label=testinput-$form
    src_rel=tools/testinput
    artifact=hyprvx-testinput-$form.so
    cmake_args=(-DHYPRVX_TESTINPUT_FORM="$form" -DCMAKE_CXX_COMPILER=c++)
else
    src_rel=platform/plugin
    artifact=hyprvx-drift-$form.so
    cmake_args=(-DHYPRVX_PLUGIN_FORM="$form" -DHYPRVX_PLUGIN_DEV="$($dev && echo ON || echo OFF)"
                -DHYPRVX_PATCHSET_ID="$patchset_id" -DCMAKE_CXX_COMPILER=c++)
fi
readonly out="$cache/plugin/$label"

if "$dry_run"; then
    echo "DRY RUN: require MemAvailable >= 6.5 GiB"
    echo "DRY RUN: flock -w 3600 $HOME/.cache/hyprvx/locks/heavy.lock"
    echo "DRY RUN: prefix $prefix (variant $variant, patchset $patchset_id)"
    echo "DRY RUN: fresh build dir $cache/build/plugin-$label-XXXXXXXX"
    echo "DRY RUN: systemd-run --user --scope -p MemoryMax=6G -- cmake -S $repo/$src_rel ${cmake_args[*]} && cmake --build -j4"
    echo "DRY RUN: write $out/$artifact and $out/build-info.json"
    exit 0
fi

[[ -f $prefix/build-info.json && -f $prefix/share/pkgconfig/hyprland.pc ]] || {
    echo "BLOCKED: no $variant prefix at $prefix; run scripts/build-hyprland.sh --variant $variant" >&2; exit 1;
}
available=$(awk '$1 == "MemAvailable:" {print $2}' /proc/meminfo)
[[ $available =~ ^[0-9]+$ ]] && (( available >= 6815744 )) || {
    echo "BLOCKED: MemAvailable=${available:-unknown} KiB; need 6815744 KiB (6.5 GiB)." >&2; exit 1;
}
exec 9< "$HOME/.cache/hyprvx/locks/heavy.lock"
flock -w 3600 9 || { echo 'BLOCKED: heavy lock timeout.' >&2; exit 1; }

mkdir -p "$cache/build"
build=$(mktemp -d "$cache/build/plugin-$label-XXXXXXXX")
log="$build.log"
PKG_CONFIG_PATH="$prefix/share/pkgconfig" systemd-run --user --scope -q -p MemoryMax=6G -p MemorySwapMax=0 --setenv=PKG_CONFIG_PATH \
    bash -c 'set -e; src=$1; dir=$2; shift 2; cmake -S "$src" -B "$dir" "$@"; cmake --build "$dir" -j4' \
    _ "$repo/$src_rel" "$build" "${cmake_args[@]}" > "$log" 2>&1 \
    || { echo "plugin build failed; log: $log" >&2; exit 1; }

so="$build/$artifact"
mkdir -p "$out"
cp "$so" "$out/$artifact.tmp"
mv "$out/$artifact.tmp" "$out/$artifact"
python3 -I - "$repo" "$out" "$form" "$dev" "$prefix_id" "$patchset_id" "$build" "$prefix" "$artifact" "$src_rel" << 'PY'
import hashlib, json, os, subprocess, sys, tempfile
from pathlib import Path
repo, out, form, dev, prefix_id, patchset_id, build, prefix, artifact, src_rel = sys.argv[1:]
so = Path(out, artifact)
# Sources that go into the plugin: the plugin tree and the core headers/sources it compiles (the test plugin: its tree).
roots = ([Path(repo, src_rel)] if src_rel != "platform/plugin" else
         [Path(repo, "platform/plugin"), Path(repo, "core/include")]
         + [Path(repo, "core/src", f) for f in ("DriftModel.cpp", "GestureModel.cpp", "Camera.cpp", "CameraMotion.cpp")])
digest = hashlib.sha256()
for root in roots:
    files = [root] if root.is_file() else sorted(p for p in root.rglob("*") if p.is_file() and "__pycache__" not in p.parts)
    for f in files:
        digest.update(str(f.relative_to(repo)).encode() + b"\0" + f.read_bytes())
git = lambda *a: subprocess.run(["git", "-C", repo, *a], capture_output=True, text=True).stdout.strip()
info = {
    "artifact": so.name, "form": form, "dev_control": dev == "true", "prefix_id": prefix_id, "patchset_id": patchset_id,
    "plugin_sha256": hashlib.sha256(so.read_bytes()).hexdigest(), "sources_sha256": digest.hexdigest(),
    "git_sha": git("rev-parse", "HEAD"),
    "sources_dirty": bool(git("status", "--porcelain", "--", *[str(r.relative_to(repo)) for r in roots])),
    "build_directory": build,
    "compiler": subprocess.run(["c++", "--version"], capture_output=True, text=True).stdout.splitlines()[0],
    "prefix_build_info": json.loads(Path(prefix, "build-info.json").read_text()),
}
fd, tmp = tempfile.mkstemp(prefix="build-info.", dir=out)
with os.fdopen(fd, "w") as handle:
    json.dump(info, handle, indent=1)
    handle.write("\n")
os.replace(tmp, Path(out, "build-info.json"))
PY
echo "$out/$artifact"
