---
description: "Task list for Feature 004 — DriftWM Window Behavior, Claude Code only (DV3-15)"
---

# Tasks: DriftWM Window Behavior (Feature 004)

**Input**: `specs/004-drift-window-behavior/`: `spec.md` (with Clarifications), `plan.md`, `research.md` (R1–R9),
`data-model.md`, `contracts/window-behavior.md` (1.0), `contracts/drift-control.md` (1.2),
`contracts/mechanism-api.md` (1.1), `contracts/test-tooling.md` (1.6), `quickstart.md`, `checklists/requirements.md`,
`checklists/window-behavior.md` (reviewer-owned).
**Governance**: constitution v1.0.0; master plan §C5, §D, §H Phase 5; decisions DV3-01, DV3-15, DV3-16, DV3-18.
**Baseline**: Feature 003 merged into `v3/spec-foundation` at `534388b`; branch `v3/004-drift-window-behavior`
(branch point `490e78c`).
**Status**: generated 2026-10-12; analyze remediation C10 (HIGH), C5, C1, C2, C4 applied and re-analyzed (no CRITICAL/HIGH);
**approved by the user on 2026-10-12** with the key-binding and instant-camera choices, on the condition met above.

**Status**: generated 2026-10-12; needs the user's approval before `/speckit-implement`.

**Tests**: REQUIRED (Constitution II).
- Tests are written from the spec and contracts before the code they judge.
- Every PASS needs raw evidence that is checksum-verified and archived into
  `~/.local/share/hyprvx/evidence/claude/<run-id>/`, plus a row in `checklists/verification.md`.
- Dispatcher replies and `hyprctl hyprvx state` are bookkeeping, never pass conditions (the frame-by-frame offset log
  of T-SNAP-OSC is judged against captures at the snap and release steps).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task).
- **[Story]**: US1–US4 from `spec.md`. Setup, Foundational, form S and gate phases have no story label.
- **Owner**: Claude for every task (DV3-15).
- **Ticking**: a task is ticked after its verification ran and its evidence is archived. Commits keep the trailers
  `Task:`, `Verify:`, `Result:`, `Evidence:`, `Not-run:`.
- **Heavy jobs** run only under the heavy lock with the memory checks of test-tooling §2 (MemAvailable ≥ 6.5 GiB,
  never lowered).
- **Forms**: native tests take `HVX_FORM=P|S`; until Phase 7 they run in form P only.

## Scope boundary

| In Feature 004 | Not in Feature 004 |
|---|---|
| Snap with hysteresis and alignment; implicit groups and group move; directional focus, center, frame window/group, home, bookmarks 1–4, history; pin to screen (patch 0014 + form S copies); keys on combinations the user does not bind; dispatchers; settings | Search/minimap/switcher UI; edge or grid snapping; tiling inside groups; persistence across restarts; timed camera transitions; unified world; Caelestia layer; performance budgets; multi-monitor/HDR; packaging; host config changes |

---

## Phase 1: Setup

- [ ] T001 Create `specs/004-drift-window-behavior/integration-log.md` (Feature 003 columns) and `checklists/verification.md` with V-01..V-17 all NOT RUN: V-01 unit (WindowBehavior + DriftModel group drag), V-02 identity refusal incl. new vkeyboard keys, V-03 T-SNAP, V-04 T-SNAP-OSC, V-05 T-GROUP, V-06 T-NAV, V-07 T-WB-OFF, V-08 T-PIN, V-09 P/S parity, V-10 Feature 003 regression, V-11 Feature 002 regression, V-12 Feature 001 regression, V-13 clean-room R1 + reproducibility, V-14 host-safety audit, V-15 single-transform lint incl. 0014 and WindowBehavior, V-16 GATE-A on the new patched prefix, V-17 plugin inert (both forms)
- [ ] T002 Register `core/src/WindowBehavior.cpp` in the `hyprvx_core` sources and `tests/unit/test_window_behavior.cpp` in the unit-test target (`CMakeLists.txt`), then a fresh build + `ctest` (existing 367 tests still pass)

---

## Phase 2: Foundational (blocking)

