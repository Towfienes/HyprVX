# Implementation Plan: Trackpad-Native Drift UX

**Branch**: `v3/003-trackpad-native-ux` | **Date**: 2026-10-12 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/003-trackpad-native-ux/spec.md`
**Master plan**: `docs/v3/HYPRVX_V3_MASTER_PLAN.md` (§D UX, §F L6, §H Phase 4, U4). Decisions DV3-01, DV3-15, DV3-16.
Baseline: Feature 002 merged at `dd4f9b9`.

## Summary

Make the trackpad the primary navigation of Drift Mode, entirely in the policy plugin:

- **Pan**: finger-source scroll sequences on empty canvas (or with Super anywhere) and 3-finger swipes pan the scene
  camera; scrolls over windows stay with the windows (research R3, R4).
- **Zoom**: pinches on canvas (or with Super, or with 3 fingers) zoom absolutely from the camera at begin, anchored at
  the begin pointer; cancel restores the start camera (R4).
- **Arbitration**: on a Drift monitor every 3-finger swipe and pinch is consumed before Hyprland's `gesture =`
  bindings and before client delivery; 4-finger gestures stay stock (R1, R4).
- **Glide**: a released pan continues with exponential decay, integrated by V1 `CameraController` (closed form), driven
  by `render.preChecks` only while moving, interrupted by any input or mode change (R5, R6).
- **Delivery**: no mechanism or patch change; forms P and S share the policy and must give byte-identical captures
  (R10). Test-only additions: vpointer `fscroll`, test-client gesture logging, and the `hyprvx-testinput` plugin
  (`hvxtest`) for synthetic gestures and frame counts (R9).
- **Gate**: L6 in a nested session for two-finger pan and glide, signed by the user; pinch, 3-finger, 4-finger-in-Drift
  and U4 hardware items BLOCKED (environment) with automated evidence (spec clarification, R2, R11).

## Technical Context

- **Language/Version**: C++23 (plugins), C++20 (`hyprvx_core` `GestureModel`), C++ (test tools), Bash, Python 3 `-I`.
- **Primary Dependencies**: Hyprland `efb50993…` with the Feature 001/002 host libs (aquamarine 0.15.1, hyprutils
  0.14.2, hyprlang 0.6.8); system `wayland-protocols` (pointer-gestures XML, generated, not vendored); GTest.
- **Storage**: N/A (session-only state, config values).
- **Testing**: GTest (L2: `GestureModel`); nested harness captures + analyzer (L4/L5); vpointer `fscroll`, test-client
  logs, `hvxtest` synthetic gestures and frame counts, socket2 events, `hyprctl clients` (L5b); L6 human checklist.
- **Target Platform**: CachyOS, AMD iGPU (GPU profile Integrated), nested Hyprland on a headless output; L6 on a
  visible nested window.
- **Project Type**: Hyprland policy plugin (two forms) + core library + test tools.
- **Performance Goals**: Drift off: listeners return at once and consume nothing (Constitution III). In Drift: no
  allocation per event (fixed-capacity velocity ring), frames only while a glide moves (VI). Cost measured, not
  budgeted (Phase 7).
- **Constraints**: heavy lock, `-j4`, `MemoryMax=6G`, MemAvailable ≥ 6.5 GiB; no host interaction, no uinput; host
  config read-only; the patch series and form S mechanism stay unchanged (patchset `d291f743b864`).
- **Scale/Scope**: 1–3 windows, 2 workspaces, 1 output (scale 1 and 1.25), both forms.

## Constitution Check

| Principle | Status | Note |
|---|---|---|
| I Host safety | PASS | Nested only. Test input through `zwlr_virtual_pointer_v1` on the verified display and the test plugin loaded by the harness into the verified session. The host gesture bindings are only read. L6 forbids 3/4-finger gestures over the nested window (host would act) |
| II Evidence | PASS | Every SC has a capture, client log, socket2/`hyprctl clients` artifact or frame count; L6 is human-signed with BLOCKED items marked, none counted as PASS |
| III Normal equivalence | PASS | Drift off consumes nothing (T-TP-OFF: captures and client logs equal to stock); the config gesture bindings are identical for every variant |
| IV Single transform | PASS | Camera changes only via `panByScreen`, `anchorZoom` (inside `pinchCamera`) and `DriftModel`; no new equation; lint covers the new policy files |
| V Reversibility | PASS | Trackpad input changes cameras only; offsets, `L(w)`, workspace and layout untouched; 4-finger workspace swipe is Hyprland's own action |
| VI Performance | PASS (measurement deferred) | Event-driven listeners; glide advances on `render.preChecks` only while moving and stops producing frames at rest (T-GLIDE frame count) |
| VII Caelestia | PASS (layers) / NOT RUN (Caelestia) | Sequences over layer surfaces pass through; no synthetic Hyprland events (the test plugin is test-only) |
| VIII Reuse | PASS | `CameraMath`, `DriftModel`, V1 `CameraController`/`Camera` for glide; V1 `GestureNavigationAdapter` rejected with source evidence (R12) |
| IX License | PASS | No third-party code copied; pointer-gestures XML is generated from the system package at build time (as `CMakeLists.txt:194` already does) |
| X Reproducible | PASS | Patch series unchanged; plugins and the test plugin built in fresh directories with `build-info.json` |

## Project Structure

### Documentation (this feature)

```text
specs/003-trackpad-native-ux/
├── spec.md  plan.md  research.md  data-model.md  quickstart.md
├── contracts/gesture-model.md (1.0)  contracts/drift-control.md (1.1 delta)  contracts/test-tooling.md (1.5 delta)
├── checklists/requirements.md  checklists/verification.md  checklists/l6-physical.md (with tasks)
├── integration-log.md
└── tasks.md                     # /speckit-tasks; implementation after the user approves it
```

### Source Code (repository root)

```text
core/include/hyprvx/core/GestureModel.hpp   # NEW pure model (+ core/src/GestureModel.cpp, tests/unit/test_gesture_model.cpp)
platform/plugin/src/policy/Gesture.cpp      # NEW: scroll/gesture listeners, sessions, glide on render.preChecks
platform/plugin/src/policy/Input.cpp        # glide interruption on button/key/axis; drag ends an open session
platform/plugin/src/policy/Session.cpp      # glide stop on off/panic/workspace/monitor; "glide" in state; config values
platform/plugin/src/policy/Plugin.hpp       # declarations
tools/testinput/                            # NEW test-only plugin hyprvx-testinput (hvxtest gesture, frames)
tools/vpointer/main.cpp                     # fscroll
tools/testclient/main.cpp                   # pointer-gestures and axis source/stop/value120 logging
scripts/build-plugin.sh                     # --testinput
tools/hyprvx_nested.py  scripts/nested.sh   # start --interactive (visible window for L6)
tests/native/config/hyprland.conf.in        # gesture bindings (§5)
tests/native/t_tp_*.sh  run_feature003.sh   # §9 tests
```

**Structure decision**: extend the Feature 002 tree (Constitution VIII); no new mechanism code and no patch.

## Phases (within this feature)

1. **Foundations** (no compositor): `GestureModel` with unit tests written first from the contract; vpointer
   `fscroll`, test-client logging, `hyprvx-testinput`, `build-plugin.sh --testinput`, harness `--interactive`;
   identity-refusal and tool tests; config gesture bindings and a GATE-A re-run.
2. **Pan and arbitration (US1, US3)**: policy sessions, finger scroll pan, 3-finger consumption, 4-finger pass-through,
   cancel; tests T-TP-PAN, T-TP-WINDOW, T-TP-SUPER, T-TP-LOCK, T-ARBITRATE, T-TP-SURFACES, T-TP-OFF.
3. **Pinch (US2)**: zoom sessions; T-PINCH, T-CANCEL.
4. **Glide (US4)**: release velocity, glide on `render.preChecks`, interruption; T-GLIDE, T-GLIDE-STOP, T-TP-MODES, T-TP-DRAG.
5. **Form S and parity**: the same suite in form S; P/S byte comparison; T-FR020.
6. **Gate**: full suites (Feature 003, 002, 001) in both forms; clean-room rebuild (R1); host-safety audit; L6
   session with the user; verification checklist; merge-readiness review; STOP at the Feature 003 gate.

## Delegation trial (ended)

The Antigravity trial (DV3-17) for `GestureModel` (WO-01) was ended by the user on 2026-10-12 before it started.
Claude Code implements every task (DV3-15).

## Risks

| Risk | Mitigation |
|---|---|
| Physical pinch/3-finger behavior differs from synthetic gestures (device timing, libinput thresholds, U4) | Synthetic gestures take the compositor's real path (R9); L6 items BLOCKED with evidence and carried to an approved hardware session; nothing counted as PASS |
| Host gestures during L6 act on the host (close, suspend) | Safety lines in the checklist; only two-finger input is requested; the harness stops the session |
| Finger scroll timing in vpointer differs from a real device (no kinetic events, stop timing) | Velocity uses only event timestamps; L6 checks the feel by hand; client-side kinetic scrolling is out of scope |
| Glide frames continue when idle | Frame counter in the test plugin; T-GLIDE requires the count to stop |
| Prefix matching of hyprctl commands | The test command word `hvxtest` does not start with `hyprvx` (Feature 002 `2deb797`) |
| Same agent writes and verifies, plus a delegated agent | Tests from the contract before the code; R1 clean-room re-run; every delegated commit re-built and re-run by Claude |

## Complexity Tracking

| Exception | Why needed | Simpler alternative rejected because |
|---|---|---|
| A test-only plugin (`hyprvx-testinput`) | Synthetic gestures must take the compositor's own path in both forms, and frames must be counted without polling | Patch 0014 + a form S copy = two implementations and a patch series change for a test-only function |
| Consumed sequences stay consumed after Drift ends mid-sequence | Hyprland and clients must never see an `update`/`end` without its `begin` | Releasing mid-sequence corrupts `CTrackpadGestures` and client gesture state |
