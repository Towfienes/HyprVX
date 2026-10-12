---
description: "Task list for Feature 003 — Trackpad-Native Drift UX, Claude Code only (DV3-15; DV3-17 trial ended before it started)"
---

# Tasks: Trackpad-Native Drift UX (Feature 003)

**Input**: `specs/003-trackpad-native-ux/` — `spec.md` (with Clarifications), `plan.md`, `research.md` (R1–R13),
`data-model.md`, `contracts/gesture-model.md` (1.0), `contracts/drift-control.md` (1.1), `contracts/test-tooling.md`
(1.5), `quickstart.md`, `checklists/requirements.md`, `checklists/trackpad.md`.
**Governance**: constitution v1.0.0; master plan §D, §F (L6), §H Phase 4, U4; decisions DV3-01, DV3-15, DV3-16.
**Baseline**: Feature 002 merged into `v3/spec-foundation` at `dd4f9b9`; branch `v3/003-trackpad-native-ux`.

**Status**: generated 2026-10-12; analyze remediation C1, C2, L1, L2 applied; **approved by the user on 2026-10-12**
(conditional on no scope change or new blocker from the remediation; re-analysis found none).

**Tests**: REQUIRED (Constitution II).
- Tests are written from the spec and contracts before the code they judge.
- Every PASS needs raw evidence that is checksum-verified and archived into
  `~/.local/share/hyprvx/evidence/claude/<run-id>/`, plus a row in `checklists/verification.md`.
- Dispatcher replies, `hyprctl hyprvx state` and `hvxtest` replies are never pass conditions (frame counts are a
  measurement only).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task).
- **[Story]**: US1–US4 from `spec.md`. Setup, Foundational, form S and gate phases have no story label.
- **Owner**: Claude for every task (DV3-15). The Antigravity trial (DV3-17) was ended by the user on 2026-10-12 before
  WO-01 started; T005–T006 are Claude's, and T007 reduces to Claude's own V-01 evidence run.
- **Ticking**: a task is ticked after its verification ran and its evidence is archived. Commits keep the trailers
  `Task:`, `Verify:`, `Result:`, `Evidence:`, `Not-run:`.
- **Heavy jobs** (Hyprland build, nested session, plugin build against a prefix) run only under the heavy lock with the
  memory checks of test-tooling §2. The MemAvailable threshold is never lowered. Native runs need the host GPU
  profile Integrated.
- **Forms**: native tests take `HVX_FORM=P|S`. Until Phase 7 they run in form P only.

## Scope boundary

| In Feature 003 | Not in Feature 003 |
|---|---|
| Two-finger and Super + two-finger pan; pinch zoom at the pointer; 3-finger arbitration (swipe pan, pinch zoom, never a binding); 4+-finger pass-through; cancel; glide and its interruption; test tools for finger scrolls and synthetic gestures; both forms; L6 in a visible nested session | Physical pinch / 3-finger / U4 validation (BLOCKED, carried to an approved hardware session); zoom momentum; hold gestures; touchscreen; host config changes (incl. three-finger drag); snap, clusters, pin, navigation (Phase 5); Caelestia layer (Phase 6); budgets (Phase 7); unified world (U); multi-monitor (Phase 8); packaging (Phase 9) |

---

## Phase 1: Setup

- [X] T001 Create `specs/003-trackpad-native-ux/integration-log.md` (append-only, same columns as Feature 002's; first rows: this feature's planning commits incl. the note that `7e0ffeb` holds only the checklist and `f2882f2` the gap fixes) and `specs/003-trackpad-native-ux/checklists/verification.md` with rows V-01..V-24, all `NOT RUN`, mapped as: V-01 `GestureModel` unit tests; V-02 tool and harness identity refusal (fscroll, `hvxtest`, `--interactive`); V-03 GATE-A after the config change; V-04 T-TP-PAN; V-05 T-TP-WINDOW; V-06 T-TP-SUPER; V-07 T-TP-LOCK; V-08 T-TP-SURFACES; V-09 T-TP-OFF; V-10 T-PINCH; V-11 T-CANCEL; V-12 T-ARBITRATE; V-13 T-GLIDE; V-14 T-GLIDE-STOP; V-15 T-TP-MODES; V-16 T-TP-DRAG; V-17 P/S parity; V-18 T-FR020; V-19 Feature 002 regression; V-20 Feature 001 regression; V-21 clean-room reproducibility; V-22 single-transform lint incl. the new policy files; V-23 host-safety audit; V-24 L6 physical checklist (human; partial by design)
- [X] T002 [P] Register `core/src/GestureModel.cpp` in `hyprvx_core` and `tests/unit/test_gesture_model.cpp` in the unit-test target in `CMakeLists.txt` (empty test file that compiles), so the core work in T005–T006 touches only the three contract files
- [X] T003 [P] Add `--testinput` to `scripts/build-plugin.sh` (test-tooling 1.5 §8: output `~/.cache/hyprvx/claude/plugin/testinput-<form>/hyprvx-testinput-<form>.so` + `build-info.json`, fresh build dir, heavy lock, MemAvailable check, `MemoryMax=6G`, `-j4`) and the CMake skeleton in `tools/testinput/CMakeLists.txt`; extend `tools/tests/test_build_scripts.py` for the new option (usage errors, dry run)

