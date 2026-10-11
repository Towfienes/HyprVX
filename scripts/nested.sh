#!/usr/bin/env bash
# Nested Hyprland harness entry point (contracts/test-tooling.md section 7); see tools/hyprvx_nested.py.
# Subcommands: start --variant stock|v2|patched [--size WxH] [--scale S] | run [--background] -- CMD... |
#              dev COMMAND [ARGS...] | capture PATH | status | stop | recover
# Exit codes: 0 ok, 1 command failed, 2 usage, 3 identity verification failed (nothing sent).
# The heavy lock is held by flock around the nested compositor, from start until it exits (stop).
set -euo pipefail
case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 -I "$repo/tools/hyprvx_nested.py" "$@"
