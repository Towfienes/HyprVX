---
description: "Task list for Feature 002 — Multi-Window Drift MVP (Option A), Claude Code only (DV3-15)"
---

# Tasks: Multi-Window Drift MVP (Feature 002)

**Input**: `specs/002-multi-window-drift-mvp/` — `spec.md`, `plan.md`, `research.md` (R1–R13), `data-model.md`,
`contracts/drift-control.md` (1.0), `contracts/mechanism-api.md` (1.0), `contracts/transform-contract.md` (1.2),
`contracts/test-tooling.md` (1.4), `quickstart.md`, `checklists/requirements.md`, `checklists/drift.md`.
**Governance**: constitution v1.0.0; master plan §C, §D, §H Phase 3; decisions DV3-01, DV3-11, DV3-12, DV3-15, DV3-16.
**Baseline**: Feature 001 merged into `v3/spec-foundation` at `6931c04`; branch `v3/002-multi-window-drift-mvp`.

**Status**: generated 2026-10-11; **implementation needs the user's approval of this file**.

**Tests**: REQUIRED (Constitution II).
- Tests are written from the spec and contracts before the code they judge.
- The mechanism is verified through the dev control before the policy exists.
- Every PASS needs raw evidence that is checksum-verified and archived into
  `~/.local/share/hyprvx/evidence/claude/<run-id>/`, plus a row in `checklists/verification.md`.
- Dispatcher replies and `hyprctl hyprvx state` are never pass conditions.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelizable (different files, no dependency on an incomplete task).
- **[Story]**: US1–US5 from `spec.md`. Setup, Foundational, mechanism, delivery and gate phases have no story label,
  except mechanism tests, which carry the story they verify.
- **Owner**: Claude for every task (DV3-15).
- **Ticking**: a task is ticked after its verification ran and its evidence is archived. Commits keep the trailers
  `Task:`, `Verify:`, `Result:`, `Evidence:`, `Not-run:`.
- **Heavy jobs** (Hyprland build, nested session, plugin build against a prefix) run only under the heavy lock with
  the memory checks of test-tooling §2. The MemAvailable threshold is never lowered. Native runs need the host GPU
  profile Integrated.
- **Forms**: native tests take `HVX_FORM=P|S` (test-tooling 1.4 §9). Until Phase 9 they run in form P only, or with
  the dev control on the patched build for mechanism tests.

## Scope boundary

| In Feature 002 | Not in Feature 002 |
|---|---|
| Drift state machine; cameras per workspace; window offsets; camera controls without trackpad; window move by offset; floating resize; spawn placement; popups, subsurfaces, X11; both delivery forms | Trackpad gestures and momentum (Phase 4); snap, clusters, pin, navigation, bookmarks (Phase 5): scenario steps 9 (snap part) and 10; Caelestia layer (Phase 6); performance budgets (Phase 7); unified world (Milestone U; U0 code unused); multi-monitor (Phase 8); packaging and host install (Phase 9) |

---

## Phase 1: Setup

