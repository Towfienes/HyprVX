#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
variant=patched
dry_run=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --variant) [[ $# -ge 2 ]] || exit 2; variant=$2; shift 2 ;;
        --dry-run) dry_run=true; shift ;;
        *) echo 'Usage: apply-patches.sh [--variant stock|v2|patched] [--dry-run]' >&2; exit 2 ;;
    esac
done
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
readonly pin=efb50993780079460b0cbed1363e2166a2de1d9f
readonly src="$HOME/.cache/hyprvx/$HYPRVX_AGENT/src/hyprland-efb5099"
patches=()
shopt -s nullglob
case $variant in
    stock) worktree="$src/stock" ;;
    v2) worktree="$src/v2"; patches=("$repo/research/v2-drift-mvp/"*.patch) ;;
    patched) worktree="$src/v3-dev"; patches=("$repo/patches/hyprland/"*.patch) ;;
    *) echo 'Unknown variant.' >&2; exit 2 ;;
esac
patchset_id=none
if [[ $variant != stock ]]; then
    [[ ${#patches[@]} -gt 0 ]] || { echo 'BLOCKED: no patches for this variant.' >&2; exit 1; }
    patchset_id=$(cat -- "${patches[@]}" | sha256sum)
    patchset_id=${patchset_id:0:12}
fi
run() {
    if "$dry_run"; then printf 'DRY RUN:'; printf ' %q' "$@"; printf '\n'; else "$@"; fi
}

if ! "$dry_run"; then
    [[ ! -L "$worktree" && -e "$worktree/.git" ]] || {
        echo 'BLOCKED: fetch the agent worktrees first.' >&2; exit 1;
    }
    [[ $(git -C "$worktree" rev-parse --path-format=absolute --git-common-dir) == "$src/.git" &&
       $(git -C "$worktree" rev-parse HEAD) == "$pin" &&
       -z $(git -C "$worktree" status --porcelain) ]] || {
        echo 'BLOCKED: patching requires a clean, pinned agent worktree.' >&2; exit 1;
    }
fi
if [[ $variant != stock ]]; then
    run git -C "$worktree" switch -c "hyprvx/0.56.2-$variant"
    run git -C "$worktree" am -- "${patches[@]}"
fi
readonly receipt="${src%/*}/hyprland-$variant-patchset.json"
if "$dry_run"; then
    printf 'DRY RUN: record verified patchset and applied HEAD in %s\n' "$receipt"
else
    applied_head=$(git -C "$worktree" rev-parse HEAD)
    python3 -I - "$receipt" "$patchset_id" "$applied_head" <<'PY'
import json
import os
from pathlib import Path
import sys
import tempfile

path = Path(sys.argv[1])
fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
try:
    with os.fdopen(fd, "w") as output:
        json.dump({"patchset_id": sys.argv[2], "head": sys.argv[3]}, output)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())
    os.replace(temporary, path)
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
PY
fi
printf 'patchset_id=%s\n' "$patchset_id"