---

## Phase 2: Foundational (blocking prerequisites)

### GestureModel (pure core; delegation-trial candidates)

- [X] T004 Write the Antigravity work order for T005–T006 in `specs/003-trackpad-native-ux/delegation/WO-01-gesture-model.md`: task IDs, the contract `contracts/gesture-model.md` as the only spec, the three files it may touch (`core/include/hyprvx/core/GestureModel.hpp`, `core/src/GestureModel.cpp`, `tests/unit/test_gesture_model.cpp`), verify commands (`cmake` + `ctest` in a build dir inside the clone, `bash scripts/check-single-transform.sh`), commit trailers, and what is out of bounds (`AGY_EXPERIMENT.md`). The user starts Antigravity with it; Claude never runs commands inside the clone
- [X] T005 [P] Write `tests/unit/test_gesture_model.cpp` from `contracts/gesture-model.md` before the implementation: the full `decideTarget` table (every rule, plus an exhaustive enumeration of `BeginContext` × kind × fingers 1–5 checking "3 fingers on a Drift monitor is never PassThrough"); `ScrollTracker` begin/continue/end per axis, idle 200 ms with wrapping times, non-finite input; `ReleaseVelocity` window 100 ms, rest 50 ms, merge of equal timestamps, ring overflow, 6000 px/s clamp; `glideDamping`, `expectedGlide`; `Glide` invariants (sum of steps = `expectedGlide` ± 0.5 px for one 10 s step, 60 Hz, 144 Hz and seeded random steps; speed never increases; `stop()`; not started → 0; invalid config → not started, no throw); `pinchCamera` anchor invariant incl. clamped zoom and invalid scale. Show that it fails (missing header or failing tests)
- [X] T006 Implement `core/include/hyprvx/core/GestureModel.hpp` and `core/src/GestureModel.cpp` exactly as `contracts/gesture-model.md` (namespace `hyprvx::drift`, no heap after construction, noexcept, `Glide` on V1 `hyprvx::CameraController` + `hyprvx::Camera` with the config validated before construction); T005 and every existing test pass
- [X] T007 Integrate T005–T006 (delegation cancelled 2026-10-12: Claude runs V-01 on its own T005–T006; the rest of this task applies only if a delegated handoff existed): if delegated, fetch from outside the clone (`git fetch /home/towfienes/Projects/HyprVX-v3-agy v3/agy-experiment:refs/agy/v3/agy-experiment`), review `git diff` against the contract line by line (incl. that tests were not weakened and that only the three files changed), cherry-pick the task commits (never `AGY_EXPERIMENT.md`), rebuild in a fresh build dir, re-run `ctest` and the lint, and record the review result, defects found and Claude effort in `integration-log.md`; archive V-01 from Claude's own run (unit evidence script as in Feature 002, two seeds)

### Test tools and harness (test-tooling 1.5)