- [X] T001 Create `specs/002-multi-window-drift-mvp/integration-log.md` (append-only, same columns as Feature 001's) and `specs/002-multi-window-drift-mvp/checklists/verification.md` with rows V-01..V-28, all `NOT RUN`, mapped as: V-01 transform 1.2 unit tests; V-02 `DriftModel` unit tests; V-03 identity refusal of the new tools and harness commands; V-04 T-MULTI; V-05 T-INPUT-MW; V-06 T-STALE-MW; V-07 T-X11; V-08 T-DRIFT-OFF; V-09 T-PANIC-DRIFT; V-10 T-IDEMP; V-11 T-MONITOR; V-12 T-ANCHOR; V-13 T-PAN; V-14 T-WS-CAMERA; V-15 T-WS-ANIM; V-16 T-KEYS; V-17 T-SCROLL; V-18 T-FULLSCREEN; V-19 T-MOVE; V-20 T-RESIZE; V-21 T-DRAG-ABORT; V-22 T-SPAWN; V-23 form P/S parity; V-24 Feature 001 regression with the plugin loaded; V-25 scenario human review; V-26 host-safety audit; V-27 reproducibility; V-28 single-transform lint
- [X] T002 Move the spike's mechanism sources with `git mv platform/plugin-spike/src platform/plugin/src/mechanism` and `git mv platform/plugin-spike/gen_copies.py platform/plugin/gen_copies.py`. Keep the spike's CMake logic as the form S part of the new `platform/plugin/CMakeLists.txt` (option `HYPRVX_PLUGIN_FORM=P|S`). Record the move in `integration-log.md` (Feature 001 documents cite the spike paths at their own commits) and update the copied-functions entry of `docs/THIRD_PARTY.md` (new path; still nested-only until T052) (Constitution IX)
- [X] T003 Write `scripts/build-plugin.sh --form P|S`: fresh build dir under `~/.cache/hyprvx/claude/build/`, the heavy lock, MemAvailable check, `systemd-run --user --scope -p MemoryMax=6G`, and output `~/.cache/hyprvx/claude/plugin/<form>/hyprvx-drift-<form>.so` plus `build-info.json` (`form`, `prefix_id`, `plugin_sha256`, `sources_sha256`, `git_sha`, compiler); it replaces `scripts/build-plugin-spike.sh`, and `tools/tests/test_build_scripts.py` gets cases for argument validation and the dry run
- [X] T004 [P] Vendor `protocols/virtual-keyboard-unstable-v1.xml` from Hyprland `efb5099` into `tools/vkeyboard/protocols/` and record its license in `docs/THIRD_PARTY.md` (Constitution IX)

---

## Phase 2: Foundational (blocking prerequisites)

### Transform contract 1.2 and the policy model (no compositor)

- [X] T005 Add the invariant tests 8–10 of `contracts/transform-contract.md` 1.2 to `tests/unit/test_camera_math.cpp` (seeded, ≥ 10⁴ cases) and see them fail to compile or fail before the implementation
- [X] T006 Implement `offsetCamera` and `worldDelta` in `core/include/hyprvx/core/CameraMath.hpp` (constexpr, no other change); T005 and all existing tests pass
- [X] T007 Write `tests/unit/test_drift_model.cpp` from `data-model.md` before the model exists. It covers:
  - every transition, and idempotence ("`enable` while `ENTERING` or `DRIFT`, and `disable`/`panic` while `OFF` or `EXITING`, are no-ops");
  - an invalid entering result going to OFF;
  - per-workspace cameras ("`validate` with limits [0.1, 2.0]"; "special workspaces never get a camera");
  - offsets ("finite; default 0", kept across workspace change, dropped on close);
  - drags (`MOVE: O = startOffset + Δ/z`, `PAN: P = startP − Δ/z`, ending on release, leaving DRIFT, panic, window close, scene workspace change);
  - `spawnOffset = centre(visibleWorldRect(C)) − centre(L)`;
  - `mechanismOffset` being 0 outside a DRIFT scene.
- [X] T008 Implement `core/include/hyprvx/core/DriftModel.hpp` and `core/src/DriftModel.cpp` (pure C++20, uses only `CameraMath`, no Hyprland types), add them to `hyprvx_core` in `CMakeLists.txt`; T007 passes; ctest all green (V-01, V-02)

### Test tools and harness (test-tooling 1.4)

- [x] T009 [P] Add `scroll DX DY` to `tools/vpointer/main.cpp` (§4a)
- [x] T010 [P] Add `wl_keyboard` (`kb_enter`, `kb_leave`, `key`, `modifiers`) and pointer `axis` logging to `tools/testclient/main.cpp` (§4); existing log lines unchanged
- [x] T011 [P] Implement `tools/vkeyboard/main.cpp` (`hyprvx-vkeyboard`, §4b): `zwp_virtual_keyboard_v1`, xkbcommon US keymap, `down`/`up`/`tap KEY`, same nested-only refusal as vpointer; CMake target `hyprvx_vkeyboard` under `HYPRVX_WITH_WAYLAND_PLATFORM`
- [x] T012 [P] Implement `tools/x11client/main.cpp` (`hyprvx-x11client`, §4c, xcb): markers, pointer/key logs per window, `drawn` lines, `--menu` override-redirect child with WM_TRANSIENT_FOR; CMake target `hyprvx_x11client`
- [x] T013 Add to `tools/hyprvx_nested.py` and `scripts/nested.sh` (§7):
  - `plugin load PATH`;
  - `xdisplay`: only a lock-file PID equal to the verified compositor or one of its children, else exit 3;
  - `run --x11`.
  Extend `tests/native/harness/test_identity_refusal.sh` with refusal cases for `xdisplay`, `plugin load` on an unverified session, and vkeyboard on a foreign display (V-03).
- [x] T014 Add the window rule for `hyprvx-x11client` to `tests/native/config/hyprland.conf.in` (§5). Add these helpers to `tests/native/lib.sh`:
  - `HVX_FORM` handling;
  - plugin load with sha256 into evidence;
  - `hvx_vkeyboard_start`, `hvx_x11client_start`;
  - a socket2 event logger `tests/native/events_log.py` (new, written for this feature);
  - `hvx_clients_snapshot`.
  The stock baseline must stay byte-identical: re-run GATE-A (`tests/native/gate_a_stock_determinism.sh`) and compare with Feature 001's baseline image `6d7f6bd6…`.
- [x] T015 Plugin skeleton in `platform/plugin/src/policy/` (`main.cpp`: init, version/compatibility check per research R10 reusing `platform/include/hyprvx/platform/PluginCompatibility.hpp`, `hyprctl hyprvx state`, clean unload). Build forms P and S with `scripts/build-plugin.sh`. Load each into its nested build: compositor alive, captures with Drift OFF byte-identical to the build without the plugin.

**Checkpoint**: unit tests green, tools and harness ready, both plugin forms load and are inert.

---

## Phase 3: Mechanism (patches 0010–0013, source of truth; verified with the dev control, no policy)

### Tests (written before the patches are judged; dev offsets via `hyprvx-dev offset`)

- [x] T016 [P] [US2] Write `tests/native/t_multi.sh` (T-MULTI): three floating clients with distinct dev offsets at z ∈ {0.25, 0.5, 1, 2}, each bbox ± 2 px of `forwardBox(L + O)`, scales 1 and 1.25; plus a `--layer TOP` client that stays byte-identical in its region while camera and offsets change (FR-013 with offsets)
- [x] T017 [P] [US3] Write `tests/native/t_input_mw.sh` (T-INPUT-MW): pointer local ± 1 on offset windows, their popup and subsurface, ≥ 20 points per state; click focus; no event on empty canvas; a pointer constraint held by a client stays in force across a camera/offset change (spec edge case; recorded NOT RUN with the reason if the test client cannot hold one)
- [x] T018 [P] [US2] Write `tests/native/t_stale_mw.sh` (T-STALE-MW): 50 random camera/offset states, three windows, partial-damage vs forced full-damage capture identical
- [x] T019 [P] [US3] Write `tests/native/t_x11.sh` (T-X11): X11 window with a dev offset, bbox ± 2 px and pointer local ± 1 at scales 1 and 1.25; its override-redirect menu with the same dev offset drawn attached and receiving input (policy inheritance is tested in T042)
- [x] T020 [P] [US4] Write `tests/native/t_resize.sh` (T-RESIZE) mechanism part: with a non-identity camera, a floating border drag (and Hyprland's resize bind) resizes by `worldDelta` ± 1, a tiled border drag changes nothing and logs once

### Implementation

- [x] T021 Author `patches/hyprland/0010-*.patch`:
  - update the vendored `src/hyprvx/CameraMath.hpp` to equal the core header byte for byte;
  - add offset storage and `setWindowOffset`/`windowOffset`/`contentOffset`/`resetWindowOffsets` (`contracts/mechanism-api.md`);
  - per-window offset camera hint in `renderWorkspaceWindows`, only when the camera is not identity or `O ≠ 0`;
  - culling (`visibleOnMonitor`) and damage (`damageWindow`, `damageSurface`, offset change) on `L + O`;
  - fullscreen ignores offsets.
- [x] T022 Author `patches/hyprland/0011-*.patch`: offsets in `windowAt`, `windowSurfaceAt`, `surfaceLocalAt`, the `mouseMoveUnified` fallback, warps and constraints, `checkInputOnDecos`, and popup reposition/unconstrain (research R8). Any seam outside this list is a stop condition, recorded with file:line in `integration-log.md` before it is added.
- [x] T023 Author `patches/hyprland/0012-*.patch`: floating resize drags through the camera (`worldDelta`, anchor on `L + O`); tiled resize blocked and logged once per drag; move drags stay blocked under camera or offset (FR-012)
- [x] T024 Author `patches/hyprland/0013-*.patch`: dev control 1.2 (`offset`, `state.offsets`, `panic` resets offsets)
- [x] T025 `scripts/check-single-transform.sh` PASS on 0001–0013. Build `patched` and run T016–T020 plus Feature 001's `run_sprint1.sh` and `run_phase2.sh` (regression, identity captures byte-identical to the Feature 001 baseline). Debug until V-04..V-07 and the mechanism part of V-20 PASS, and record each run in `integration-log.md`.

**Checkpoint**: the mechanism draws and hit-tests offset windows correctly with no policy.

---

## Phase 4: User Story 1 — Turn Drift Mode on and off without losing anything (P1) 🎯 MVP

**Goal**: toggle, off, Super+Escape and panic, with the stock image and Hyprland's window model untouched.
**Independent Test**: T-DRIFT-OFF (capture byte-identical to stock, `hyprctl clients` unchanged, no `movewindow*`).

### Tests

- [x] T026 [P] [US1] Write `tests/native/t_drift_off.sh` (T-DRIFT-OFF, ×3): stock capture of the scene; then in the patched build, enter Drift, set a camera and offsets (dispatchers and dev offsets), leave with `hyprvx:off` and with Super+Escape (vkeyboard); compare the capture with stock, compare `hyprctl clients`, check socket2 for `movewindow*`; re-enter and check that the windows are back at their world positions and the camera is restored (US1.4)
- [x] T027 [P] [US1] Write `tests/native/t_panic_drift.sh` (T-PANIC-DRIFT, 10/10): panic from Drift, the first capture after the reply equals stock (FR-002 measurement)
- [x] T028 [P] [US1] Write `tests/native/t_idemp.sh` (T-IDEMP): `on` twice, `off` twice, panic while OFF; captures unchanged after the first call; `hyprvx:on` on a special workspace keeps Drift OFF
- [x] T029 [P] [US1] Write `tests/native/t_monitor.sh` (T-MONITOR): remove the scene output while in Drift (`hyprctl output remove` in the nested session); compositor alive, Drift OFF; re-add, Drift stays OFF; plus plugin unload while in Drift acting as panic

### Implementation

- [x] T030 [US1] Implement the state machine adapter in `platform/plugin/src/policy/Session.cpp`:
  - `DriftModel` per monitor;
  - entering validation (R10) with a notification naming the expected and running build;
  - `hyprvx:toggle`/`on`/`off`/`panic` dispatchers (`contracts/drift-control.md`);
  - on enter, push the scene's camera and offsets to the mechanism;
  - on exit, identity camera and offset 0 for every window, stored state kept;
  - listeners on `monitor.removed` and plugin unload.
- [x] T031 [US1] Implement Super+Escape in `platform/plugin/src/policy/Input.cpp` (keyboard bus listener, cancels only that key press in DRIFT; returns immediately when OFF)
- [x] T032 [US1] Run T026–T029 in form P; debug until V-08..V-11 PASS

**Checkpoint**: Drift can be entered and left safely (MVP).

---

## Phase 5: User Story 2 — See and navigate several real windows (P1)

**Goal**: camera controls without trackpad, a camera per workspace, no workspace-switch animation in Drift.
**Independent Test**: T-ANCHOR, T-PAN, T-WS-CAMERA, T-WS-ANIM, plus T-MULTI with policy-driven cameras.

### Tests

- [x] T033 [P] [US2] Write `tests/native/t_anchor.sh` (T-ANCHOR): 20 zoom steps by `hyprvx:zoom` and by Super + `scroll` at a fixed pointer; cumulative drift of the world point under the pointer ≤ 1 px (measured on the capture of a marker under the pointer)
- [x] T034 [P] [US2] Write `tests/native/t_pan_drift.sh` (T-PAN): `hyprvx:pan dx dy` and a Super + left drag on empty canvas move content by the request ± 1 px; a Super + left drag over a top-layer surface is not a pan
- [x] T035 [P] [US2] Write `tests/native/t_ws_camera.sh` (T-WS-CAMERA): two workspaces with different cameras; each switch shows its own camera; a recreated workspace id gets its stored camera
- [x] T036 [P] [US2] Write `tests/native/t_ws_anim.sh` (T-WS-ANIM): workspace animations enabled in the nested config at runtime; captures right after a switch in Drift equal the settled image; control: the same switch with Drift OFF is animated at the same capture times

### Implementation

- [x] T037 [US2] Implement `hyprvx:pan`, `hyprvx:zoom`, `hyprvx:reset` and the config value `plugin:hyprvx:zoom_step` in `platform/plugin/src/policy/Camera.cpp`, through `DriftModel` and `CameraMath` only
- [x] T038 [US2] Implement Super + wheel (zoom per detent, value120-based) and Super + left drag on empty canvas (pan) in `platform/plugin/src/policy/Input.cpp`; empty canvas = no window content and no top/overlay layer surface under the pointer (FR-007)
- [x] T039 [US2] Implement per-workspace cameras and animation suppression on `workspace.active` in `platform/plugin/src/policy/Session.cpp` (R2, R7)
- [x] T040 [US2] Run T033–T036 and T016 (with policy cameras) in form P; debug until V-12..V-15 PASS

---

## Phase 6: User Story 3 — Work in a real window inside Drift Mode (P1)

**Goal**: click, type, scroll and menus in offset windows, X11 included; fullscreen as stock.
**Independent Test**: T-KEYS, T-SCROLL, T-INPUT-MW, T-X11 with policy inheritance, T-FULLSCREEN.

### Tests

- [x] T041 [P] [US3] Write `tests/native/t_keys.sh` (T-KEYS) and `tests/native/t_scroll.sh` (T-SCROLL): key taps reach only the clicked offset window; `scroll` reaches the window under the pointer, while Super + `scroll` zooms and the client gets nothing
- [x] T042 [P] [US3] Extend `tests/native/t_x11.sh` with a policy case: the override-redirect menu takes its parent's offset (and the focused-window fallback when it has no parent), drawn attached and receiving input
- [x] T043 [P] [US3] Write `tests/native/t_fullscreen.sh` (T-FULLSCREEN): a fullscreen window in Drift is byte-identical to the stock fullscreen capture; leaving fullscreen restores the Drift view

### Implementation

- [x] T044 [US3] Implement override-redirect offset inheritance (R9) on window open in `platform/plugin/src/policy/Windows.cpp`; logged when it falls back to 0
- [x] T045 [US3] Run T041–T043, T017 and T019 in form P; debug until V-05, V-07, V-16..V-18 PASS

---

## Phase 7: User Story 4 — Move windows in the canvas (P2)

**Goal**: Super + left drag moves a window by changing only its offset; floating resize works; drags end cleanly.
**Independent Test**: T-MOVE, T-RESIZE, T-DRAG-ABORT.

### Tests

- [x] T046 [P] [US4] Write `tests/native/t_move.sh` (T-MOVE): drags of d at z ∈ {0.5, 1, 2}; bbox moves by d ± 1 px, `O` changes by `worldDelta`, `hyprctl clients` `at`/`size` unchanged, no `movewindow*`; the user's `bindm` move never runs in Drift
- [x] T047 [P] [US4] Write `tests/native/t_drag_abort.sh` (T-DRAG-ABORT): panic, `hyprvx:off`, client quit and workspace change during a drag; compositor alive, no stuck grab (the next click reaches a client)

### Implementation

- [x] T048 [US4] Implement the move drag in `platform/plugin/src/policy/Input.cpp` and `Windows.cpp` (Super + left button on a window, `O += worldDelta(C, Δ)` through `DriftModel`, cancelled before keybinds, ended on every `data-model.md` drag end condition)
- [x] T049 [US4] Run T046, T047 and T020 in form P; debug until V-19..V-21 PASS

---

## Phase 8: User Story 5 — New windows appear where I am looking (P3)

- [x] T050 [P] [US5] Write `tests/native/t_spawn.sh` (T-SPAWN): a window mapped in Drift under a panned, zoomed camera is centred in the viewport ± 2 px; mapped with Drift OFF, it is placed as stock
- [x] T051 [US5] Implement spawn placement (R6) in `platform/plugin/src/policy/Windows.cpp`. Choose the earliest window event that has `L(w)` and draws no frame before the offset is set, and record the choice with file:line in `integration-log.md`. Run T050 in form P until V-22 PASS.

---

## Phase 9: Delivery form S (DV3-16)

- [ ] T052 Regenerate the copied functions with `platform/plugin/gen_copies.py` from the patched draft tree (0001–0013), list every new hook target with its demangled signature, and confirm each has a dynamic symbol in the stock binary. A target without a symbol is a stop condition (recorded, then designed around) before T053. Update the copied-functions entry of `docs/THIRD_PARTY.md` with the full list of copied functions and helpers, their count, and the plugin's status as the DV3-16 delivery form (Constitution IX).
- [ ] T053 Implement the mechanism API with offsets in `platform/plugin/src/mechanism/` (offset map keyed by window, same rules as `contracts/mechanism-api.md`) and build form S with `scripts/build-plugin.sh --form S`
- [ ] T054 Run every Feature 002 test (T016–T050) in form S at scales 1 and 1.25; compare every capture with form P byte for byte (V-23); run the stability checks (refused by a mismatched build, unload while in Drift)

---

## Phase 10: Feature 002 gate

- [ ] T055 [P] Write `tests/native/t_scenario.sh` (T-SCENARIO, SC-008). It scripts the 12-step scenario with test clients standing in for VS Code and Firefox:
  - steps 3, 5 and 7 by `hyprvx:zoom`, `hyprvx:pan` and Super + wheel/drag;
  - step 8 by click, keys and scroll;
  - step 9 as move;
  - step 10 skipped with a note (Phase 5);
  - steps 11 and 12 by Super+Escape and a stock comparison.
  It captures every step and runs in forms P and S.
- [ ] T056 Write `tests/native/run_feature002.sh` (all §9 tests, both forms, both scales); run it, plus `run_sprint1.sh` and `run_phase2.sh` in both forms with the plugin loaded and Drift OFF (V-24)
- [ ] T057 R1 clean-room: rebuild `patched` and both plugin forms from fresh worktrees and build directories without ccache, re-run `run_feature002.sh`, and write `specs/002-multi-window-drift-mvp/r1-source-verification.md` before reading the new results. The report covers, with file:line:
  - FR-003: no pass element and no cancelled event while OFF;
  - Constitution IV: no inline camera arithmetic in 0010–0013 or the policy;
  - Constitution V: no write of workspace, `L`, tiling or layout outside the floating resize;
  - FR-013: layer, lock, IME and cursor never offset;
  - FR-016: the dev control is absent from form S and from a non-dev build (symbol check on the binaries, and `hyprvx-dev` rejected);
  - every test changed after code existed, with its reason.
  Then do a second clean build for reproducibility (V-27).
- [ ] T058 [P] V-26 host-safety audit of every nested session of this feature (no host signature, private runtime dirs, agent-prefix binaries only, host `hypr`/`caelestia` config unchanged), and V-28 lint record
- [ ] T059 Assemble the T-SCENARIO image set (both forms) with paths, run-ids and sha256 in `checklists/verification.md` for the user's review (V-25, NOT AUTOMATABLE)
- [ ] T060 Update `checklists/verification.md` (V-01..V-28 with archive paths and `sha256sum -c` results), `integration-log.md`, `.ai/CURRENT_STATE.md` and `.ai/NEXT_TASK.md` with honest statuses; validate `quickstart.md` by running it and correct any drift; constitution re-check of the delivered code in `plan.md`
- [ ] T061 Merge-readiness review: every commit since `f2c8a9a` reviewed, every PASS backed by archived evidence, `git rev-parse dev == 15f75b0`, nothing pushed, U0 code absent from this branch
- [ ] T062 **Feature 002 gate**: present the evidence, the scenario review and the open items. STOP for the user's approval before merging into `v3/spec-foundation` or starting any later feature.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (1)** → **Foundational (2)** → **Mechanism (3)** → **US1 (4)** → US2 (5), US3 (6), US4 (7) and US5 (8) in priority
  order → **Form S (9)** → **Gate (10)**.
- US2–US5 depend on US1 (Drift must exist) and on the mechanism; they are independent of each other except that they
  share `Input.cpp`, `Windows.cpp` and `Session.cpp` (sequential edits).
- Form S (9) needs every mechanism patch (3) and the full policy (4–8).

### Task dependencies (non-obvious)

- T005 before T006, and T007 before T008 (test first).
- T004 before T011.
- T013 and T014 before any native test run.
- T015 before T030.
- T016–T020 before T021–T025 are judged.
- T021 before T022 and T023.
- T024 before T025 (mechanism tests use `hyprvx-dev offset`).
- T026–T029 before T030 is judged; the same holds in each later story phase (tests before the implementation they
  judge).
- T052 before T053.
- T055 needs US1–US4.
- T057 runs on the final commit; T062 is last.

### Execution order (single agent)

T001 → T015 sequentially, with [P] tasks in any order inside a group. Then T016–T020 (tests) → T021–T025; then each
story phase as tests → implementation → run; then T052–T054; then T055–T062.

## Parallel Example: Phase 3 tests

```text
T016 tests/native/t_multi.sh   T017 tests/native/t_input_mw.sh   T018 tests/native/t_stale_mw.sh
T019 tests/native/t_x11.sh     T020 tests/native/t_resize.sh
```

## Implementation Strategy

1. **MVP**: Phases 1–4. Drift can be entered and left safely, offset windows draw and hit-test correctly, and OFF is
   stock.
2. **Increments**: US2 (navigation) → US3 (work in windows) → US4 (move) → US5 (spawn). Each is independently testable
   with its T-* scripts.
3. **Delivery**: form S once the policy is complete; byte parity with form P.
4. **Gate**: scenario review by the user, R1 clean-room, then STOP at T062.

## Notes

- Scenario steps 9 (snap part) and 10 (pin) are Phase 5 per the roadmap; the scenario script marks them skipped, it
  does not fake them.
- Every new seam outside research R8 is a stop condition: record it with file:line before adding it.
- `checklists/drift.md` items are reviewer-owned and stay unchecked unless the user reviews them.
