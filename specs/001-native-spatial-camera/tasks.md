---
description: "Task list for Feature 001 — Native Spatial Camera (Claude Code only since DV3-15)"
---

# Tasks: Native Spatial Camera (Feature 001)

**Input**: `specs/001-native-spatial-camera/` — `spec.md`, `plan.md`, `research.md`, `data-model.md`,
`contracts/transform-contract.md` (v1.1), `contracts/dev-control.md` (v1.1), `contracts/test-tooling.md` (v1.2),
`quickstart.md`, `checklists/verification.md`.
**Governance**: constitution v1.0.0; master plan §I (Sprint 1); decisions DV3-09..DV3-15.

**Status**: approved by the user at `966e6b6`; Sprint 1 (T001–T053) in progress. Since DV3-15 (2026-10-11) Claude
Code is the only implementation agent: every remaining task is owned by Claude. Tasks completed by Codex keep
`(owner: codex)` as a historical record; their evidence stays archived.

**Tests**: REQUIRED (Constitution II). Native tests are written from the spec and exist before any patch is judged
(infra-before-claims). Every PASS needs raw evidence that is checksum-verified and archived into
`~/.local/share/hyprvx/evidence/<agent>/<run-id>/` (`contracts/test-tooling.md` §8) and a row in
`checklists/verification.md`. A commit trailer never points to evidence that is not on this machine with a verifying
`SHA256SUMS`. A dispatcher reply is never a pass condition.

## Format: `[ID] [P?] [Story] Description (owner: …)`

- **[P]**: parallelizable — different files and no dependency on an incomplete task.
- **[Story]**: US1–US4 from `spec.md`. Setup, Foundational and gate phases have no story label.
- **Owner**: `claude` for every open task (DV3-15). `(owner: codex)` on a completed task records who did it.
  A task is ticked after its verification ran and its evidence is archived; commits keep the trailers `Task:`,
  `Verify:`, `Result:`, `Evidence:`, `Not-run:`.
- **Heavy jobs** (Hyprland build, nested session) run only under the heavy lock with the memory checks in
  `contracts/test-tooling.md` §2. Scripts run with `HYPRVX_AGENT=claude` and the Claude cache root.

## Scope boundary

| Part | Phases | Status |
|---|---|---|
| **Sprint 1** (master plan Phase 0 + 1) | 1–6: Setup, Foundational, US1, US2, US4, Sprint 1 gate | Approved, in progress |
| **Phase 2** (master plan Phase 2) | 7–9: US3 input + popups, delivery spike, Feature 001 gate | **BLOCKED** until T053 (Sprint 1 gate) passes and the user approves |
| Not in Feature 001 | U0 spike (DV3-12), window move via `O(w)` (Feature 002) | — |

---

# SPRINT 1

## Phase 1: Setup (shared infrastructure)

**Purpose**: freeze interfaces and prepare caches and builds (T002/T003 were for the retired Codex lane; done, kept as history).

