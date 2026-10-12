# Implementation Plan: DriftWM Window Behavior

**Branch**: `v3/004-drift-window-behavior` | **Date**: 2026-10-12 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/004-drift-window-behavior/spec.md`
**Master plan**: `docs/v3/HYPRVX_V3_MASTER_PLAN.md` (§C5 ownership, §D UX rows Move/Snap/Pin/Navigation, §H Phase 5).
Decisions DV3-01, DV3-15, DV3-16, DV3-18. Baseline: Feature 003 merged at `534388b`.

## Summary

Add canvas window-manager behavior to Drift Mode:

- **Snap** while moving a window (Super + drag): per-axis attach at 24 screen px, gap 12 world px, release 32 world px
  (hysteresis, no oscillation), with edge alignment; a pure core rule (`WindowBehavior::snapMove`, research R2).
- **Groups**: windows snapped together form an implicit group (derived, R3); Shift + Super + drag moves the group as
  one on the Feature 002 drag controller extended to a list of members (R4).
- **Navigation** (instant camera jumps, R6): Super + arrow focuses the neighbour (V1 `SpatialNavigator`) and reveals
  it; center, frame window/group (V1 `frameRectInViewport`), home, bookmarks 1–4, history back/forward (V1
  `CameraHistory`, R1). Keys on combinations the user's config does not bind, plus dispatchers (R7).
- **Pin**: a window drawn at a fixed screen position and size above the canvas windows; needs mechanism patch 0014 and
  the matching form S copies (R5).
- **Gate** (master plan): L2 + L5 tests for snap and groups; navigation and pin also tested at L5; Feature 001–003
  regressions in both forms; P/S parity.

## Technical Context

- **Language/Version**: C++23 (plugins, patch), C++20 (`hyprvx_core` `WindowBehavior`), Bash, Python 3 `-I`.
- **Primary Dependencies**: Hyprland `efb50993…` + patch series 0001–0014 (form P) or stock + plugin mechanism (form
  S); host libs as Feature 003; V1 core (`NavigationState`, `SpatialNavigation`, `ViewportFraming`, `World`); GTest.
- **Storage**: N/A (session-only state, config values).
- **Testing**: GTest (L2 `WindowBehavior`, extended `DriftModel` group drag); nested captures + analyzer (L4/L5),
  vpointer drags, vkeyboard combinations, `hyprctl clients` snapshots and socket2 `movewindow` counts (L5b).
- **Target Platform**: CachyOS, AMD iGPU, nested Hyprland on a headless output.
- **Project Type**: patch series + policy plugin (two forms) + core library + test tools.
- **Performance Goals**: Drift off: nothing added (FR-001). Snap runs once per drag motion over ≤ tens of windows
  (O(n)); navigation on key press only; no frame loop. Measured later (Phase 7).
- **Constraints**: heavy lock, `-j4`, MemAvailable ≥ 6.5 GiB; nested only; host config read-only; a new patched
  prefix id after 0014 (GATE-A and Feature 001–003 regressions re-run on it).
- **Scale/Scope**: 3–4 windows, 2 workspaces, 1 output (scale 1 and 1.25), both forms.

## Constitution Check

| Principle | Status | Note |
|---|---|---|
| I Host safety | PASS | Nested only; keys from `hyprvx-vkeyboard` (identity-verified); host config read only (R7 checks were reads) |
| II Evidence | PASS | Every SC has captures, `hyprctl clients` snapshots, socket2 counts or frame-by-frame offset logs; L2 unit evidence archived |
| III Normal equivalence | PASS | Drift off: no key consumed, pins and snaps inactive (T-WB-OFF vs stock) |
| IV Single transform | PASS | Camera results only via `CameraMath` (frame/center/reveal adapters); the mechanism pin uses an identity camera with a translation through the existing modif helpers; lint extended to 0014 and `WindowBehavior` |
| V Reversibility | PASS | Snap, groups, navigation and pin write offsets, cameras, pins, bookmarks and history only; Hyprland's model unchanged (SC-006); Drift off = stock |
| VI Performance | PASS (measurement deferred) | No polling, no frame loop; O(n) per drag motion |
| VII Caelestia | NOT RUN (Caelestia) | No Caelestia surface involved; keys avoid every combination the user binds (R7) |
| VIII Reuse | PASS | V1 `CameraHistory`, `frameRectInViewport`, `SpatialNavigator` reused; `IslandInteractionController` rejected with source evidence (R1) |
| IX License | PASS | No third-party code copied (phantomat reference only, DV3-18); patch 0014 is new HyprVX code on BSD-3 Hyprland, recorded like 0010–0013 |
| X Reproducible | PASS | Patch series regenerated deterministically; form S copies generated from the series; clean builds recorded |

## Project Structure

### Documentation (this feature)

```text
specs/004-drift-window-behavior/
├── spec.md  plan.md  research.md  data-model.md  quickstart.md
├── contracts/window-behavior.md (1.0)  drift-control.md (1.2 delta)  mechanism-api.md (1.1 delta)  test-tooling.md (1.6 delta)
├── checklists/requirements.md  checklists/verification.md (with tasks)
├── integration-log.md
└── tasks.md                     # /speckit-tasks; implementation after the user approves it
```

### Source Code (repository root)

```text
core/include/hyprvx/core/WindowBehavior.hpp     # NEW pure rules (+ core/src/WindowBehavior.cpp, tests/unit/test_window_behavior.cpp)
core/include/hyprvx/core/DriftModel.hpp          # group drag (members list), snap state in the drag
patches/hyprland/0014-hyprvx-screen-pinned-windows.patch   # NEW mechanism: pin draw order, identity translation, hit-test, damage
platform/plugin/src/mechanism/                   # form S: pin functions + regenerated copied bodies
platform/plugin/src/policy/Behavior.cpp          # NEW: keys, dispatchers, navigation, bookmarks, history, pin, settings
platform/plugin/src/policy/Input.cpp             # Shift + Super group drag; snap on drag motion
platform/plugin/src/policy/Session.cpp           # state JSON (pins, history, bookmarks); cleanup on off/panic/unload
tools/vkeyboard/main.cpp                         # new keys
tests/native/t_snap.sh t_snap_osc.sh t_group.sh t_nav.sh t_pin.sh t_wb_off.sh run_feature004.sh
```

**Structure decision**: extend the Feature 002/003 tree (Constitution VIII); one new mechanism patch, for pin only.

## Phases (within this feature)

1. **Foundations** (no compositor): `WindowBehavior` and `DriftModel` group drag with unit tests written first from
   the contract; vkeyboard keys and refusal tests; `lib.sh` helpers.
2. **Snap and groups (US1, US2)**: policy integration on the drag; T-SNAP, T-SNAP-OSC, T-GROUP.
3. **Navigation (US3)**: keys and dispatchers, bookmarks, history; T-NAV, T-WB-OFF.
4. **Pin (US4)**: patch 0014 (draft tree, lint, export), new patched prefix, GATE-A re-run, form S mechanism + copies;
   policy pin/unpin; T-PIN.
5. **Form S and parity**: every Feature 004 test in form S; `compare_forms.py`.
6. **Gate**: Feature 004/003/002/001 suites in both forms; clean-room rebuild (R1); host-safety audit; verification
   checklist; merge-readiness review; STOP at the Feature 004 gate.

## Risks

| Risk | Mitigation |
|---|---|
| Snap oscillation (master plan risk) | Per-axis hysteresis with release > threshold; L2 slow-path tests and the frame-by-frame L5 test (SC-002) |
| Patch 0014 touches the render loop and hit-testing | Narrow seams next to 0010/0011; single-transform lint; T-PIN input checks; GATE-A and full regressions on the new prefix |
| Form S must copy more Hyprland functions | Copies regenerated from the series by the existing generator; parity test; THIRD_PARTY record updated |
| Super + arrow overrides the user's focus key in Drift | The master plan assigns it with the same meaning (focus in a direction); Drift off passes through (T-WB-OFF) |
| Keys of the interactive nested window are taken by the host | Tests use the harness keyboard only (Feature 003 T045 finding) |
