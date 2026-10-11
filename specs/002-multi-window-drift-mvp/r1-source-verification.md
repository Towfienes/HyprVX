# R1 source verification (T057), Feature 002

Written by Claude on 2026-10-12 **before** the clean-room rebuild and re-run results were read (DV3-15: author and
verifier are the same agent, so the source review is fixed first). Sources:

- mechanism: `patches/hyprland/0001–0013` (patchset `d291f743b864`) applied to Hyprland `efb5099`; line numbers refer
  to the applied draft tree `~/.cache/hyprvx/claude/src/hyprland-draft` at `bf59c232` (paths below are relative to its
  `src/`);
- policy and form S: this repository at the commit that adds this file (`platform/plugin/src/...`).

## (a) FR-003 / FR-005: nothing added while Drift is OFF

| Path | Source | OFF behaviour |
|---|---|---|
| Pass camera hint | `render/Renderer.cpp:1197-1205` | `pushHyprvxCamera` adds an element only `if (HYPRVXCAMERA)`, i.e. a non-identity camera (`hyprvx/SceneCamera.cpp` `activeCamera`) |
| Per-window offset hint | `render/Renderer.cpp:442-449` | `renderSceneWindow` adds the two hint elements only when the window pass is active **and** `appliedOffset(w) != 0`; otherwise it is the stock `renderWindow` call |
| Window-pass context | `render/Renderer.cpp:1207-1211` | `active` only for the monitor's active workspace without fullscreen; reset by a scope guard |
| `noSimplify` | `render/Renderer.cpp:2203` | set only with a camera or `anyWindowOffset()` |
| DS / solitary blocks | `output/Monitor.cpp:1818, 2039` | only with a non-identity camera or `anyWindowOffset()` |
| Offset helpers | `hyprvx/WindowOffset.cpp:132, 143, 150` | `appliedOffset`, `appliedContentOffset`, `anyWindowOffset` return at once when no offset is stored |
| Policy sync when OFF | `platform/plugin/src/policy/Session.cpp:65-94` | for a monitor not in DRIFT: `resetCamera` only if the camera is not already identity (line 72); every window gets `mechanismOffset = 0`, and `setWindowOffset` with the current value does nothing (`hyprvx/WindowOffset.cpp` `setWindowOffset`) |
| Input listeners | `platform/plugin/src/policy/Input.cpp:69, 80, 106` (key, axis, button press) | return before any test while `anyDrift()` is false, so nothing is cancelled; release (`:98`) and motion (`:129`) act only while a cancelled Drift press is held, which cannot happen OFF |

Native evidence: T-PLUGIN-INERT (load/unload byte-identical with Drift OFF), T-DRIFT-OFF (exits byte-identical to
stock sessions), Feature 001 regression with the plugin loaded and Drift OFF (V-24).

## (b) Constitution IV: one transform

- `scripts/check-single-transform.sh` (self-tested) passes on 0001–0013 and on the policy and form S sources fed as
  all-added diffs (12 files); a planted `x * camera.zoom` line is reported (lint is not vacuous).
- Non-vendored arithmetic left: the policy computes pan/zoom/drags/spawn only through `DriftModel` →
  `cammath::panByScreen`, `anchorZoom`, `worldDelta`, `visibleWorldRect` (`core/src/DriftModel.cpp`); the mechanism uses
  `cammath::offsetCamera` (`hyprvx/SceneCamera.cpp` `windowRenderModif`) and adds `O(w)` to logical boxes
  (`box + O`, a translation in world units, not camera arithmetic); `windowLocalPoint = screenToWorld(S) - O`
  (`hyprvx/InputCamera.cpp`).

## (c) Constitution V: Hyprland's window model is not written

- Policy (`platform/plugin/src/policy/*.cpp`): no assignment to a window's workspace, position, size, floating state
  or layout target, no `movewindow`/`resizewindow` dispatch; the only hyprctl call is `invokeHyprctlCommand("version")`
  (`Compat.cpp:24`, read-only). The policy writes only the mechanism (`HyprVX::setCamera`, `resetCamera`,
  `setWindowOffset`, `resetWindowOffsets`) and, on a workspace switch in Drift, finishes the workspace's render
  animations (`Session.cpp` `finishSwitchAnimation`: `m_renderOffset->warp()`, `m_alpha->warp()`; R7), which are
  render state, not the window model.