- [X] T008 [P] Add `fscroll T0 DT N DX DY [stop]` to `tools/vpointer/main.cpp` per test-tooling 1.5 §4a (axis → axis_source(finger) → frame per axis, explicit times, optional stop, limits with an `error` line)
- [X] T009 [P] Add pointer-gestures binding and the swipe/pinch, `axis_source`, `axis_stop` and `axis_value120` log lines to `tools/testclient/main.cpp` per test-tooling 1.5 §4 (existing lines unchanged)
- [X] T010 [P] Implement the test-only plugin in `tools/testinput/src/main.cpp`: hyprctl command `hvxtest` (`gesture swipe|pinch begin|update|end …` calling `g_pInputManager->onSwipe*/onPinch*` with the given fields and range checks; `frames <monitor>` from a `render.pre` counter), per test-tooling 1.5 §8
- [X] T011 Add `start --interactive` to `tools/hyprvx_nested.py` / `scripts/nested.sh`: same verified session, but the Wayland output (the visible nested window) stays enabled and no headless output is created, for L6 only; captures are refused in this mode. Let `plugin load` accept `plugin/testinput-*` paths. Keep every identity rule
- [X] T012 Extend `tests/native/harness/test_identity_refusal.sh` and `tests/native/static_evidence.sh`: `fscroll` and `hvxtest` refused outside a verified session, `--interactive` refuses captures; run it (V-02)
- [X] T013 Add the gesture bindings of test-tooling 1.5 §5 to `tests/native/config/hyprland.conf.in` (`gesture = 3, down, close`, `gesture = 3, up, fullscreen`, `gesture = 3, left, float`, `gesture = 4, horizontal, workspace`; never a command binding) and re-run `tests/native/gate_a_stock_determinism.sh`; the baseline hash must be unchanged (V-03)
- [X] T014 [P] Add test helpers: `tests/native/f003_geom.py` (expected pan `−(ΣDX, ΣDY)·f`, swipe pan, `pinchCamera`, `ReleaseVelocity` and `expectedGlide` re-implemented from the contract, cross-checked against the unit-test vectors) and `lib.sh` helpers `hvx_fscroll`, `hvx_gesture`, `hvx_frames`, `hvx_load_testinput`

### Policy skeleton

- [X] T015 Add `platform/plugin/src/policy/Gesture.cpp` and declarations in `Plugin.hpp`: listeners for `input.mouse.axis` (finger source only), `gesture.swipe.*`, `gesture.pinch.*`; config values `plugin:hyprvx:pan_factor` (1.0, "finite, > 0"), `plugin:hyprvx:glide` (0.5, "finite, in [0, 1]"), `plugin:hyprvx:glide_min_speed` (100, "finite, ≥ 0"), each "invalid value → default, one log line"; `shutdownGesture()` on unload. Every target is `PassThrough` for now; listeners return at once when no monitor is in Drift and no session is open. Build forms P and S; run `t_plugin_inert.sh` (no regression)

---

## Phase 3: User Story 1 — Pan the canvas with two fingers (P1) 🎯 MVP

**Goal**: finger scrolls on empty canvas (or with Super anywhere) pan; over windows they reach the window.
**Independent test**: T-TP-PAN, T-TP-WINDOW, T-TP-SUPER, T-TP-LOCK, T-TP-SURFACES (scroll part), T-TP-OFF (scroll part).

### Tests

- [X] T016 [P] [US1] Write `tests/native/t_tp_pan.sh` (T-TP-PAN): three windows, z 1 and 0.5, `fscroll` on canvas; every bbox moves by the expected pan ± 1 px; no client logs an axis event
- [X] T017 [P] [US1] Write `tests/native/t_tp_window.sh` (T-TP-WINDOW): `fscroll` over a window without Super; the client logs the same values with source `finger` and both stops; capture unchanged
- [X] T018 [P] [US1] Write `tests/native/t_tp_super.sh` (T-TP-SUPER): vkeyboard holds `KEY_LEFTMETA`; `fscroll` over a window pans; the client logs nothing
- [X] T019 [P] [US1] Write `tests/native/t_tp_lock.sh` (T-TP-LOCK): a sequence begun on canvas keeps panning after the pointer moves over a window; one begun over a window stays delivered after the pointer moves to canvas
- [X] T020 [P] [US1] Write `tests/native/t_tp_surfaces.sh` (T-TP-SURFACES) scroll part: over the test layer surface and with a fullscreen window on the scene, finger scrolls are delivered (client logs) and nothing moves
- [X] T021 [P] [US1] Write `tests/native/t_tp_off.sh` (T-TP-OFF) scroll part: Drift off, finger scrolls: captures byte-identical to the stock session and client logs identical to the stock session

