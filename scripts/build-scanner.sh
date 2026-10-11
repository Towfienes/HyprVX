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
    *) echo 'Usage: build-scanner.sh [--dry-run]' >&2; exit 2 ;;
esac
[[ $# == 0 ]] || { echo 'Unexpected arguments.' >&2; exit 2; }

readonly cache="$HOME/.cache/hyprvx/$HYPRVX_AGENT"
readonly src="$cache/src/hyprwayland-scanner-v0.4.6"
readonly build="$cache/build/scanner-v0.4.6"
readonly prefix="$cache/tools"
run() {
    if "$dry_run"; then printf 'DRY RUN:'; printf ' %q' "$@"; printf '\n'; else "$@"; fi
}

for directory in "$src" "$build" "$prefix"; do
    [[ ! -L "$directory" ]] || { echo 'BLOCKED: scanner directory is a symlink.' >&2; exit 1; }
done
if [[ ! -e "$src" ]]; then
    run mkdir -p "${src%/*}"
    run git clone --depth 1 --branch v0.4.6 https://github.com/hyprwm/hyprwayland-scanner "$src"
elif [[ ! -d "$src/.git" ]]; then
    echo 'BLOCKED: scanner source is not the expected clone.' >&2; exit 1
fi
if ! "$dry_run"; then
    [[ $(git -C "$src" rev-parse HEAD) == $(git -C "$src" rev-parse 'v0.4.6^{commit}') &&
       $(<"$src/VERSION") == 0.4.6 && -z $(git -C "$src" status --porcelain) ]] || {
        echo 'BLOCKED: scanner source is not clean v0.4.6.' >&2; exit 1;
    }
    [[ ! -e "$build" ]] || { echo 'BLOCKED: scanner requires a fresh build directory.' >&2; exit 1; }
fi
run cmake -S "$src" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_INSTALL_PREFIX=$prefix" -DCMAKE_INSTALL_LIBDIR=lib
run cmake --build "$build" -j2
run cmake --install "$build"
printf 'Scanner v0.4.6 prefix: %s\n' "$prefix"
