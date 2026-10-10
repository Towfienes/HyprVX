# Integration Log — Feature 001

Append-only. Owner: Claude (master plan §O1). One row per handoff, integration, contract change, ownership transfer
or gate decision. Evidence rules: `contracts/test-tooling.md` §8 — a task is ticked only after its evidence run is
archived under `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` and `sha256sum -c SHA256SUMS` passes.

## Frozen interfaces (K0, T001 — 2026-10-11)

| Contract | Version | Frozen at |
|---|---|---|
| `contracts/transform-contract.md` | 1.1 | `966e6b6` |
| `contracts/test-tooling.md` | 1.0 | `966e6b6` |
| `contracts/dev-control.md` | (unversioned, unchanged) | `966e6b6` |

Changes after this point require a version bump and a row below before any consumer uses them.

## Log

| Date | Task | Agent | Branch@sha | Verify command | Result | Evidence run-id | Archive path | `sha256sum -c` | Review verdict | Ownership transfer / notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 2026-10-11 | T001 | claude | v3/001-native-spatial-camera@966e6b6 | `grep "Contract version" contracts/*.md` | PASS | — (documentation task, no runtime evidence) | — | — | self | K0 freeze recorded above |
| 2026-10-11 | T002 | claude | v3/001-native-spatial-camera@76b97aa | `git -C ../HyprVX-v3-codex branch --show-current; git -C ../HyprVX-v3-codex remote -v; stat -c %h .git/objects/pack/*.pack; flock -n heavy.lock true` | PASS | — (setup task; outputs in this row) | — | — | self | Codex clone `/home/towfienes/Projects/HyprVX-v3-codex` (standalone `--no-hardlinks` clone, not a worktree; branch `v3/001-codex` from `76b97aa`; push URL `DISABLED`; pack link count 1). Caches `~/.cache/hyprvx/{claude,codex,locks}`; lock `~/.cache/hyprvx/locks/heavy.lock` mode 0444, `flock` OK; archive `~/.local/share/hyprvx/evidence/{claude,codex}`. Exclusive file ownership per master plan §O1 now in force |
| 2026-10-11 | WO-01 | claude | v3/001-native-spatial-camera | — | ISSUED | — | — | — | — | Codex batch 1 (T003–T006, T010–T014) and 1b (T015–T021, no T009/T022): `work-orders/WO-01-codex.md` |
| 2026-10-11 | T007, T008 | claude | v3/001-native-spatial-camera@e94ffd2 | `cmake -S . -B ~/.cache/hyprvx/claude/build/core-e94ffd2 -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja -j2 && ctest -j2`; `hyprvx-sim` output `cmp` vs `ab6647c`; scratch self-check `selfcheck.cpp` (in the run) | PASS | `20261011-003807-core-e94ffd2` | `~/.local/share/hyprvx/evidence/claude/20261011-003807-core-e94ffd2/` | OK (12 files) | self (independent verification is T009, Codex) | ctest 298/298 (baseline 298/298); sim byte-identical; 200 000 random cases bit-exact vs V1 formulas incl. anchor zoom; identity exact with non-zero `M`; header clean under g++/clang++ C++20/23 `-Werror`. Degenerate-input differences from V1 (untested by any suite): non-finite anchor leaves position unchanged (V1: NaN); subnormal zoom clamped to `DBL_MIN` in the anchor math. T009 is now unblocked for WO-02 |
| 2026-10-11 | WO-02 | claude | v3/001-native-spatial-camera@494e22d | — | ISSUED | — | — | — | — | Codex T009 (after its T004–T006): `work-orders/WO-02-codex.md` |