### Implementation

- [X] T022 [US1] Implement scroll sessions in `platform/plugin/src/policy/Gesture.cpp`: `ScrollTracker` per sequence; at `Begin` build `BeginContext` (Drift under the pointer, layer/layer-popup test shared with `Input.cpp`'s press target, the scene workspace's fullscreen flag, `model().drag()`, Super, window hit test) and `decideTarget`; the owning monitor and workspace are fixed at begin; cancel every event of a consumed sequence until `End`; Pan applies `model().pan(monitor, −(dx, dy)·f)` with `f = input:touchpad:scroll_factor × pan_factor` ("≤ 0 or non-finite → 1.0" for the touchpad factor) and `sync(true)`; no camera change once `live` is false
- [X] T023 [US1] Run T016–T021 in form P at scales 1 and 1.25; archive (V-04..V-07, scroll parts of V-08 and V-09)

---

## Phase 4: User Story 2 — Pinch to zoom at the pointer (P1)

**Goal**: pinches on canvas (or with Super) zoom absolutely from the begin camera at the begin pointer; over windows they reach the window; cancel restores.
**Independent test**: T-PINCH, T-CANCEL (pinch part).

### Tests

- [X] T024 [P] [US2] Write `tests/native/t_pinch.sh` (T-PINCH): `hvxtest gesture pinch` at three anchors with scales 0.5, 2, 0.01 and 50; anchor world point fixed ± 1 px, zoom = clamp(z0·s); Super + pinch over a window zooms; pinch over a window without Super: client logs the pinch, capture unchanged
- [X] T025 [P] [US2] Write `tests/native/t_cancel.sh` (T-CANCEL) pinch part: a pinch ended with `cancelled 1` gives a capture byte-identical to the one before it, and no glide (frame count stops)

### Implementation

- [X] T026 [US2] Implement gesture sessions for pinch in `platform/plugin/src/policy/Gesture.cpp`: `decideTarget(Pinch, fingers, ctx)` at begin; cancel every event of a consumed gesture until its end; Zoom sets `model().setCamera(workspace, pinchCamera(C0, M, a0, scale))` per update; cancelled end restores `C0`; consumed gestures stay consumed after the monitor leaves Drift, without camera changes
- [X] T027 [US2] Run T024–T025 in form P at scales 1 and 1.25; archive (V-10, pinch part of V-11)

---

## Phase 5: User Story 3 — Multi-finger gestures never trigger window actions (P1)

**Goal**: 3-finger swipes pan, 3-finger pinches zoom, and over layers / fullscreen / during a drag they are consumed without effect; 4+ fingers stay stock.
**Independent test**: T-ARBITRATE, T-CANCEL (swipe part), T-TP-SURFACES and T-TP-OFF (gesture parts).

### Tests

- [X] T028 [P] [US3] Write `tests/native/t_arbitrate.sh` (T-ARBITRATE): ≥ 20 3-finger swipes (down, up, left, right) and 3-finger pinches in Drift over canvas, over a window and over the layer surface; window count, fullscreen and floating unchanged (`hyprctl clients -j`), no fullscreen/special events (socket2), camera as specified; Drift-off control performs each bound action; 4-finger horizontal swipe switches the workspace in both modes and each workspace keeps its camera
- [X] T029 [US3] Extend `tests/native/t_cancel.sh` (3-finger swipe cancel), `tests/native/t_tp_surfaces.sh` (3-finger over the layer and with fullscreen: consumed, nothing moves, no binding acts) and `tests/native/t_tp_off.sh` (pinches and 3/4-finger gestures with Drift off: captures and client logs equal to stock, bindings act)

### Implementation

- [X] T030 [US3] Implement swipe sessions and the 3/4-finger rules in `platform/plugin/src/policy/Gesture.cpp`: 3-finger swipe Pan applies `model().pan(monitor, delta·pan_factor)`; a swipe ended with `cancelled` restores the camera at begin (`C0`) and starts no glide (FR-012); `Ignore` targets cancel every event and change nothing; 4+ fingers pass through untouched
- [X] T031 [US3] Run T028–T029 (and re-run T024–T025, T016–T021) in form P at scales 1 and 1.25; archive (V-12, full V-08, V-09, V-11)

---

## Phase 6: User Story 4 — Released pans glide to a stop (P2)

