#!/usr/bin/env bash
set -euo pipefail

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
variant=stock
dry_run=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --variant) [[ $# -ge 2 ]] || exit 2; variant=$2; shift 2 ;;
        --dry-run) dry_run=true; shift ;;
        *) echo 'Usage: build-hyprland.sh [--variant stock|v2|patched|patched-nodev] [--dry-run]' >&2; exit 2 ;;
    esac
done
case $variant in stock|v2|patched|patched-nodev) ;; *) echo 'Unknown variant.' >&2; exit 2 ;; esac
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if ! "$dry_run"; then
    available=$(awk '$1 == "MemAvailable:" {print $2}' /proc/meminfo)
    [[ $available =~ ^[0-9]+$ ]] && (( available >= 6815744 )) || {
        echo "BLOCKED: MemAvailable=${available:-unknown} KiB; need 6815744 KiB (6.5 GiB)." >&2; exit 1;
    }
    exec 9< "$HOME/.cache/hyprvx/locks/heavy.lock"
    flock -w 3600 9 || { echo 'BLOCKED: heavy lock timeout.' >&2; exit 1; }
fi

python3 -I "$repo/tools/hyprvx_build.py" "$repo" "$variant" "$dry_run"
