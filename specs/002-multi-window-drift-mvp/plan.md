# Implementation Plan: Multi-Window Drift MVP (Option A)

**Branch**: `v3/002-multi-window-drift-mvp` | **Date**: 2026-10-11 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/002-multi-window-drift-mvp/spec.md`
**Master plan**: `docs/v3/HYPRVX_V3_MASTER_PLAN.md` (§C blueprint, §D UX, §H Phase 3). Decisions DV3-01, DV3-11,
DV3-15, DV3-16. Baseline: Feature 001 merged at `6931c04`.

## Summary

Turn Feature 001's camera proof into a usable Drift Mode for the windows of one workspace.

- **Mechanism** (patch series, source of truth): extends the camera seams with a per-window world offset `O(w)`. Each
  window is drawn through its own offset camera (research R1). Culling, damage and every input seam use `L + O`
  (R8). Floating resize runs through the camera (R5).
- **Policy** (plugin `hyprvx-drift`): the OFF/ENTERING/DRIFT/EXITING state machine, a camera per workspace, the offset
  store, Drift controls (dispatchers, Super + wheel, Super + drag, Super + Escape), window move by offset, spawn
  placement and X11 menu inheritance.
- **Delivery**: both forms, the patched build plus the plugin (form P) and stock Hyprland plus the plugin with
  mechanism hooks (form S, DV3-16), pass the same suite.
- **Gate**: the user's 12-step scenario, with keyboard/pointer camera controls in place of trackpad.

## Technical Context

- **Language/Version**:
  - C++23 for the Hyprland patches and the plugin;
  - C++20 for `hyprvx_core` (`CameraMath` 1.2, new `DriftModel`);
  - C (xcb) and C++ for the test tools;
  - Bash for scripts and Python 3 `-I` for analysis.
- **Primary Dependencies**:
  - Hyprland `efb50993…` with the host libs of Feature 001 (aquamarine 0.15.1, hyprutils 0.14.2, hyprlang 0.6.8,
    hyprgraphics 0.5.1, hyprcursor 0.1.13);
  - hyprwayland-scanner v0.4.6 and glaze b518eec (pinned);
  - wayland-client 1.26, xkbcommon 1.13.2, xcb 1.17 (test tools only);
  - GTest.
- **Storage**: N/A. Offsets and cameras live for the compositor session (persistence is later work).
- **Testing**:
  - GTest (L2: transform 1.2 invariants, `DriftModel`);
  - nested harness with captures and the analyzer (L4/L5);
  - vpointer, vkeyboard, test-client logs and the X11 client (L5b);
  - socket2 event logs and `hyprctl clients` (ownership).
- **Target Platform**: CachyOS, AMD iGPU (host GPU profile Integrated), nested Hyprland on a headless output.
- **Project Type**: compositor patch series + Hyprland plugin (two forms) + C++ libraries + dev tools.
- **Performance Goals**:
  - Drift OFF adds no pass elements and intercepts no input (Constitution III); listeners return at once.
  - In Drift, one hint pair per drawn window. Cost is measured, not optimised (Phase 7).
  - No per-frame IPC or polling (Constitution VI).
- **Constraints**:
  - Builds run under the heavy lock with `-j4`, `MemoryMax=6G` and MemAvailable ≥ 6.5 GiB; the threshold is never
    lowered.
  - No host interaction; no uinput.
  - The patch series stays the source of truth (DV3-16).
- **Scale/Scope**: 1–4 windows per scene, 2 workspaces, 1 output (scale 1 and 1.25), both delivery forms.

## Constitution Check

Design-time assessment; runtime evidence goes to `checklists/verification.md`.

| Principle | Status | Note |
|---|---|---|
| I Host safety | PASS | Nested only. vkeyboard and vpointer refuse non-nested displays. `xdisplay` resolves XWayland only through the verified compositor PID. The plugin is loaded only into nested sessions and never into the host |
| II Evidence | PASS | Every SC has a capture, client log, socket2 log or `hyprctl clients` artifact. SC-008 is human-signed. Dispatcher replies and `hyprvx state` are never evidence |
| III Normal equivalence | PASS | Leaving Drift sets identity camera and zero mechanism offsets, so the stock path adds no elements. T-DRIFT-OFF, T-PANIC-DRIFT, and Feature 001 regression with the plugin loaded |
| IV Single transform | PASS | `offsetCamera` and `worldDelta` are added to `CameraMath.hpp` (contract 1.2); the lint covers the new patches; the policy uses only `CameraMath` |
| V Reversibility | PASS | `O(w)` is HyprVX-owned; `L`, workspace, tiling and layout are never written (T-DRIFT-OFF, T-MOVE: no `movewindow*`). Floating resize is the intended exception (§C5) |
| VI Performance | PASS (measurement deferred) | Event-driven listeners; one hint pair per window in Drift (Complexity Tracking) |
| VII Caelestia | PASS (layers) / NOT RUN (Caelestia) | Layer, lock, IME and cursor are never offset or transformed; no synthetic events |
| VIII Reuse | PASS | `CameraMath`, `PluginCompatibility`, the Feature 001 harness and tools, and the spike's hook registry and generator are reused. `DriftModel` is new (R12) |
| IX License | PASS | Virtual-keyboard XML (MIT, from Hyprland `protocols/`) is recorded in `docs/THIRD_PARTY.md`. Copied Hyprland functions in form S stay BSD-3 (recorded in Feature 001). No DriftWM code |
| X Reproducible | PASS | Pinned upstream plus the ordered series 0001–0013; plugins built in fresh directories with `build-info.json` (compiler, headers prefix id, source hash) |

### Re-check of the delivered code (T060, 2026-10-12, final commit `420efff`)

| Principle | Status | Evidence on the delivered code |
|---|---|---|
| I Host safety | PASS | V-26 audit `20261012-032253-audit-7f12c15-v26`: 1114 nested sessions, none with the host signature, private runtime dirs, agent-prefix binaries only, host `hypr`/`caelestia` unchanged; V-03 refusals (`20261012-032136-static-420efff-f002_static`). Known limit: x11client completes the X handshake before its SO_PEERCRED check (xcb), after the environment and DISPLAY checks; it draws only |
| II Evidence | PASS | Every PASS in `checklists/verification.md` cites an archive; 22 cited archives and 366 per-test runs pass `sha256sum -c`; SC-008 (V-25) awaits the user's review and signature |
| III Normal equivalence | PASS | T-DRIFT-OFF, T-PANIC-DRIFT, T-PLUGIN-INERT and V-24 on the final commit in both forms; source review `r1-source-verification.md` (a) |
| IV Single transform | PASS | V-28 lint on 0001–0013 and the 19 policy/form S sources (not vacuous); the core `current.zoom * factor` lint hit is a zoom target passed to `anchorZoom`, not an equation (R1 finding 3) |
| V Reversibility | PASS | No write of workspace, `L`, tiling or layout outside Hyprland's floating resize (R1 (c)); T-MOVE: no `movewindow*`, `L` unchanged |
| VI Performance | PASS (measurement deferred) | Listeners return at once with Drift OFF; no polling or per-frame IPC. Not measured in this feature: the damage union adds each damaged box at every offset window's offset (conservative, cost grows with the number of offset windows) |
| VII Caelestia | PASS (layers) / NOT RUN (Caelestia) | Layer, lock, IME and cursor never offset (R1 (d), T-MULTI layer region identical); Caelestia itself not run in nested |
| VIII Reuse | PASS | As planned; form S reuses the patched bodies through `gen_copies.py` |
| IX License | PASS | `docs/THIRD_PARTY.md` records the virtual-keyboard XML and the 20 member functions + 5 helpers copied into form S |
| X Reproducible | PASS | V-27: two clean-room builds of the final code; plugins bit-identical, Hyprland differs only by the `git am` commit (same tree), captures 556/556 identical per form |

## Project Structure

### Documentation (this feature)

```text
specs/002-multi-window-drift-mvp/
├── spec.md  plan.md  research.md  data-model.md  quickstart.md
├── contracts/drift-control.md  contracts/mechanism-api.md
├── contracts/transform-contract.md (1.2 delta)  contracts/test-tooling.md (1.4 delta)
├── checklists/requirements.md  checklists/verification.md (created with tasks)
├── integration-log.md           # append-only run/decision log
└── tasks.md                     # /speckit-tasks; implementation after the user approves it
```

### Source Code (repository root)

```text
core/include/hyprvx/core/CameraMath.hpp      # 1.2: offsetCamera, worldDelta (+ tests/unit/test_camera_math.cpp)
core/include/hyprvx/core/DriftModel.hpp      # NEW pure policy model (+ core/src/DriftModel.cpp, tests/unit/test_drift_model.cpp)
patches/hyprland/0010-*.patch … 0013-*.patch # NEW: offsets in render/cull/damage; offsets in input; floating resize; dev control 1.2
platform/plugin/                             # NEW policy plugin hyprvx-drift
├── src/policy/                              #   adapter: dispatchers, bus listeners, workspace cameras, drags, spawn, validation
└── src/mechanism/                           #   form S only: hook registry, wrappers and generated copies (from platform/plugin-spike)
scripts/build-plugin.sh                      # NEW: --form P|S, fresh build dir, build-info.json
tools/vpointer/ (scroll)  tools/testclient/ (keyboard, axis)  tools/vkeyboard/ (NEW)  tools/x11client/ (NEW)
tools/hyprvx_nested.py  scripts/nested.sh    # plugin load, xdisplay, run --x11
tests/native/                                # §9 tests (t_drift_off.sh … t_scenario.sh), run_feature002.sh
```

**Structure decision**: Extend the existing tree (Constitution VIII).

- The spike's mechanism sources move from `platform/plugin-spike/` to `platform/plugin/src/mechanism/` with `git mv`,
  so there is one copy. Feature 001 documents cite the spike paths at their own commits, which stay valid.
- `scripts/build-plugin-spike.sh` is replaced by `scripts/build-plugin.sh --form S`.
- `CameraMath.hpp` in the patch series is updated by patch 0010 to stay byte-identical to the core header; patch 0001
  is not rewritten, so Feature 001 evidence keeps referring to an unchanged series prefix.

## Phases (within this feature)

1. **Foundations** (no compositor):
   - `CameraMath` 1.2 and its invariants;
   - `DriftModel` with its unit tests (state machine, idempotence, per-workspace cameras, offsets, drags, spawn
     placement);
   - test tools (vpointer scroll, vkeyboard, test-client keyboard and axis, X11 client), harness `plugin load` /
     `xdisplay` / `run --x11`, and the config rule for the X11 client;
   - tool tests and the identity-refusal tests for the new tools.
2. **Mechanism** (patches 0010–0013, source of truth):
   - offsets in render, culling and damage (0010);
   - offsets in hit-test, surface-local, fallback, warps and constraints, decoration hits and popups (0011);
   - floating resize through the camera, tiled resize blocked (0012);
   - dev control 1.2 (0013).
   - Verified with the dev control alone (no policy): T-MULTI, T-INPUT-MW, T-STALE-MW, T-X11 with dev offsets.
     Then lint and Feature 001 regression.
3. **Policy, form P**: the plugin on the patched build: state machine and validation, dispatchers, built-in inputs,
   per-workspace cameras with animation suppression, move drag, spawn, X11 menu inheritance.
   - Tests: T-DRIFT-OFF, T-PANIC-DRIFT, T-IDEMP, T-ANCHOR, T-PAN, T-MOVE, T-RESIZE, T-DRAG-ABORT, T-KEYS, T-SCROLL,
     T-WS-CAMERA, T-WS-ANIM, T-SPAWN, T-FULLSCREEN, T-MONITOR.
4. **Form S** (DV3-16 delivery):
   - regenerate the copied functions from the patched tree (0001–0013);
   - implement the mechanism API with offsets in the plugin;
   - run the full suite in form S, with byte-identical captures against form P.
5. **Gate**:
   - T-SCENARIO in both forms;
   - Feature 001 regression in both forms with the plugin loaded;
   - clean-room rebuild and re-run (R1);
   - verification checklist and the human review (SC-008);
   - merge-readiness review;
   - the Feature 002 gate, which stops for the user's approval (merge after the gate, master plan §L).

## Risks

| Risk | Mitigation |
|---|---|
| A seam that uses window positions outside the R8 list | Stop condition: record file:line before adding; T-INPUT-MW / T-MULTI / T-STALE-MW cover popups, subsurfaces, decorations |
| Form S hook limits (file-static `updateRelativeCursorCoords`, inlined special-workspace hint; private access; ABI) | Already open under DV3-16. New seams are checked for a dynamic symbol before they are designed; an unreachable seam is recorded and both forms are compared byte for byte |
| XWayland lazy start, or DISPLAY not resolvable safely | `xdisplay` refuses unless the lock-file PID belongs to the verified compositor; a refusal is BLOCKED (never a host display) |
| Virtual keyboard focus or modifier quirks | Modifier sharing was verified in source (R11); a test that cannot press Super is BLOCKED, never PASS |
| Same agent writes and verifies (DV3-15) | Tests come from the spec and contracts before the code is judged; mechanism verified by dev control before the policy exists; R1 clean-room re-run; test changes after the code exists need a reason in `integration-log.md` |
| RAM pressure during builds | Heavy lock, `MemoryMax=6G`, one heavy job at a time, waiting for MemAvailable (never lowering the threshold) |

## Complexity Tracking

| Exception | Why needed | Simpler alternative rejected because |
|---|---|---|
| One hint pair per drawn window while Drift is on | Draws decorations, popups and subsurfaces of an offset window together under one transform (R1) | Adding `O` to `renderdata.pos` needs separate seams for every decoration, popup and subsurface position, plus their damage and hit tests |
| Two plugin forms from one source | DV3-16 delivers form S; the patch series must stay the source of truth and be debuggable (R3) | Form S alone hides mechanism bugs until runtime; form P alone is not the chosen delivery |
| Policy cancels Super + left button, Super + wheel and Super + Escape before keybinds in Drift | Keeps Hyprland's own move bind from writing `L(w)` and makes Drift controls work without config (R4) | User-config binds cannot stop `bindm movewindow` from writing logical geometry |
