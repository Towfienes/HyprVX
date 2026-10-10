#!/usr/bin/env bash
set -euo pipefail

case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
dry_run=false
case ${1:-} in
    '') ;;
    --dry-run) dry_run=true; shift ;;
    *) echo 'Usage: fetch-hyprland.sh [--dry-run]' >&2; exit 2 ;;
esac
[[ $# == 0 ]] || { echo 'Unexpected arguments.' >&2; exit 2; }

readonly pin=efb50993780079460b0cbed1363e2166a2de1d9f
readonly src="$HOME/.cache/hyprvx/$HYPRVX_AGENT/src/hyprland-efb5099"
run() {
    if "$dry_run"; then printf 'DRY RUN:'; printf ' %q' "$@"; printf '\n'; else "$@"; fi
}

[[ ! -L "$src" ]] || { echo 'BLOCKED: source directory is a symlink.' >&2; exit 1; }
if [[ ! -e "$src" ]]; then
    run mkdir -p "${src%/*}"
    run git clone --branch v0.56.2 --recurse-submodules https://github.com/hyprwm/Hyprland "$src"
elif [[ ! -d "$src/.git" ]]; then
    echo 'BLOCKED: existing source directory is not the expected clone.' >&2; exit 1
fi
if "$dry_run"; then
    run git -C "$src" rev-parse HEAD
    printf 'Expected upstream HEAD: %s\n' "$pin"
else
    [[ $(git -C "$src" rev-parse HEAD) == "$pin" ]] || {
        echo 'BLOCKED: upstream HEAD differs from the pin.' >&2; exit 1;
    }
    [[ -z $(git -C "$src" status --porcelain --untracked-files=no) ]] || {
        echo 'BLOCKED: upstream checkout has changes.' >&2; exit 1;
    }
fi

for variant in stock v2 v3-dev; do
    worktree="$src/$variant"
    [[ ! -L "$worktree" ]] || { echo 'BLOCKED: worktree is a symlink.' >&2; exit 1; }
    if [[ ! -e "$worktree" ]]; then
        run git -C "$src" worktree add --detach "$worktree" "$pin"
    elif ! "$dry_run"; then
        [[ $(git -C "$worktree" rev-parse --path-format=absolute --git-common-dir) == "$src/.git" ]] || {
            echo 'BLOCKED: variant is not a worktree of this agent clone.' >&2; exit 1;
        }
        git -C "$worktree" merge-base --is-ancestor "$pin" HEAD || {
            echo 'BLOCKED: variant does not descend from the pinned upstream.' >&2; exit 1;
        }
    fi
    run git -C "$worktree" submodule update --init --recursive
done
printf 'Source: %s\n' "$src"