**Goal**: pans released with speed glide by the closed-form rule and stop on any interruption; no frames at rest.
**Independent test**: T-GLIDE, T-GLIDE-STOP, T-TP-MODES, T-TP-DRAG.

### Tests

- [X] T032 [P] [US4] Write `tests/native/t_glide.sh` (T-GLIDE): `fscroll` releases at 1000 and 3000 px/s with glide 0.5 and 1.0; final displacement = `expectedGlide` ± 2 px; `hvx_frames` constant over 500 ms after the end; glide 0, a release below `glide_min_speed` and a 60 ms rest before the stop: no movement after the stop; a 3-finger swipe release glides too
- [X] T033 [P] [US4] Write `tests/native/t_glide_stop.sh` (T-GLIDE-STOP): during a glide, a finger scroll, a click, a key, `hyprvx:off`, panic and a workspace switch each stop it (two captures one frame apart identical; frame count stops); removing the glide's output (`hyprctl output remove`, as Feature 002 `t_monitor.sh`) leaves the compositor alive with Drift OFF and no further camera change
- [X] T034 [P] [US4] Write `tests/native/t_tp_modes.sh` (T-TP-MODES): Drift off, panic and removal of the owning output in the middle of a consumed pinch and a consumed scroll: remaining events stay consumed (no client logs them), capture after OFF equals stock, the compositor stays alive, next gesture behaves as configured
- [X] T035 [P] [US4] Write `tests/native/t_tp_drag.sh` (T-TP-DRAG): finger scroll, pinch and 3-finger swipe during a Super + drag (pan and move) change neither the camera nor the dragged window; a drag beginning during a scroll sequence ends that sequence's camera effect

### Implementation

- [X] T036 [US4] Implement glide in `platform/plugin/src/policy/Gesture.cpp`: pan sessions feed `ReleaseVelocity` with the screen deltas; at a non-cancelled end with `live`, `Glide::start(v, cfg)` with `cfg` from the config values; while active, a `render.pre` listener (registered only while gliding) steps by the steady-clock time since the last step and applies `model().pan` + `sync(true)`; unregister when the glide ends
- [X] T037 [US4] Wire glide interruption and session closing: `Input.cpp` (button, key, any axis event; a Feature 002 drag start sets the open session's `live = false`), `Session.cpp` (`hyprvx:off`/`toggle`/`panic`, workspace change on the glide's monitor, monitor removal, unload; `"glide"` per monitor in `hyprctl hyprvx state`), and gesture begin in `Gesture.cpp`
- [X] T038 [US4] Run T032–T035 (and the US1–US3 tests again) in form P at scales 1 and 1.25; archive (V-13..V-16)

---

## Phase 7: Delivery form S and parity

- [X] T039 Build forms S and S-dev of the policy plugin and `testinput-S`; run every Feature 003 test in form S at scales 1 and 1.25; compare all captures with form P using `tests/native/compare_forms.py` (byte-identical; exclusions only with a reason) (V-17)
- [X] T040 [P] Write and run `tests/native/t_fr020.sh` (T-FR020): the form S plugin alone and `patched-nodev` + form P plugin answer `unknown request` to `hvxtest`; `strings` finds no `hvxtest` in either delivery plugin (V-18)

---

## Phase 8: Feature 003 gate

