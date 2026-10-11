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
# Hyprland's CMake fetches glaze v7.2.0 with FetchContent; the build disables Git discovery for truthful provenance,
# so glaze is pinned here and handed to CMake as FETCHCONTENT_SOURCE_DIR_GLAZE.
readonly glaze_tag=v7.2.0
readonly glaze_pin=b518eec7a22e56ffa238b072c07f47efa7cea97f
readonly glaze_src="$HOME/.cache/hyprvx/$HYPRVX_AGENT/src/glaze-$glaze_tag"
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

[[ ! -L "$glaze_src" ]] || { echo 'BLOCKED: glaze directory is a symlink.' >&2; exit 1; }
if [[ ! -e "$glaze_src" ]]; then
    run git clone --depth 1 --branch "$glaze_tag" https://github.com/stephenberry/glaze.git "$glaze_src"
elif [[ ! -d "$glaze_src/.git" ]]; then
    echo 'BLOCKED: existing glaze directory is not the expected clone.' >&2; exit 1
fi
if "$dry_run"; then
    printf 'Expected glaze HEAD: %s\n' "$glaze_pin"
else
    [[ $(git -C "$glaze_src" rev-parse HEAD) == "$glaze_pin" && -z $(git -C "$glaze_src" status --porcelain) ]] || {
        echo 'BLOCKED: glaze checkout is not clean at the pin.' >&2; exit 1;
    }
fi
printf 'Source: %s\nglaze: %s\n' "$src" "$glaze_src"