- [X] T001 K0 contract freeze: confirm `specs/001-native-spatial-camera/contracts/transform-contract.md` is v1.1 and `contracts/test-tooling.md` is v1.0, then create `specs/001-native-spatial-camera/integration-log.md` with the columns task, branch@sha, verify command, result, evidence run-id, archive path, `sha256sum -c` result, review verdict, ownership transfers (owner: claude)
- [X] T002 Create the Codex clone and per-agent caches exactly as master plan §O3: `git clone --no-hardlinks --branch v3/001-native-spatial-camera /home/towfienes/Projects/HyprVX /home/towfienes/Projects/HyprVX-v3-codex`, branch `v3/001-codex`, push URL `DISABLED`, `~/.cache/hyprvx/{claude,codex,locks}`, `~/.cache/hyprvx/locks/heavy.lock`, evidence archive `~/.local/share/hyprvx/evidence/{claude,codex}` (test-tooling §1, §8); record in `specs/001-native-spatial-camera/integration-log.md` (owner: claude)
- [X] T003 S0 sandbox smoke test, run by the user-launched Codex session in `/home/towfienes/Projects/HyprVX-v3-codex` and recorded as PASS/BLOCKED per item: write under `~/.cache/hyprvx/codex`; `flock` on the read-only `~/.cache/hyprvx/locks/heavy.lock`; `systemd-run --user --scope -p MemoryMax=1G true`; read access to `/dev/dri`; `git commit` via on-request escalation; command outputs saved as an evidence run per test-tooling §8 (if the cache is not writable, the item is BLOCKED and the outputs are given in the session for the user and Claude); items BLOCKED fall back to the user or Claude (owner: codex)
- [X] T004 [P] Add `build-v3/` and the evidence policy of `contracts/test-tooling.md` §8 to `.gitignore` (raw evidence lives outside the repo; as a safeguard `evidence/*/*` is ignored except `summary.json` and `SHA256SUMS`) (owner: codex)
- [X] T005 [P] Add options `HYPRVX_WITH_SDL`, `HYPRVX_WITH_WAYLAND_PLATFORM`, `HYPRVX_WITH_HYPRLAND_PLUGIN` (default OFF) to `CMakeLists.txt`; each gates its targets, tests (`test_native_event_bridge.cpp`, `test_overlay_state.cpp`, `test_plugin_compatibility.cpp` as applicable) and dependency discovery (`CMakeLists.txt:161-211`); replace absolute `/usr/share/wayland-protocols` paths with `pkg_get_variable(wayland-protocols pkgdatadir)` (owner: codex)
- [X] T006 V-05: configure fresh `build-v3/` headless (all options OFF) and fully enabled; build `-j2`; run `ctest --test-dir build-v3 --output-on-failure`; save configure/build/ctest logs for both configurations as an evidence run per test-tooling §8 and cite its run-id in the commit trailer (owner: codex)

**Checkpoint**: interfaces frozen; headless core build and V1 tests reproducible.

---

## Phase 2: Foundational (blocking prerequisites)

**Purpose**: the single transform, the reproducible build pipeline, the hardened harness and the measurement
tools. No user story can be judged before this phase and GATE-A pass.

### Camera math (contract v1.1)

- [X] T007 Implement `core/include/hyprvx/core/CameraMath.hpp` exactly as the frozen C++ interface in `contracts/transform-contract.md` (namespace `hyprvx::cammath`, header-only, C++20, `SPDX-License-Identifier: BSD-3-Clause`, identity short-circuit, `ZoomLimits{0.1, 2.0}` default, `damageBox` = roundOut + 1 px); record the license in `docs/THIRD_PARTY.md` (owner: claude)
- [X] T008 Make `core/src/Camera.cpp` delegate `worldToScreen`, `screenToWorld`, `visibleWorldRect` and the anchor math to `CameraMath` with `M = {0,0}` and unbounded limits, keeping `setZoom` validation (throws, no clamping); existing `tests/unit/test_camera.cpp` and simulators must pass unchanged (V-25 part 1) (owner: claude)
- [X] T009 Write `tests/unit/test_camera_math.cpp` (invariants 1–7 of the contract, ≥ 10⁴ seeded cases, seed printed on failure, independent oracle in `long double` and hand-computed dyadic values, non-zero `M` identity exactness, rejection cases, V1 equivalence) and register it in `CMakeLists.txt`; written from the contract, not from the header's code (V-06, V-25) (owner: claude)

### Reproducible build pipeline (`contracts/test-tooling.md` §1–§3)

