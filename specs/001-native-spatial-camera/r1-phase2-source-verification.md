# R1 source verification for Phase 2 (T069), patches 0006–0009

Written by Claude on 2026-10-11 **before** the Phase 2 clean-room re-run results were read (DV3-15: author and
verifier are the same agent, so the source review is fixed first, as in `r1-source-verification.md` for T044).
Source: `patches/hyprland/0001–0009` at repo commit `1bf59b0` (patchset `efaf0ea2cf21`), applied with
`scripts/apply-patches.sh --reset` to Hyprland `efb5099`; line numbers refer to that applied tree
(`~/.cache/hyprvx/claude/src/hyprland-efb5099/v3-dev`, HEAD `7d883d1d`).

## (a) FR-002: identity keeps the stock input path

Every input seam goes through one of three helpers in `src/hyprvx/InputCamera.cpp`; each returns its input unchanged
unless `activeCamera()` has a value (`:18-25`, `:27-34`; `screenToWorld(screen)` also when no monitor is under the
point, `:13-16`). `activeCamera()` is the Sprint 1 gate (exact `cammath::isIdentity`, no fullscreen, an active
workspace).

| Seam (FR / research R2) | Source | Identity behaviour |
|---|---|---|
| `windowAt`: monitor by screen position, then world test (FR-007) | `src/desktop/state/ViewHitTester.cpp:36` | `pos == screenPos` |
| `windowSurfaceAt`, `surfaceLocalAt` | `ViewHitTester.cpp:265, 291` | unchanged point |
| Pointer focus on refocus-less motion | `src/managers/input/InputManager.cpp:231` (`sendMotionEventsToFocused`) | `isWindowContent ? screenToWorld : POINTER`, both equal |
| Seat-local coordinates, constraints, warps | `InputManager.cpp:287, 306, 315, 329, 641` (`mouseMoveUnified`) | world = screen; warps target the stock point |
| Resize-cursor icon on borders | `InputManager.cpp:678` | stock condition, plus "no active camera" |
| Button press: decoration and border hit selection | `InputManager.cpp:889` (`processMouseDownNormal`), `src/desktop/view/Window.cpp:455` (`checkInputOnDecos`) | unchanged point |
| Scroll and cursor warp after scroll | `InputManager.cpp:1030, 1041` (`onMouseWheel`) | unchanged point |
| Warp to a window | `Window.cpp:1628` (`warpCursor`), `src/config/shared/actions/ConfigActions.cpp:61` | `worldToScreen` / `screenToWorld` are identities |
| Compositor move/resize drags (FR-012, DV3-11) | `src/layout/supplementary/DragController.cpp:135` | blocked and logged only `if activeCamera()` |
| Popup constraint box (`CPopup::reposition`) | `src/desktop/view/Popup.cpp:383-391` | world box only `if activeCamera()` on the owner's monitor |
| Popup map without a monitor (`CPopup::onMap`) | `Popup.cpp:209-213, 229` | falls back to the owner's monitor; `enter` only with a monitor. Unreachable while a monitor is enabled (`MonitorQueryCore::closestTo`), see V-19 |

Native evidence for the same claim: the identity control state of `t_pointer`, `t_popup` and `t_drag_scope`, and T-ID
in the Sprint 1 regression.

## (b) Constitution IV: one transform

- `scripts/check-single-transform.sh` (self-tested) reports no camera arithmetic outside the vendored
  `src/hyprvx/CameraMath.hpp` for all nine patches.
- Manual review: `InputCamera.cpp` uses `cammath::inverse` and `cammath::forward` only; the popup constraint uses
  `cammath::inverseBox`. The remaining arithmetic is the stock subtraction of a surface or window origin from a
  world point.

## (c) Screen-space surfaces stay outside the camera (FR-004)

`isWindowContent()` (`InputCamera.cpp:36-52`) is true only for window views, window popups and subsurfaces outside a
layer surface. Layer surfaces, layer popups, lock surfaces and surfaces without a desktop view (IME popups) keep
screen coordinates at `InputManager.cpp:231, 306, 329, 641`. Known gap, as written in `InputCamera.hpp`: a
subsurface of a layer popup is treated as window content. No native test exercises layer, lock or IME input under a
camera (source evidence only).

## Tests changed after the input patches existed (from `integration-log.md`)

| Test | Change | Reason |
|---|---|---|
| `tests/native/config/hyprland.conf.in` | `debug:disable_logs = false` | the 0008 blocked-drag WARN lines were never written to the nested log (a runtime `keyword` has no effect; the setting applies on config reload only) |
| `tests/native/config/hyprland.conf.in` | `cursor:invisible = true` | the cursor sprite raced the `t_popup` identity capture after `hyprvx-vpointer` appeared, in either build |
| `tools/hyprvx_nested.py` | HVX-1 created after exec-once again; log copied after the compositor exits | the earlier order put the cursor into GATE-A frames; the log tail could still be buffered |
| `tests/native/t_drag_scope.sh` | identity control runs only after a PASS | a real FAIL (moved window) was reported as BLOCKED |
| `tests/native/lib.sh` | `HVX_PATCHED_PREFIX` (negative controls), `HVX_SPIKE_PLUGIN` (T067) | new run modes; the default path is unchanged |

No tolerance or threshold was changed. GATE-A was re-run after each test-config change and stayed byte-identical
to the Sprint 1 baseline (stock1.png `6d7f6bd6…`).

## Clean-room re-run (read after the sections above were committed)