- Patches 0010–0013 add no line that assigns `m_workspace`, calls `moveToWorkspace`, `setBox`, `move`/`resize` or sets
  `m_isFloating` (grep of the added lines). The only geometry write is Hyprland's own floating resize drag, which 0012
  feeds with the pointer in the window's L space (`layout/supplementary/DragController.cpp` `updateDragWindow`,
  `mouseMove`; FR-009); a move drag and a tiled resize are blocked and logged (`dragBegin`).

## (d) FR-013: layer, lock, IME and cursor are never offset

- Drawing: offsets are applied only in `renderWorkspaceWindows` (the window pass, `render/Renderer.cpp:442-449`);
  layers, lock surfaces, IME popups and the cursor are rendered outside it, unchanged from Feature 001.
- Content ownership: `hyprvx/WindowOffset.cpp:87` `contentOwner` returns a window only for window surfaces, their
  subsurfaces and window-owned popups; a layer popup (`layerOwner()`) and every non-window view return null, so
  `appliedContentOffset` is 0 for them (damage `render/Renderer.cpp` `damageSurface`, hit-testing 0011).
- Native evidence: T-MULTI TOP layer region byte-identical under camera and offsets (V-04); T-PAN drag over a TOP
  layer is not a pan (V-13).

## (e) FR-016: the dev control is a test-build feature only

- Patched build: `hyprvx/DevControl.cpp:3` `#ifdef HYPRVX_DEV`; without it `registerDevControl` is empty
  (`:208-210`), so the `hyprvx-dev` command and dispatchers are never registered (`Compositor.cpp:724` call). The
  build tool adds `-DHYPRVX_DEV=ON` only for the test variant `patched`; `patched-nodev` builds without it.
- Form S: `platform/plugin/src/mechanism/Install.cpp:50` calls `registerDevControl` only `#ifdef HYPRVX_DEV`, defined
  only by `HYPRVX_PLUGIN_DEV=ON` (`platform/plugin/CMakeLists.txt:46-47`), i.e. the `S-dev` test build;
  `DevControl.cpp` is entirely inside `#ifdef HYPRVX_DEV`.
- Binary check (run after this report, `tests/native/t_fr016.sh`): the literal `hyprvx-dev` absent from plugin S and
  from the patched-nodev binary, present in S-dev and the dev build (controls); `hyprctl hyprvx-dev state` rejected in
  stock + S and patched-nodev + P sessions, which still enter Drift.

## (f) Tests changed after the code existed

Each change is recorded with its reason in `integration-log.md`; none relaxes an acceptance criterion.

| Test | Change | Reason |
|---|---|---|
| `t_input_mw` | setup point moved to A-local (100,100) | A's centre lies under A's popup (the compositor was right) |
| `t_stale_mw` | ready reference is a TOP layer client | an off-screen window gets no frame callbacks (BLOCKED, not FAIL) |
| `x11client` / `t_x11` | menu grabs the pointer while mapped; NestedOnly accepts Hyprland as the X peer | real X11 menus grab; Hyprland itself listens on the X sockets |
| `t_drag_scope` (Feature 001) | floating border resize now expected to follow d/z; press moved below the centre line; d/z on both axes | F002 FR-009 supersedes F001's blocked resize; stock `cornerFromBox` picks the corner by quadrant |
| `t_monitor` | re-added output compared with a stock session doing the same round trip | Hyprland itself moves floating windows (140,100) → (139,99) through FALLBACK (US1.3 allows it) |
| `t_anchor` | `scroll 0 -1` instead of `scroll 0 -120` | vpointer `scroll` takes detents |
| `t_drag_abort` | pointer-driver barrier on workspace 2 is client c2 | hidden clients give no frames |
| `t_ws_camera`, `t_x11` policy case | windows mapped with Drift OFF | spawn placement (US5) centres windows mapped in Drift by design |
| `compare_forms.py` | t_plugin_inert per-plugin images excluded with the reason | image names encode the plugin; each is judged against its own run's reference |

## (g) Form S equivalence

Form S runs the same patched bodies (generated copies, `platform/plugin/gen_copies.py`, 20 member functions + 5
helpers from 0001–0013) and wrappers for the hunks that only add code around a stock body; the offset code is a port
of 0010 (`platform/plugin/src/mechanism/WindowOffset.cpp`). Known gap carried from Feature 001: special-workspace
windows rendered inline in `renderAllClientsForWorkspace` get no camera hint in form S (a wrapper cannot reach them);
special workspaces never become a Drift scene, so no Feature 002 test exercises them. Native evidence: V-23 (532
captures byte-identical between forms).