- [X] T041 Write `tests/native/run_feature003.sh` (all §9 tests, both forms, both scales) and run it; then `run_feature002.sh` and `run_sprint1.sh` / `run_phase2.sh` with `HVX_REGRESSION_FORM=P|S` and the Feature 003 plugin (V-19, V-20)
- [X] T042 Write `specs/003-trackpad-native-ux/checklists/l6-physical.md` per research R11: safety lines first (no 3- or 4-finger gesture over the nested window; stop with `nested.sh stop`); items performed by hand (two-finger pan on canvas, two-finger scroll over a window, Super + two-finger pan, glide and its stop by touching, glide 0, Drift off stock scrolling) with PASS/FAIL/NOT RUN fields; items BLOCKED (environment) with the automated run-ids attached (pinch, Super + pinch, pinch over a window, 3-finger arbitration, 4-finger in Drift, U4); signature line
- [X] T043 R1 clean-room: rebuild both plugin forms and both test plugins from a fresh worktree without ccache (the patched prefix id must still be `d291f743b864`), re-run `run_feature003.sh`, and write `specs/003-trackpad-native-ux/r1-source-verification.md` before reading the results, covering with file:line: FR-001 (nothing consumed while OFF), Constitution IV (no inline camera arithmetic in the new policy or core files), V (no write to offsets, `L`, workspace or layout from trackpad input), FR-020 (no `hvxtest` in delivery builds), every test changed after code existed with its reason; then a second clean build for reproducibility (V-21)
- [X] T044 [P] V-23 host-safety audit of every nested session of this feature (incl. the L6 session) and V-22 lint record: extend `tests/native/static_evidence.sh` so the single-transform lint also covers `core/include/hyprvx/core/GestureModel.hpp` and `core/src/GestureModel.cpp` (it covers `platform/plugin/src/{policy,mechanism}` today, so `Gesture.cpp` is already included), then run it
- [X] T045 L6 session with the user: start `nested.sh start --variant patched --interactive`, load the form P plugin, turn Drift on with three test windows; the user performs `checklists/l6-physical.md` with the real trackpad and records each item and the signature; Claude records the result honestly (V-24, NOT AUTOMATABLE; the BLOCKED items stay BLOCKED)
- [X] T046 Update `checklists/verification.md` (V-01..V-24 with archive paths and `sha256sum -c` results), `integration-log.md` (incl. the delegation-trial outcome: defects found in review, rework, Claude effort saved or not), `.ai/CURRENT_STATE.md` and `.ai/NEXT_TASK.md`; validate `quickstart.md` by running it and correct any drift; constitution re-check of the delivered code in `plan.md`
- [X] T047 Merge-readiness review: every commit since `142d2d0` (branch point) reviewed (including integrated delegated commits), every PASS backed by archived evidence, `git rev-parse dev == 15f75b0`, nothing pushed, no `AGY_EXPERIMENT.md` on this branch
- [ ] T048 **Feature 003 gate**: present the evidence, the signed L6 checklist with its BLOCKED items, the delegation-trial outcome and the open items. STOP for the user's approval before merging into `v3/spec-foundation` or starting any later feature.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (1)** → **Foundational (2)** → **US1 (3)** → **US2 (4)** → **US3 (5)** → **US4 (6)** → **Form S (7)** → **Gate (8)**.
- US2–US4 share `Gesture.cpp` (sequential edits) and depend on the session infrastructure built in US1.
- Form S (7) needs the full policy (3–6). The patch series and form S mechanism do not change.

### Task dependencies (non-obvious)

- T002 before T005; T004 before a delegated T005–T006; T005 before T006 (test first); T007 after T006 and before T022
  (the policy uses `GestureModel`).
- T003 before T010's build; T010 and T011 before T012; T008–T013 before any native Feature 003 test run.
- T013 (config) needs the GATE-A re-run before any capture comparison is trusted.
- T014 before the native tests (expected values).
- T015 before T022; in each story phase the tests come before the implementation they judge.
- T040 needs `patched-nodev` (Feature 002 build variant).
- T043 runs on the final commit; T045 needs the user present; T048 is last.

### Execution order (Claude, with the trial in parallel)

T001–T015 (T005–T006 by Claude since the trial was cancelled) → US1 → US2 → US3 → US4 → T039–T040 → T041–T048.

## Parallel Example: Phase 2 tools

```text
T008 tools/vpointer/main.cpp   T009 tools/testclient/main.cpp   T010 tools/testinput/src/main.cpp   T014 tests/native/f003_geom.py
```

## Implementation Strategy

1. **MVP**: Phases 1–3. Two-finger pan works and is physically checkable in a nested session; Drift off stays stock.
2. **Increments**: US2 (pinch) → US3 (arbitration) → US4 (glide), each independently testable.
3. **Delivery**: form S with byte parity.
4. **Gate**: R1 clean-room, L6 with the user (partial by design), then STOP at T048.

## Notes

- The delegation trial (DV3-17) was ended before it started; no Antigravity work exists. Only Claude's runs produce
  evidence.
- Physical pinch, 3-finger, 4-finger-in-Drift and U4 items stay BLOCKED in L6 and are listed as open at the gate.
- `checklists/trackpad.md` items are reviewer-owned and stay unchecked unless the user reviews them.
