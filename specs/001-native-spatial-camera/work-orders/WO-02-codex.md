# Work Order WO-02 — Codex lane, Sprint 1: T009

**Issued by**: Claude · **Date**: 2026-10-11
**Base**: `v3/001-native-spatial-camera@494e22d` (T007/T008 integrated). All WO-01 rules (§0 ownership, §1 hard
rules, §5 handoff) apply unchanged.
**Order**: run after your WO-01 batch 1 items T004–T006 (T009 registers its target in `CMakeLists.txt`, which T005
also edits). It may run before or after batch 1b. It does not lift the stop point before T022.

## Before you start
`git fetch origin && git merge --ff-only origin/v3/001-native-spatial-camera` is no longer possible once you have
your own commits. Use `git merge --no-ff origin/v3/001-native-spatial-camera` instead (needs the user's approval
because the sandbox keeps `.git` read-only). Never rebase or force.

## T009 — `tests/unit/test_camera_math.cpp`
Write it from `contracts/transform-contract.md` v1.1 (invariants 1–7), not from the header's code:
- ≥ 10⁴ seeded random cases per property; print the seed and the failing inputs on failure; the default seed is
  fixed so CI runs are deterministic, overridable by the environment variable `HYPRVX_TEST_SEED`.
- Expected values from an independent oracle (`long double` arithmetic and hand-computed dyadic values). The oracle
  lives in the test file only.
- Invariant 4 with non-zero `M` (for example `M = {1920, 0}`, `W = {0.1, …}`) must compare with `==`, not a tolerance.
- Invariant 5: every `Error` value is reachable; `NonFinite` takes precedence over zoom checks.
- Invariant 6: for random world boxes, `damageBox` contains `forwardBox` and its edges are integers.
- Invariant 7: `hyprvx::Camera{P,z}.worldToScreen(W) == forward({P,z}, {0,0}, W)` and the same for
  `screenToWorld`/`inverse` and `setZoomAroundScreenPoint`/`anchorZoom`, compared with `==`.
- `anchorZoom`: clamping to `ZoomLimits`, and non-finite `a` or `zNew` returns the camera unchanged.
- Register the file in `hyprvx_core_tests` (or a new target discovered by `gtest_discover_tests`) in `CMakeLists.txt`.

Verify: headless configure (T005 options OFF), `ninja -j2`, `ctest -j2 --output-on-failure`, plus one run with a
second seed. Evidence run `<YYYYMMDD-HHMMSS>-unit-<short-sha>` with the logs and both seeds. A failure caused by the
header is a contract question: report it with inputs and expected/actual values; do not change the test to match
the header and never edit `CameraMath.hpp`.

Context: Claude's own scratch check (run `20261011-003807-core-e94ffd2`) is a self-check, not a substitute for this
task; T009 is the independent verification (V-06, V-25).