- [X] T010 [P] Write `scripts/fetch-hyprland.sh`: clone `hyprwm/Hyprland` with submodules into `~/.cache/hyprvx/$HYPRVX_AGENT/src/hyprland-efb5099`, verify `git rev-parse HEAD == efb50993780079460b0cbed1363e2166a2de1d9f`, create per-variant git worktrees `stock`, `v2`, `v3-dev`; refuse when `HYPRVX_AGENT` is unset (owner: codex)
- [X] T011 [P] Write `scripts/build-scanner.sh`: build `hyprwayland-scanner` v0.4.6 into `~/.cache/hyprvx/$HYPRVX_AGENT/tools` without system installation (owner: codex)
- [X] T012 Write `scripts/apply-patches.sh`: `git am` an ordered patch directory (`patches/hyprland/` or `research/v2-drift-mvp/` for variant `v2`, none for `stock`) onto the variant worktree, using only `*.patch` files in lexical order (e.g. `research/v2-drift-mvp/*.patch`, never `PROVENANCE.md`/`SHA256SUMS`); print `patchset_id = sha256(cat patches in order)[:12]` (owner: codex)
- [X] T013 Write `scripts/build-hyprland.sh --variant stock|v2|patched`: heavy lock, `MemAvailable ≥ 6.5 GiB` check, PSI abort, `systemd-run --user --scope -p MemoryMax=6G`, CMake + Ninja `-j4`, ccache in the agent cache, `-DNO_HYPRPM=TRUE`, real `GIT_COMMIT_HASH`, `GIT_DIRTY=hyprvx-<patchset_id>` (or `clean-upstream` for stock), `HYPRVX_DEV` defined for `patched`, install to `~/.cache/hyprvx/$HYPRVX_AGENT/prefix/<patchset_id|stock|v2-…>`, write `build-info.json` per test-tooling §3 (owner: codex)
- [X] T014 V-01, V-03: run fetch + build for variant `stock` (heavy job); copy `build-info.json` and the build log into an evidence run per test-tooling §8 and cite its run-id in the commit trailer (owner: claude)

### Hardened nested harness (Constitution I, test-tooling §7)

- [X] T015 Port and harden `tools/hyprvx_nested.py`: `--binary` from the agent prefix (never `$PATH`), re-verify `SO_PEERCRED` PID + process start time + signature ≠ host + private `/tmp/hvx-*` before every command including `stop`, `HYPRLAND_NO_SD_VARS=1`, `dbus-run-session`, `recover` for stale sessions, logs kept after stop; reuse `platform/src/NestedGuard.cpp` semantics (owner: claude)
- [X] T016 Write `scripts/nested.sh` with the subcommands and exit codes of test-tooling §7, holding the heavy lock from `start` to `stop` (owner: claude)
- [X] T017 V-07: negative tests in `tests/native/harness/test_identity_refusal.sh` (wrong PID, wrong start time, host signature, non-private runtime dir ⇒ exit 3 and nothing sent; `stop` on a stale session signals nothing it cannot verify) (owner: claude)

### Measurement tools (test-tooling §4–§6)

- [X] T018 [P] Implement `tools/testclient/` (`hyprvx-testclient`): deterministic content, colors and marker sizes exactly as test-tooling §4, `--control` FIFO commands `fill`/`redraw`/`sync`/`quit`, JSONL log with `seq`/`frame_done`, `--layer TOP|OVERLAY`, `--scale-aware`; CMake target under `HYPRVX_WITH_WAYLAND_PLATFORM` in `CMakeLists.txt` (owner: claude)
- [X] T019 [P] Implement `tools/analyze_frame.py` (`python -I`, modes `bbox` and `diff`, JSON output and exit codes of test-tooling §6) with synthetic-image tests in `tools/tests/test_analyze_frame.py` whose expected answers are computed independently of the analyzer (owner: claude)
- [X] T020 [P] Write the shared nested config template `tests/native/config/` (animations off, fixed gaps/borders, identical for all variants; format verified against Hyprland `efb5099` source) and the shared helpers `tests/native/lib.sh` (run-id, ready-signal capture rule of test-tooling §5, raw evidence written to `~/.cache/hyprvx/$HYPRVX_AGENT/evidence/<run-id>/`, `summary.json` and a final `SHA256SUMS` copied to the repo index `evidence/<run-id>/` per test-tooling §8, BLOCKED on timeout) (owner: claude)
- [X] T021 Write `scripts/check-single-transform.sh` (Constitution IV lint): fail if any `patches/hyprland/*.patch` hunk outside the vendored `CameraMath.hpp` contains inline camera arithmetic (`/ zoom`, `* zoom`, `/z`, `*z` on camera fields) or a second forward/inverse formula; self-test against a synthetic bad patch (owner: claude)

### GATE-A — deterministic baseline

- [X] T022 GATE-A (V-23): write `tests/native/gate_a_stock_determinism.sh` and run it — three nested stock sessions, reference scene (one 800×600 testclient at a fixed position), captures pixel-identical by `analyze_frame.py diff`; on failure fix the harness, never a tolerance (owner: claude)

