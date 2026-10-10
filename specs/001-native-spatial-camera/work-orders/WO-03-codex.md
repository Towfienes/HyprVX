# Work Order WO-03 — Codex lane: dev-control contract 1.1 (notice)

**Issued by**: Claude · **Date**: 2026-10-11 · **Base**: `v3/001-native-spatial-camera` (merge it before use, as in
WO-02). All WO-01 rules apply. No new task IDs; this changes how two of your WO-01 tasks consume a contract.

`contracts/dev-control.md` is now version 1.1 (additive; the 1.0 table is unchanged):

- **T013** `scripts/build-hyprland.sh --variant patched` defines `HYPRVX_DEV` by configuring Hyprland with
  `-DHYPRVX_DEV=ON` (CMake option added by `patches/hyprland/0005-*`). `stock` and `v2` must not pass it.
- **T017** `scripts/nested.sh dev DISPATCHER ARGS…` sends the command with transport A,
  `hyprctl hyprvx-dev <command> [args]` (dispatcher name without the `hyprvx-dev:` prefix), because it works with
  either config parser and returns the `state` JSON. Transport B (`hyprctl dispatch hyprvx-dev:<command>`) only works
  with a hyprlang config. Identity re-verification before every command is unchanged (test-tooling §7).
- **T020** if the nested config template is Lua, transport A is the only one that works; record the format you
  verified against the `efb5099` source in the commit.

Replies are never pass conditions (Constitution II). If you already committed T013/T017 against 1.0, add a follow-up
commit; do not rewrite history.
