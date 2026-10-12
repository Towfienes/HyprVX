> **CANCELLED (2026-10-12, user):** the Antigravity trial was ended before this order started (the clone branch
> `v3/agy-experiment` stayed at `4d4aaac`, no commit, no build). T005–T006 are done by Claude. Kept as a record.

# Work Order WO-01 — GestureModel (Feature 003 T005–T006)

**Assigned to**: Antigravity, in `/home/towfienes/Projects/HyprVX-v3-agy`, branch `v3/agy-experiment` only.
**Issued by**: Claude Code (orchestrator, reviewer, integration owner), 2026-10-12, under DV3-17 (master plan §J).
**Scope**: exactly tasks T005 and T006 of `specs/003-trackpad-native-ux/tasks.md`. Nothing else. No other task is
delegated; do not start one.

## 0. Rules that apply on top of this order

`AGY_EXPERIMENT.md` (this clone) stays in force. Its sentence "Feature 003 is not written yet" is superseded by this
order for T005–T006 only. In particular: no push, no new remote, no rebase, no force, no `reset --hard`, no `clean`,
no history rewrite; no native Hyprland build, no plugin build, no nested compositor, no `HYPRVX_AGENT` runs, no host
changes; never write outside this clone.

## 1. Sync (first step, once)

```sh
git status --short                      # must be empty
git fetch origin
git merge --no-ff --no-edit origin/v3/003-trackpad-native-ux
```

If the merge reports a conflict, stop, run `git merge --abort`, and report BLOCKED. After the merge,
`specs/003-trackpad-native-ux/` and this file exist in the clone.

## 2. Read before writing

1. `specs/003-trackpad-native-ux/contracts/gesture-model.md` — **the only specification of the code**. Every rule,
   constant, signature and invariant comes from it. If something is unclear or looks wrong, stop and report; do not
   guess and do not change the contract.
2. `specs/003-trackpad-native-ux/research.md` R3–R5 and R12 (why the rules are what they are).
3. `core/include/hyprvx/core/CameraMath.hpp` (`cammath::Vec`, `CameraState`, `ZoomLimits`, `anchorZoom`),
   `core/include/hyprvx/core/CameraMotion.hpp` and `core/src/CameraMotion.cpp` (V1 `CameraController`; note its
   constructor throws `std::invalid_argument` on an invalid config), `core/include/hyprvx/core/Camera.hpp`.
4. Style references: `core/include/hyprvx/core/DriftModel.hpp`, `core/src/DriftModel.cpp`,
   `tests/unit/test_drift_model.cpp`, `tests/unit/test_camera_math.cpp` (seeded randomness via `HYPRVX_TEST_SEED`).

## 3. Files you may change (and no others)

| File | State now | Task |
|---|---|---|
| `tests/unit/test_gesture_model.cpp` | one comment line, already in the test target | T005 |
| `core/include/hyprvx/core/GestureModel.hpp` | does not exist | T006 |
| `core/src/GestureModel.cpp` | one comment line, already in `hyprvx_core` | T006 |

`CMakeLists.txt` is already done (T002). Any other file changed is a reason to reject the handoff.

## 4. T005 — tests first (commit 1)

Write `tests/unit/test_gesture_model.cpp` (GTest, namespace use like `test_drift_model.cpp`) covering, from the
contract:

- `decideTarget`: one test per rule 1–12, plus an exhaustive enumeration of all 64 `BeginContext` combinations ×
  {Scroll, Swipe, Pinch} × fingers 1–5 asserting the table and that a 3-finger Swipe/Pinch with `drift == true` is
  never `PassThrough`.
- `ScrollTracker`: Begin/Continue/End per axis (stop on one axis only → still open; stops on both → End), stop while
  closed → Ignore, idle > 200 ms re-begins, wrapping `uint32_t` times (e.g. 0xFFFFFFF0 → 0x00000010), non-finite delta
  and axis ∉ {0,1} → Ignore, `reset`.
- `ReleaseVelocity`: empty → 0; one sample → 0; known samples → exact value; samples older than 100 ms excluded; rest
  > 50 ms → 0; equal timestamps merged; ring overflow (> 32 samples) keeps the newest; clamp to 6000 px/s preserving
  direction; non-finite deltas ignored.
- `glideDamping` (strength ≤ 0, NaN, 0.5 → 7, 1 → 2, > 1 treated as 1); `expectedGlide` (below `minSpeed` → 0,
  clamp at `maxSpeed`).