**Checkpoint**: CameraMath integrated and tested; stock built reproducibly; harness refuses wrong targets; tools
proven on synthetic data; GATE-A PASS. Claude may write patches during this phase, but none is judged yet.

---

## Phase 3: User Story 1 — Normal Mode stays stock (Priority: P1) 🎯 MVP

**Goal**: the patched build with identity camera is indistinguishable from stock-from-source.
**Independent Test**: T-ID — same scene in stock and patched nested sessions, captures pixel-identical ×3.

### Tests for User Story 1 (written before patches are judged)

- [X] T023 [US1] Write `tests/native/t_id.sh`: stock vs patched identity, same scene and config, `diff` with `--max-pixels 0`, three repetitions; records `hyprvx-dev:state` `passElementsAdded` as a supplementary diagnostic artifact only. US1.2 evidence is this pixel identity plus the independent source verification of the FR-002 identity early return in T044 (a dispatcher readout is never a pass condition, Constitution II) (owner: claude)

### Implementation for User Story 1

- [X] T024 [US1] Prepare the Claude upstream worktree `~/.cache/hyprvx/claude/src/hyprland-efb5099/v3-dev` with `scripts/fetch-hyprland.sh` (manual clone of the same commit is acceptable for drafting; the scripted fetch is used for every judged build) (owner: claude)
- [X] T025 [US1] Author `patches/hyprland/0001-*.patch`: vendor `CameraMath.hpp` unchanged, add per-monitor camera state (`origin`, `zoom`, `revision` per `data-model.md`: origin finite; zoom finite and `z_min ≤ z ≤ z_max` (MVP 0.1 … 2.0); revision incremented on every change), default `identityFor(M)`, no render change (owner: claude)
- [X] T026 [US1] V-09: build `patched` (heavy job, Claude cache) and run `tests/native/t_id.sh` ×3 against the stock build of T014; record evidence; T-ID FAIL is a stop condition (owner: claude)

**Checkpoint**: identity path proven for the current series head. T-ID is re-run after every later patch (T040).

---

## Phase 4: User Story 2 — Real window visibly zooms and pans correctly (Priority: P1)

**Goal**: a real window is drawn at the size and position the contract predicts, complete, without stale pixels.
**Independent Test**: T-Z025/T-Z05/T-Z2, T-PAN, T-STALE, T-STALE-STATIC and layer isolation on the patched build.

### Tests for User Story 2 (written before patches are judged)

- [X] T027 [P] [US2] Write `tests/native/t_zoom.sh`: z = 0.25 and 0.5 with an 800×600 window, z = 2.0 with a 400×300 window (spec US2.1–2.2), expected boxes from `contracts/transform-contract.md` forward, ± 2 px, four markers (owner: claude)
- [X] T028 [P] [US2] Write `tests/native/t_pan.sh`: pans of ±300 px on both axes at z ∈ {0.5, 1.0}, displacement ± 2 px (owner: claude)
- [X] T029 [P] [US2] Write `tests/native/t_stale.sh`: 50 seeded random camera states (seed recorded); each frame vs the frame after `hyprvx-dev:full-damage`, 0 mismatches (US2.3) (owner: claude)
- [X] T030 [P] [US2] Write `tests/native/t_stale_static.sh`: fixed non-identity camera, deterministic `fill` sequence through the testclient control FIFO, each partial-damage frame vs a forced full-damage frame of the same client state, 0 mismatches (US2.5) (owner: claude)
- [X] T031 [P] [US2] Write `tests/native/t_layer.sh`: `hyprvx-testclient --layer TOP` region identical with identity and non-identity camera; window decorations enclose the transformed surface (US2.4, V-14) (owner: claude)
- [X] T032 [US2] Write `tests/native/run_sprint1.sh`: runs T-ID, US2 and US4 scripts at scale 1 and 1.25 (V-15) and aggregates `summary.json` results (owner: claude)

### Negative control (U1, GATE-N)

