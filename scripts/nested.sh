#!/usr/bin/env bash
# Nested Hyprland harness entry point (contracts/test-tooling.md section 7); see tools/hyprvx_nested.py.
# Subcommands: start --variant stock|v2|patched [--size WxH] [--scale S] [--interactive] | run [--background] [--x11] -- CMD... |
#              dev COMMAND [ARGS...] | plugin load|unload PATH | xdisplay | capture PATH | status | stop | recover
# (plugin, xdisplay and run --x11: Feature 002 test-tooling 1.4 section 7)
# Exit codes: 0 ok, 1 command failed, 2 usage, 3 identity verification failed (nothing sent).
# The heavy lock is held by flock around the nested compositor, from start until it exits (stop).
set -euo pipefail
case ${HYPRVX_AGENT:-} in
    claude|codex) ;;
    *) echo 'Set HYPRVX_AGENT to claude or codex.' >&2; exit 2 ;;
esac
readonly repo="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 -I "$repo/tools/hyprvx_nested.py" "$@"