- `Glide`: sum of `step` until inactive equals `expectedGlide` within 0.5 px for (a) one 10 s step, (b) 1/60 s steps,
  (c) 1/144 s steps, (d) seeded random steps in [1 ms, 50 ms] (`HYPRVX_TEST_SEED`, default seed fixed); step length
  never increases per unit time; `stop()` → next step returns 0 and `active()` false; not started → step 0; invalid
  configs (NaN, `stopSpeed` ≤ 0, `maxSpeed` ≤ `stopSpeed`, negative `minSpeed`) → `start` returns false and nothing
  throws; `dt` ≤ 0 or NaN → 0.
- `pinchCamera`: seeded cases where the world point under the anchor in `start` (via `cammath::inverse`) is under the
  anchor in the result (`cammath::forward`) within 1e-6 logical px, including scales that hit both limits; scale ≤ 0
  or NaN → `start` unchanged.

Run it before T006 and keep the failing output (the header is missing, so the build fails): that is the "red" step.
Commit only the test file:

```
test(v3): Feature 003 GestureModel unit tests (WO-01, T005)

Task: T005
Verify: cmake --build build-agy -j4  (expected to fail: GestureModel.hpp missing)
Result: FAIL (expected red step) — <first error line>
Evidence: build-agy/wo01-t005-red.log (not committed)
Not-run: native tests (out of scope)
```

## 5. T006 — implementation (commit 2)

Create `core/include/hyprvx/core/GestureModel.hpp` and implement `core/src/GestureModel.cpp` exactly as the
contract: namespace `hyprvx::drift`; `noexcept` everywhere; no heap allocation after construction (fixed-size ring
for `ReleaseVelocity`); no I/O, no clock reads; `Glide` built on `hyprvx::CameraController` + `hyprvx::Camera`, with the
configuration validated **before** the controller is constructed or reconfigured (no exception can escape); camera
results only through `cammath` functions (no camera equation written inline — Constitution IV). Do not weaken or
delete any T005 test to make it pass; if a test is wrong against the contract, fix the test in a separate, clearly
explained commit and say so in the handoff.

Commit only the two core files:

```
feat(v3): Feature 003 GestureModel (WO-01, T006)

Task: T006
Verify: <exact commands of §6>
Result: PASS | FAIL — <ctest summary line>
Evidence: build-agy/wo01-*.log (not committed)
Not-run: native tests (out of scope)
```

## 6. Verify commands (inside this clone only)

```sh
cmake -S . -B build-agy -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-agy -j4 2>&1 | tee build-agy/wo01-build.log
ctest --test-dir build-agy --output-on-failure 2>&1 | tee build-agy/wo01-ctest.log
HYPRVX_TEST_SEED=987654321 build-agy/hyprvx_core_tests --gtest_filter='*Gesture*' 2>&1 | tee build-agy/wo01-seed2.log
git diff --check origin/v3/003-trackpad-native-ux..HEAD
bash scripts/check-single-transform.sh --self-test
for f in core/include/hyprvx/core/GestureModel.hpp core/src/GestureModel.cpp; do
  git diff --no-index /dev/null "$f" > "build-agy/$(basename "$f").patch"; done
bash scripts/check-single-transform.sh build-agy/GestureModel.hpp.patch build-agy/GestureModel.cpp.patch 2>&1 | tee build-agy/wo01-lint.log
```

`build-agy/` is ignored by git (`.gitignore`); never `git add` it. Report the
lint output verbatim: a line such as `start.zoom * scale` (the pinch target zoom, not a camera equation) may be
reported; do not rewrite code to hide it, just report it. Every existing test must still pass (326 before this order).

## 7. Handoff (what to report when done)

- The two (or three) commit hashes on `v3/agy-experiment` and `git log --oneline origin/v3/003-trackpad-native-ux..HEAD`.
- `git diff --stat origin/v3/003-trackpad-native-ux..HEAD` (must list only the three files of §3 and this clone's own
  `AGY_EXPERIMENT.md`).
- The ctest summary line, the seed-2 summary line, the lint output, and the red-step first error line.
- Anything you were unsure about in the contract, and any test you changed after the implementation existed (with
  the reason).
- Do not tick `tasks.md`; Claude reviews, rebuilds and re-runs everything before deciding (T007).