- [X] T033 [US2] V-08: build variant `v2` (heavy job) and capture z = 0.5 / 0.25 and a zoom/pan sequence, driving V2 through its own `hyprvx:*` dispatchers (research R2: `KeybindManager.cpp:123-266`, zoom clamp [0.2, 1.5], centre-anchored) via a variant adapter in `tests/native/lib.sh`, since V2 has no `hyprvx-dev:*` control; in the same run-id also capture the V2 identity image and record the inputs for checks S1 (variant provenance) and S2 (camera applied, judged on captures) of test-tooling §6.1 (owner: claude)
- [X] T034 [US2] GATE-N (V-24): run the analyzer on the T033 captures, on the stock identity capture of GATE-A and on synthetic known-good fixtures; write a GATE-N report in the run's `summary.json` that evaluates S1, S2, P and N and classifies the outcome exactly per the decision table in test-tooling §6.1 (PASS / BLOCKED invalid setup / FAIL defective analyzer / BLOCKED V2 defect not reproduced). Claude performs check H (visual confirmation on the archived PNGs) during integration review; GATE-N is PASS only when S1, S2, P, N and H all hold. Any other outcome stops Sprint 1 patch judging, and an alternative negative control needs user approval (owner: claude)

### Implementation for User Story 2 (Claude lane; all hunks call `CameraMath`, Constitution IV)