- [ ] T003 [P] Write `tests/unit/test_window_behavior.cpp` from `contracts/window-behavior.md` 1.0 before the code: `snapMove` attach at `threshold/zoom` for zoom 0.5/1/2, exact gap, perpendicular alignment, hold until more than `release` from the snapped coordinate, release, scripted slow back-and-forth paths with ≥ 10 crossings and 0 alternating steps (SC-002), nearest pair wins, ties keep the previous target then the lower id, disabled/invalid settings (`threshold > 0`, `gap >= 0`, `release >= threshold`) and non-finite input return the candidate unchanged; `groupOf` (pairs, chains, `gap ± 1` tolerance, no overlap = no neighbour, unknown seed = empty); `neighbour` (four directions, none); `frameCamera`/`centerCamera`/`revealCamera` against hand values incl. zoom clamps [0.1, 2.0] and margin 5%; `Bookmarks` slots 1–4 (others rejected, empty recall); history wrapper capacity 32, back/forward, new record clears forward. Confirm the red build
- [ ] T004 Implement `core/include/hyprvx/core/WindowBehavior.hpp` and `core/src/WindowBehavior.cpp` per the contract (camera results only through `CameraMath`; V1 `SpatialNavigator`, `frameRectInViewport`, `CameraHistory` reused through adapters, research R1)
- [ ] T005 Extend the drag in `core/include/hyprvx/core/DriftModel.hpp` / `core/src/DriftModel.cpp` per research R4 and data-model "Drag": tests first in `tests/unit/test_drift_model.cpp` (move with a members list, the same world delta for every member, snap state carried in the drag, cancel restores every start offset, a closed member leaves the drag, a closed snap target releases that axis), then the implementation; existing Feature 002 drag tests unchanged
- [ ] T006 V-01: run `tests/unit/run_unit_evidence.sh f004` (two seeds + mutation check of snap hold/release, group tolerance and framing clamp) and archive
- [ ] T007 [P] Add `KEY_HOME`, `KEY_END`, `KEY_F1`–`KEY_F4`, `KEY_LEFTBRACE`, `KEY_RIGHTBRACE`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFTSHIFT`, `KEY_LEFTCTRL`, `KEY_P` to the key table of `tools/vkeyboard/main.cpp` (test-tooling 1.6); unknown names still exit 2
- [ ] T008 [P] Add `hvx_combo` and `hvx_world_box` to `tests/native/lib.sh`, an independent expected-value helper `tests/native/f004_geom.py` (snap gap/alignment, group positions, center/frame/reveal cameras re-derived from the contract, with `selftest`), and `tests/native/run_feature004.sh` (tests × forms P, S × scales 1, 1.25; aggregate archive)
- [ ] T009 V-02: extend the identity-refusal run so the new vkeyboard keys are refused outside a verified session; run `tests/native/static_evidence.sh` and archive
- [ ] T010 Policy skeleton `platform/plugin/src/policy/Behavior.cpp` (+ declarations in `Plugin.hpp`, init/shutdown from `Session.cpp`): config values `plugin:hyprvx:snap` (0/1, default 1), `snap_threshold` (screen px, 24, > 0), `snap_gap` (world px, 12, ≥ 0), `snap_release` (world px, 32, ≥ threshold), `frame_margin` (fraction, 0.05, [0, 0.4]) with one log line per invalid key; dispatchers `hyprvx:focus|center|frame|home|bookmark|history|pin|unpin` registered (returning `err:state` until implemented); no key consumed yet. Build forms P, S, S-dev; V-17: `t_plugin_inert.sh` in both forms

---

## Phase 3: User Story 1 — Snap (Priority: P1) 🎯 MVP

**Goal**: windows snap with gap, alignment and hysteresis during Super + drag. **Independent test**: T-SNAP, T-SNAP-OSC.

- [ ] T011 [P] [US1] Write `tests/native/t_snap.sh` (T-SNAP): three windows, Drift on; at zoom 0.5, 1 and 2 drag B towards A until within the threshold: the capture shows the gap and aligned edges ±1 px (expected from `f004_geom.py`); continued drag within the release distance keeps the snapped box; beyond it B follows the pointer; snap off (`snap 0`) lands B exactly at the pointer; `hyprctl clients` unchanged and no `movewindow` event (SC-006)
- [ ] T012 [P] [US1] Write `tests/native/t_snap_osc.sh` (T-SNAP-OSC): a scripted slow path crossing the threshold back and forth ≥ 10 times in 2-px steps; after each step the offset is logged; the sequence may change only at the scripted snap and release steps (0 alternating steps, SC-002); captures at a snap step and a release step judged against the expected boxes
- [ ] T013 [US1] Implement snap in `platform/plugin/src/policy/Input.cpp` on drag motion through `DriftModel` and `snapMove` (others = non-pinned scene windows of the same workspace; none while the workspace shows a fullscreen window; settings from T010)
- [ ] T014 [US1] Run T011–T012 in form P at scales 1 and 1.25; archive (V-03, V-04)

---

## Phase 4: User Story 2 — Groups (Priority: P1)

**Goal**: snapped windows move together with Shift + Super + drag. **Independent test**: T-GROUP.

- [ ] T015 [P] [US2] Write `tests/native/t_group.sh` (T-GROUP): A–B snapped, C apart; Shift + Super + drag on A moves A and B by the same distance ±1 px and C not at all; chain A–B–C moves together; plain Super + drag of B past the release distance leaves the group and a later Shift + Super + drag of A moves A only; a moved group snaps as one to C; Shift + Super + drag on a window in no group moves it alone; Drift off/on keeps the grouping; Escape during a group drag restores every member; closing B during a group drag on A: B leaves the drag, A finishes the drag at the expected position, no crash; `hyprctl clients` unchanged, no `movewindow`
- [ ] T016 [US2] Implement the group drag in `platform/plugin/src/policy/Input.cpp` (Shift read at drag begin, members = `groupOf`, snap with the members as the moving set) and the cancel paths in `platform/plugin/src/policy/Session.cpp` (every member restored)
- [ ] T017 [US2] Run T015 and re-run T011–T012 in form P at scales 1 and 1.25; archive (V-05)

---

## Phase 5: User Story 3 — Navigation (Priority: P1)

**Goal**: keyboard jumps between windows and views. **Independent test**: T-NAV, T-WB-OFF.

- [ ] T018 [P] [US3] Write `tests/native/t_nav.sh` (T-NAV): with windows placed around the canvas, through the harness keyboard (`hvx_combo`) and the dispatchers: Super + arrow focuses the expected neighbour (`hyprctl activewindow`) and the capture shows it fully visible with the margin; no neighbour = no change; center (window center at screen center ±1 px); frame window and frame group (expected cameras incl. a clamp case); home (identity); bookmarks store/recall and empty recall; history back/forward and a new jump clearing forward; a jump that changes nothing records nothing; dispatchers return `err:state` with Drift off; `hyprctl clients` unchanged except focus
- [ ] T019 [P] [US3] Write `tests/native/t_wb_off.sh` (T-WB-OFF): with Drift off every new key combination reaches the focused client (client key log) and captures equal a stock session doing the same; a pinned window is drawn as stock; with Drift on, keys that the feature does not define still reach the client
- [ ] T020 [US3] Implement keys and dispatchers in `platform/plugin/src/policy/Behavior.cpp` per `contracts/drift-control.md` 1.2: instant camera jumps (glide stopped first), history recorded only when the camera changes, bookmarks and history per workspace, keys consumed only on a monitor in DRIFT and only for the defined combinations, each press acts once and holding a key does not repeat, releases pass through; extend the Feature 002 key listener in `Input.cpp`
- [ ] T021 [US3] Run T018–T019 in form P at scales 1 and 1.25; archive (V-06, V-07)

---

## Phase 6: User Story 4 — Pin (Priority: P2)

**Goal**: a window fixed on screen above the canvas. **Independent test**: T-PIN.

- [ ] T022 [US4] Write patch 0014 in the draft tree per `contracts/mechanism-api.md` 1.1 (`setWindowPin`/`windowPin`/`resetWindowPins`; pinned windows skipped in the regular loops of `renderWorkspaceWindows` and drawn after the floating pass under identity camera with translation `M + pin(w) − L(w)`; hit-test pinned first, surface-local coordinates and popups with the same translation; damage and culling with the pinned box; inactive outside Drift or under fullscreen; dev control `pin`/`unpin`/`state`/`panic`); export `patches/hyprland/0014-hyprvx-screen-pinned-windows.patch`; single-transform lint on the series
- [ ] T023 [US4] Build the new patched prefix and `patched-nodev`; record the new prefix id; V-16: GATE-A re-run (stock baseline unchanged)
- [ ] T024 [P] [US4] Write `tests/native/t_pin.sh` (T-PIN): a mechanism-only case through dev control (pin a window, pan and zoom: drawn box constant ±1 px; drawn above an overlapping window; a click on the overlap reaches the pinned window), then the policy cases: Super + Shift + P pins the focused window keeping its drawn top-left, drawn at its own size; pans/zooms keep it; unpin at z = 1 keeps the whole box and at z = 2 keeps the top-left with the size doubled, and then it moves with the canvas; a popup of the pinned window opens at the pinned position and its click reaches the popup; no snap to a pinned window and no group membership; two pins: the later on top; a pin follows its window to another workspace; Drift off and fullscreen draw identity; `hyprctl clients` unchanged, no `movewindow`
- [ ] T025 [US4] Implement pin/unpin in `platform/plugin/src/policy/Behavior.cpp` (drawn top-left stored through `setWindowPin`; unpin sets `O(w)` to keep the drawn top-left, then clears the pin; pinned windows excluded from snap, groups and navigation; pins in `hyprctl hyprvx state`; cleanup on window close; panic and unload per Feature 002)
- [ ] T026 [US4] Form S: pin functions in `platform/plugin/src/mechanism/`, copied bodies regenerated from the series by the existing generator, THIRD_PARTY record updated (functions and counts); build forms S and S-dev
- [ ] T027 [US4] Run T024 in form P at scales 1 and 1.25; archive (V-08)

---

## Phase 7: Delivery form S and parity

- [ ] T028 Run every Feature 004 test in form S at scales 1 and 1.25; compare all captures with form P using `tests/native/compare_forms.py` (byte-identical; exclusions only with a recorded reason) (V-09)

---

## Phase 8: Feature 004 gate

- [ ] T029 Run `run_feature004.sh` (both forms, both scales), then `run_feature003.sh`, `run_feature002.sh` and `run_sprint1.sh` / `run_phase2.sh` with `HVX_REGRESSION_FORM=P|S` on the new prefix (V-10, V-11, V-12)
- [ ] T030 R1 clean-room: rebuild the patched prefix, both plugin forms and the test plugins from a fresh worktree without ccache; re-run `run_feature004.sh`; write `specs/004-drift-window-behavior/r1-source-verification.md` before reading the results, covering with file:line: FR-001 (nothing acts with Drift off), Constitution IV (no inline camera arithmetic in the new core, policy and patch code), V and FR-005 (no write to Hyprland position, size, workspace, floating state, stacking order or layout), the pin mechanism scope, every test changed after code existed with its reason; then a second clean build for reproducibility (V-13)
- [ ] T031 [P] V-14 host-safety audit (`host_safety_audit.sh` and `session_ready_audit.py` since the feature start) and V-15 lint record (`static_evidence.sh` extended to patch 0014 and `WindowBehavior`, reviewed-findings list kept exact)
- [ ] T032 Update `checklists/verification.md` (V-01..V-17 with archive paths and `sha256sum -c` results), `integration-log.md`, `.ai/CURRENT_STATE.md` and `.ai/NEXT_TASK.md`; validate `quickstart.md` by running it and correct any drift; constitution re-check of the delivered code in `plan.md`
- [ ] T033 Merge-readiness review: every commit since `490e78c` reviewed, every PASS backed by archived evidence, `git rev-parse dev == 15f75b0`, nothing pushed
- [ ] T034 **Feature 004 gate**: present the evidence, the reviewer checklist state and the open items. STOP for the user's approval before merging into `v3/spec-foundation` or starting any later feature

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (1)** → **Foundational (2)** → **US1 (3)** → **US2 (4)** → **US3 (5)** → **US4 (6)** → **Form S (7)** → **Gate (8)**.
- US2 builds on US1's snap; US3 is independent of US1/US2 in code but shares `Input.cpp` (sequential edits); US4 needs
  the new mechanism (T022–T023) before its policy and tests.

### Task dependencies (non-obvious)

- T002 before T003; T003 before T004 (test first); T005's tests before its implementation; T006 after T004–T005.
- T007–T008 before any native Feature 004 test; T009 after T007.
- T010 before T013, T016, T020, T025.
- T022 → T023 → T024 (its mechanism case needs dev control) → T025 → T026 → T027.
- T023 changes the patched prefix: every later native run uses it; the regressions in T029 run on it.
- T030 runs on the final commit; T034 is last.

### Execution order (Claude)

T001–T010 → US1 → US2 → US3 → US4 → T028 → T029–T034.

## Parallel Example: Phase 2

```text
T003 tests/unit/test_window_behavior.cpp   T007 tools/vkeyboard/main.cpp   T008 tests/native/lib.sh + f004_geom.py + run_feature004.sh
```

## Implementation Strategy

- **MVP**: US1 (snap) on top of the Feature 002/003 baseline; the master plan's gate centers on snap and clusters.
- **Increment 2**: US2 (groups), completing the gate's snap/cluster coverage.
- **Increment 3**: US3 (navigation), keyboard only, no mechanism change.
- **Increment 4**: US4 (pin), the only mechanism change (patch 0014), last so a problem there cannot block the rest.