- [X] T035 [US2] Author `patches/hyprland/0002-*.patch` render seam: hint at `RENDER_PRE_WINDOWS` composed with any existing geometry modifier, reset before `RENDER_POST_WINDOWS`; no hint and no `noSimplify` when identity (FR-002); `drawSurface` clip, `visibleRegion` and `clipBox` via `forwardBox` (RC1); `noSimplify` while non-identity (RC2); special-workspace dim rect outside the hint; corner radius × z; direct scanout / solitary blocked while non-identity (likely not exercised by the nested Wayland backend; record as NOT AUTOMATABLE (nested) with source evidence at the gate) (owner: claude)
- [X] T036 [US2] Author `patches/hyprland/0003-*.patch` culling: `shouldRenderWindow` / `visibleOnMonitor` use `visibleWorldRect ∩ W(w)` (FR-006; window outside the monitor's logical rect but inside the visible world rect is rendered) (owner: claude)
- [X] T037 [US2] Author `patches/hyprland/0004-*.patch` damage: `damageSurface` / `damageWindow` / `damageBox` / `damageRegion` for window-owned content via `cammath::damageBox` (decorations included); camera change damages the whole output; layer popups/subsurfaces never transformed (FR-005) (owner: claude)
- [X] T038 [US2] Author `patches/hyprland/0005-*.patch` dev control exactly as `contracts/dev-control.md` (`camera`, `camera-mon`, `panic`, `state`, `full-damage`), compiled only with `HYPRVX_DEV`, validation through `cammath::validate` (FR-008, FR-009) (owner: claude)
- [X] T039 [US2] Run `scripts/check-single-transform.sh` on the series; fix every finding in the patches (owner: claude)
- [X] T040 [US2] Build `patched` (0001–0005), run `tests/native/run_sprint1.sh` after GATE-A (T022) and GATE-N (T034) pass; debug and fix in the patches until V-09, V-10, V-11, V-12, V-12b, V-14, V-15 PASS or a stop condition is reached; log each run (owner: claude)

**Checkpoint**: US2 geometry, stale-frame and layer checks PASS at scale 1 and 1.25, T-ID still PASS.

---

## Phase 5: User Story 4 — Safe return to normal (Priority: P2)

**Goal**: `panic` always returns to the identity image in one frame; invalid camera requests never change state.
**Independent Test**: T-OFF 10/10 and invalid-request rejection judged by captures.

### Tests for User Story 4

- [X] T041 [P] [US4] Write `tests/native/t_off.sh`: from random non-identity cameras, `hyprvx-dev:panic`, next ready frame equals the T-ID image, 10/10 (V-13, SC-004) (owner: claude)
- [X] T042 [P] [US4] Write `tests/native/t_invalid.sh`: NaN, Inf, z ≤ 0, z < 0.1, z > 2.0 requests; capture before and after is identical (state readout recorded as diagnostic only) (US4.2) (owner: claude)

### Implementation for User Story 4

- [X] T043 [US4] Ensure `panic` sets every camera to `identityFor(M)` exactly and disables the scene, and setters leave the camera unchanged on rejection, in `patches/hyprland/0001-*.patch` / `0005-*.patch`; after GATE-A (T022) and T-ID (T026) pass, run `t_off.sh` and `t_invalid.sh` until V-13 PASS (owner: claude)

**Checkpoint**: Sprint 1 user stories complete.

---

## Phase 6: Sprint 1 gate

- [X] T044 R1 clean-room re-verification (DV3-15: author and verifier are now the same agent): rebuild `patched` from the committed patches in freshly fetched worktrees and fresh build directories, re-run `tests/native/run_sprint1.sh`, and write the source-verification report before reading the new results; the report also lists every test changed after patches existed, with its reason from `integration-log.md`. The report MUST verify from source and cite file:line for: (a) FR-002 — with identity camera no hint, no `noSimplify` and no extra pass element is added (US1.2 evidence together with T-ID); (b) Constitution IV — no inline camera arithmetic; (c) RC1/RC2 coverage; (d) FR-004 — lock screen, IME, notifications and software cursor are emitted outside the PRE_WINDOWS…POST_WINDOWS hint (stage order, research R3), recorded as source evidence because they are not exercised natively in Sprint 1. A disagreement between the re-run and earlier results is reproduced on the same commit and `build-info.json` before any task is ticked (owner: claude)
- [X] T045 [P] Informational cost measurement (plan: "Drift frame cost is measured, not optimized"; Constitution VI; NOT a gate): write and run `tests/native/m_cost.sh` — nested compositor `utime+stime` from `/proc/<pid>/stat` and frames presented over a fixed 10 s dev-control camera sequence and a 60 s idle window, for stock, patched identity and patched non-identity; median and [min, max] of ≥ 5 runs with conditions per master plan §E1; results recorded in `checklists/verification.md` notes (owner: claude)
- [X] T046 [P] V-02, V-04: clean rebuild twice from scratch dirs; same `patchset_id` and same test results (reproducibility levels input + test-result; binary sha256 reported as observed) (owner: claude)
- [X] T047 [P] V-22: audit harness logs and host state notes — no host `hyprctl`, plugin or config change during Sprint 1 runs; record in `checklists/verification.md` (owner: claude)
- [X] T048 V-20 preparation: assemble the Sprint 1 image set (paths and run-ids) in `specs/001-native-spatial-camera/checklists/verification.md` for human review; the user signs SC-005 (NOT AUTOMATABLE) (owner: claude)
- [X] T049 Update `checklists/verification.md` (V-01..V-15, V-20, V-22..V-25), `integration-log.md`, `.ai/CURRENT_STATE.md` and `.ai/NEXT_TASK.md` with archive paths (`~/.local/share/hyprvx/evidence/<agent>/<run-id>/`), run-ids, `sha256sum -c` results and honest statuses (owner: claude)
- [X] T050 Validate `quickstart.md` steps 1–4 against the delivered scripts; correct documentation drift in `specs/001-native-spatial-camera/quickstart.md` (owner: claude)
- [X] T051 Constitution re-check of delivered Sprint 1 code against principles I–X; record exceptions in `plan.md` Complexity Tracking (owner: claude)
- [ ] T052 Merge-readiness review: every Sprint 1 commit since `966e6b6` reviewed (including the integrated Codex commits), every PASS backed by archived evidence, `git rev-parse dev == 15f75b0` (owner: claude)
- [ ] T053 **G1 — Sprint 1 gate**: present results; STOP for user approval before any Phase 2 task (owner: claude)

---

# MASTER-PLAN PHASE 2 (input proof + delivery spike) — BLOCKED until T053 passes and the user approves

Note: "Phase N" headings in this file are Spec Kit task phases; "master-plan Phase 0/1/2" refers to
`docs/v3/HYPRVX_V3_MASTER_PLAN.md` §H. Sprint 1 = master-plan Phases 0+1 = task Phases 1–6.

## Phase 7: User Story 3 — Pointer input lands where the window is drawn (Priority: P1, scheduled after Sprint 1)

**Goal**: pointer focus, motion and buttons reach the drawn window at matching surface-local coordinates.
**Independent Test**: virtual pointer at screen points S on the verified nested display; testclient JSONL vs
`surfaceLocal` ± 1 unit.

### Tests for User Story 3

- [ ] T054 [US3] Vendor `wlr-virtual-pointer-unstable-v1.xml` under `tools/vpointer/protocols/` and implement `tools/vpointer/` (`zwlr_virtual_pointer_v1`, absolute motion and buttons, connects only to the nested `WAYLAND_DISPLAY` given by `scripts/nested.sh`); starts only after T053 and user approval, like every Phase 2 task (owner: claude)
- [ ] T055 [US3] Record the license of the vendored virtual-pointer protocol XML in `docs/THIRD_PARTY.md` (Constitution IX) before T054 is integrated, and bump `specs/001-native-spatial-camera/contracts/test-tooling.md` to v1.1 with the testclient `--popup`/`--subsurface` modes and the `hyprvx-vpointer` CLI before T056/T057 start (owner: claude)
- [ ] T056 [P] [US3] Add an `xdg_popup` mode (`--popup` at a fixed parent-relative offset, popup logs its own JSONL events) and a `wl_subsurface` mode (`--subsurface` with distinct marker colors) to `tools/testclient/`, per `contracts/test-tooling.md` v1.1 from T055 (owner: claude)
- [ ] T057 [P] [US3] Write `tests/native/t_pointer.sh`: z = 0.25 plus pan, ≥ 20 sampled points per camera state, `enter`/`motion` local = `surfaceLocal(S)` ± 1 (US3.1, V-16) (owner: claude)
- [ ] T058 [P] [US3] Write `tests/native/t_focus.sh`: click focuses the window drawn under S; motion over empty canvas reaches no window (US3.2–3.3, V-17) (owner: claude)
- [ ] T059 [P] [US3] Write `tests/native/t_popup.sh`: popup and subsurface drawn attached to their parent, transformed and clipped to transformed bounds (FR-003), and hit-testable when zoomed (US3.4, V-18); popup mapped outside every monitor does not crash the nested compositor (V-19) (owner: claude)
- [ ] T060 [P] [US3] Write `tests/native/t_drag_scope.sh`: while non-identity, a held-button drag inside the client reports `surfaceLocal` coordinates; Super+LMB move and border resize drags do not change the window's logical geometry (`scripts/nested.sh run hyprctl clients -j` before/after, never the host instance) (FR-012) (owner: claude)

### Implementation for User Story 3

- [ ] T061 [US3] Author `patches/hyprland/0006-*.patch` hit-test: `windowAt` selects the monitor by screen position, then `inverse`; `windowSurfaceAt` and `surfaceLocalAt` via `surfaceLocal` (FR-007) (owner: claude)
- [ ] T062 [US3] Author `patches/hyprland/0007-*.patch` seat-local paths: fallback local, held-button motion, pointer constraints and warps via `forward`; IME, layer and grab surfaces never transformed (research R2 input defects) (owner: claude)
- [ ] T063 [US3] Author `patches/hyprland/0008-*.patch`: decoration/corner hit selection via `inverse`; block and log compositor move/resize drags while non-identity (FR-012, DV3-11) (owner: claude)
- [ ] T064 [US3] Author `patches/hyprland/0009-*.patch` popups: `CPopup::reposition` constraint in world terms; null-monitor guard in `CPopup::onMap` (spec edge case) (owner: claude)
- [ ] T065 [US3] Run `scripts/check-single-transform.sh`, rebuild, run US3 tests plus the full Sprint 1 suite (regression); debug until V-16..V-19 PASS (owner: claude)

**Checkpoint**: one real window correct in pixels and input.

---

## Phase 8: Delivery spike (master plan §A item 4; DV3-01)

- [ ] T066 Reproduce seams 0002–0009 with `createFunctionHook` in a nested-only plugin under `platform/plugin-spike/` built against the stock-from-source prefix; (owner: claude)
- [ ] T067 Run the identical Sprint 1 + US3 suite on stock-from-source + plugin; write `specs/001-native-spatial-camera/delivery-decision.md` comparing results, stability and maintenance cost (V-21) (owner: claude)
- [ ] T068 Clean-room review of the `delivery-decision.md` evidence and a re-run of the plugin suite from fresh build directories (owner: claude)

---

## Phase 9: Feature 001 gate

- [ ] T069 R1 for Phase 2: clean-room rebuild and full-suite re-run (Sprint 1 + US3 + spike) on the final commit, with a source-verification report on 0006–0009 as in T044 (owner: claude)
- [ ] T070 Update `checklists/verification.md` (all V-xx), `integration-log.md`, `.ai/CURRENT_STATE.md`, `.ai/NEXT_TASK.md`; human review V-20 for the Phase 2 image set (owner: claude)
- [ ] T071 **Feature 001 gate**: present L4 + L5 + L5b evidence and the delivery decision; STOP for user approval before merging into `v3/spec-foundation`, Feature 002 or the U0 spike (owner: claude)

---

## Dependencies & Execution Order

### Phase dependencies
- **Phase 1** → **Phase 2** → US1 / US2 / US4 judging. Claude may author patches (T025, T035–T038, T043) as soon as
  T007 and T024 are done; no patch is *judged* before T022 (GATE-A) and, for US2, T034 (GATE-N).
- **US1** first (T026) because T-ID is a stop condition. **US2** and **US4** share the 0001–0005 series and are
  judged after it; US4 tests do not depend on US2 tests.
- **Phase 6** requires Phases 3–5. **Phases 7–9 are BLOCKED** until T053 and user approval.

### Task dependencies (non-obvious)
| Task | Depends on |
|---|---|
| T002, T003 | T001 (done; retired Codex lane) |
| T006 | T005 |
| T008 | T007 |
| T009 | T007, T008, T005 |
| T012 | T010 |
| T013 | T010, T011, T012 |
| T014 | T013, T003 |
| T015 → T016 → T017 | T014 for live runs (T015 authoring can start earlier) |
| T020 (live verification only; authoring is parallel) | T015, T018 |
| T022 GATE-A | T014, T016, T018, T019, T020 |
| T023 | T020 |
| T024 | T010 (for judged builds) |
| T025 | T007, T024 |
| T026 | T022, T023, T025, T013 |
| T027–T032 | T020 (authoring); runs need patches |
| T033 | T013, T027, T029 |
| T034 GATE-N | T033, T019, T022 (stock identity capture = positive control P) |
| T035–T038 | T025 (series order 0001→0005) |
| T039 | T021, T038 |
| T040 | T022, T034, T039, T027–T032 |
| T041–T042 | T020 |
| T043 | T022 (GATE-A), T026 (T-ID image), T038, T041, T042 |
| T044 | T040, T043 |
| T045 (informational) | T040, T043 |
| T053 G1 | T044–T052 |
| T056, T057 | T055 (contract v1.1) |
| T054–T071 | T053 + user approval (no exceptions) |
| T054 | T055 (license recorded before integration), T018 |

### Execution order (single agent, DV3-15)
- T014 → T009 → T015 → T016 → T017 → T018 → T019 → T020 → T021 → T022 (GATE-A) → T023 → T024 → T025 (scripted build
  of 0001) → T026 (T-ID) → T027–T032 → T033 → T034 (GATE-N) → T039 → T040 → T041 → T042 → T043 → T044–T053.
- `[P]` marks tasks that touch different files and could be done in any order; one agent runs them sequentially.
- Heavy jobs (T014, T022, T026, T033, T040, T044, T045, T046) are serialized by the heavy lock.

## Parallel Example: User Story 2 tests

```text
Task: "tests/native/t_zoom.sh"  Task: "tests/native/t_pan.sh"  Task: "tests/native/t_stale.sh"
Task: "tests/native/t_stale_static.sh"  Task: "tests/native/t_layer.sh"
```

## Implementation Strategy

1. **MVP = US1** (T-ID on the patched build): proves the patch infrastructure does not degrade Normal mode.
2. **Sprint 1 = US1 + US2 + US4** behind GATE-A and GATE-N, closed by R1 and G1.
3. **Phase 2 = US3 + delivery spike**, only after explicit approval.
4. Commits go directly to `v3/001-native-spatial-camera` (DV3-13). A task is ticked only after its verification ran
   and its evidence is archived and checksum-verified (`contracts/test-tooling.md` §8).

## Notes
- A stop condition (master plan §I item 9, `.ai/NEXT_TASK.md`) halts the work and is reported to the user.
- Tolerances belong to the spec; nobody loosens them. Test changes after patches exist need a reason in
  `integration-log.md`.
- Missing environment ⇒ BLOCKED or NOT RUN, never PASS.
