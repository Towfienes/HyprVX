# Research: Native Spatial Camera (Feature 001)

**Date**: 2026-10-10 · **Method**: read-only source inspection. No builds, no compositor contact, no test runs.
"Confirmed (source)" means proven by reading code. "Hypothesis" means the visible effect is inferred and still
needs a visual test. Upstream references are to Hyprland `efb50993780079460b0cbed1363e2166a2de1d9f` (v0.56.2),
read with `git -C HyprVX-v2/hyprland show efb5099:<path>`.

---

## R0. Host environment (verified 2026-10-10, read-only)

| Item | Value |
|---|---|
| Hyprland | pacman `0.56.2-4.1`; `/usr/include/hyprland/src/version.h` `GIT_COMMIT_HASH efb50993…`; `/usr/share/pkgconfig/hyprland.pc` 0.56.2 |
| ABI string | `efb50993780079460b0cbed1363e2166a2de1d9f_aq_0.15_hu_0.14_hg_0.5_hc_0.1_hlg_0.6` |
| Libraries | aquamarine 0.15.1, hyprutils 0.14.2, hyprgraphics 0.5.1, hyprcursor 0.1.13, hyprlang 0.6.8, wayland 1.26.0, wayland-protocols 1.49 |
| Not packaged | `hyprwayland-scanner`, `hyprland-protocols` (V2 built scanner v0.4.6 / b863271 locally; protocols are a Hyprland submodule) |
| Toolchain | gcc 16.2.1, clang 23.1.1, cmake 4.4.4, meson 1.12.1, ninja, ccache, gtest 1.18, sdl3 3.4.18, sdl2-compat |
| GPU | AMD HawkPoint `[1002:1900]` (iGPU) |
| Memory | 14 GiB total, ~2.8 GiB available, 7.3 GiB swap in use at audit time |
| Kernel | 7.2.9-1-cachyos |
| Tools present | grim, slurp, foot, alacritty, kitty, nvtop, ydotool (**forbidden**: uinput is host-wide) |
| Tools absent | perf, heaptrack, valgrind, hyperfine, tracy, wlrctl, wtype, wayland-info |
| Hyprland config | Lua (`~/.config/hypr/hyprland.lua`, `hl.*` API) |

## R1. V1 audit (`/home/towfienes/Projects/HyprVX`, `dev@15f75b0`)

**Short version:** V1 is solid compositor-agnostic camera and world math with good unit tests, plus a socket2
observer, a nested-only overlay prototype and a plugin that only observes. Nothing in V1 moves, zooms or hit-tests
real window content.

### Core (no dependencies beyond the standard library)
- **`core/src/Camera.cpp`**
  - `worldToScreen` (41-48): `(w − position_)·zoom_`
  - `screenToWorld` (50-57): `position_ + s/zoom_`
  - `setZoomAroundScreenPoint` (27-39): `position_ = worldAnchor − anchor/zoom_`
  - `visibleWorldRect` (59-67)
  - Screen coordinates are viewport-local. Output origin and scale are never used in the transform, even
    though `OutputDescriptor` stores them.
- **`CameraMotion.cpp`**
  - `updateManualMotion` (282-347): closed-form exponential damping (frame-rate independent).
  - `updateFlight` (349-426): position and zoom are lerped independently, so a zooming flight is **not**
    anchor-stable.
  - Zoom is clamped to 0.1–4.0 (236-262).
- **Drag-pan has no release momentum:** `RuntimeCameraController::panByWorld/panEnd` zero the velocity
  (`daemon/src/RuntimeCameraController.cpp:71-89`).
- **`World.cpp` / `Island.cpp`:** single-owner rule; join/detach with rollback (228, 273). Gap: the non-const
  `World::island()` exposes a mutable `Island*`, and `Island::addMember` (14-23) checks only its own members.
- **`IslandInteractionController`:** only used by tests. `simulator/visual_main.cpp:608-830` duplicates it with
  hard-coded constants.
- **Navigation:**
  - `SpatialNavigator::findNeighbor` (`SpatialNavigation.cpp:190-345`): ±65° cone, score with
    distance/angle/lateral/overlap terms.
  - `CameraHistory` (`NavigationState.cpp:53-158`): capacity 64.
  - Transactions (161-211); `ZoneStore` (213-275).
  - `frameRectInViewport` (`ViewportFraming.cpp:9-116`).
- **Multi-output** (`OutputState.cpp`, `OutputTopology.cpp:49-163`): simulator only; `eDP-2`/`DP-1` are hard-coded.
- **Tests:** 79 GTest (camera 4, camera_motion 10, geometry 3, island 3, island_interaction 6,
  spatial_navigation 8, viewport_framing 6, navigation_state 13, output_state 8, output_topology 10, world 2,
  world_ownership 6).

### Runtime / daemon libraries
- **socket2 and parsing:**
  - `Socket2Stream.cpp:54-84` path resolution; 64 KiB line cap.
  - `EventParser` has no monitor events and no geometry.
  - `RuntimeMirror` is metadata-only (256 tombstones); `RuntimeBridge` handles epochs.
- **`SpatialProjector`** (`SpatialProjection.cpp:162-177`): **invents** geometry (1000×700 slots, 3 columns,
  stride 3600). The idea of workspace regions is reusable for the unified world's initial layout.
- **Persistence** (`ProjectionStateStore.cpp:318-356`): temp file, fsync, rename, fsync of the directory, `.bak`,
  `.corrupt.N`. `ProjectionStateManager` is only used by tests.
- **Navigation input** (`RuntimeNavigationInput.cpp`): semantic actions and coalescing. Reusable.
- **Tests:** 192 GTest (parser 36, adapter 13, mirror 19, socket2 14, bridge 6, projection 27,
  projection_state 30, runtime_camera 23, runtime_input 24) + 13 Python + 1 CMake replay. Total with core and
  platform: **298, none measuring real compositor behavior.**

### Platform
- **`platform/src/HyprlandPlugin.cpp` (165 lines):**
  - 7 event-bus listeners (58-75) feeding JSON datagrams.
  - `control("enable")` refuses at 116-121 (`IWindowTransformer` has no inverse hit-test and no popups).
  - No function hooks, no render or input hooks.
  - Exact-commit check (`PluginCompatibility.hpp`).
- **`NestedGuard.cpp`:**
  - `requireNestedEnvironment` (68-102): `HYPRVX_NESTED_DEV=1` plus a 0600 marker plus `SO_PEERCRED` PID on both
    the Wayland and socket2 sockets (`unixSocketPeer` 35-52, `nestedContractMatches` 56-66).
  - Caveats: `host_signature` is trusted from the marker; the Wayland display is not re-verified at
    `wl_display_connect`; only the pure function is tested.
- **`NativeSession.cpp` (1000 lines):** layer-shell overlay drawing projected rectangles. `actualGeometry_` is
  written but never read. Untested.
- **`tools/hyprvx_nested.py` (300 lines):**
  - Start (82-178): private `/tmp/hvx-*`, signature ≠ host, both peer PIDs == launched PID, then a marker.
  - `load_session` (48-64) re-verifies before every command (good).
  - It runs `Hyprland` from `$PATH`, i.e. **unpatched**.
  - Gap: after a crash, `stop` cannot clean up a stale session.

### Untracked Phase 4 patch
`platform/patches/hyprland-v0.56.2-spatial.patch` (437 lines) is **inert**. See `research/v1-phase4/PROVENANCE.md`.
- Its equations match V2: forward `S = (L − M − pan)·z + M`; inverse `L = M + pan + (S − M)/z`.
- Defects:
  - Nothing sets the camera.
  - The wheel warp uses a logical position.
  - The render hint replaces the geometry modifier instead of composing with it.
  - Monitor choice differs between `windowAt` and `windowSurfaceAt`.
  - RC1/RC2 not addressed.
  - Struct-layout ABI hazard.

### Build
- `CMakeLists.txt:161-211`: SDL3 and Hyprland pkg-config are required unconditionally. Absolute
  `/usr/share/wayland-protocols/...` paths at 193-194.
- `build/CMakeCache.txt:554-574` still points at the deleted `/tmp/hyprland-src`. See `research/v1-build-stale/`.
- The "298/298 PASS" came from existing binaries.

## R2. V2 audit (`/home/towfienes/Projects/HyprVX-v2`, `v2/drift-mvp@31086d7`)

- **Root commit and reproducibility:** the root commit adds 10 files. `hyprland/` (nested git repo, branch
  `hyprvx-v0.56.2`, clean, HEAD `14d1232`) and `tools/hyprwayland-scanner` are untracked.
  `patches/0001-…patch` is byte-identical to `format-patch efb5099..HEAD`.
- **Diff (8 files, +288/−33):**
  - `Monitor.hpp:40-44,381`: `SSpatialCameraTransform`.
  - `KeybindManager.cpp:123-266`: 13 `hyprvx:*` dispatchers; zoom clamp [0.2, 1.5]; centre-anchored.
  - `Renderer.cpp:1181-1189,1241-1246`: the hint; `2763-2799`: damage.
  - `Window.cpp:452,1071-1084,1211-1216`.
  - `ViewHitTester.cpp:35,269,296`.
  - `InputManager.cpp:633-637,665,674,893-894,1028-1029,2177-2178`.
  - `DragController.cpp:364-365`.
- **Equations are consistent:**
  - render `z·(W_px − pan·s)`
  - inverse `M + pan + (pos − M)/z`, copy-pasted at 9 sites
  - damage `(box − M − pan)·z·s`
  - drag `Δ/z`

### Renderer root cause (confirmed by source)
- **Hypothesis "renderModif overwritten" is REFUTED for the default path.**
  - `ElementRenderer.cpp:190-194` `drawHints` assigns `renderModif = *m_data.renderModif`.
  - However, between V2's push (1188) and reset (1241-1246) no other hint element is emitted:
    `renderWorkspaceWindows`, `renderWindow`, popups, specials, pinned and fullscreen emit none, and workspace
    swipe uses `m_renderOffset` added to the position (562).
  - The only other hint producers are `renderAllClientsForWorkspace` geometry (1116-1128, identity on the
    normal path from `renderMonitor` 2191) and `renderIME` (1283-1295).
  - It *does* become a composition bug when `renderWorkspace` is called with a non-identity geometry box.
- **RC1 — surface scissor uses the untransformed box.**
  - `drawSurface` passes `damage ∩ windowBox` (unmodified) at `ElementRenderer.cpp:327,345,367,385`.
  - `renderTextureInternal` scissors to it (`OpenGL.cpp:1574-1594`) while drawing the modified quad (1512).
  - `visibleRegion()` (`SurfacePassElement.cpp:139-179`) and `renderdata.clipBox` (`Renderer.cpp:633-637`) are
    also untransformed.
  - Borders (`OpenGL.cpp:2172-2178`) apply the modifier and clip only to damage, which explains the observed
    "decorations transform, surfaces don't".
- **RC2 — pass simplification uses untransformed geometry.**
  - `CRenderPass::simplify` (`Pass.cpp:37-87`) discards and occludes using pre-camera `boundingBox()` and
    `opaqueRegion()` (`SurfacePassElement.cpp:117-137`, `RectPassElement.cpp:16-27`, `TexPassElement.cpp:28-33`).
  - Borders and shadows have no bounding box (`PassElement.cpp:3-9`), so they are never culled.
  - Opaque windows subtract their *original* rectangle from the damage of everything beneath, including the
    background clear (`Renderer.cpp:1389-1399`).
  - **Hypothesis:** stale framebuffer regions across zoom/pan steps are the user's "gray stair-steps".
- **Other render issues:**
  - The blur path applies the modifier to damage (446, 475); the non-blur path does not.
  - `drawRect` rounding uses the unmodified box (156-176).
  - The special-workspace dim (1192-1207) and dim-around (622-626) sit inside the transform.
  - Direct scanout/solitary (2181) bypasses `renderWorkspace`.
- **Input:**
  - Window, popup and subsurface hit-testing are correct (surface-local values are in unscaled units).
  - Broken:
    - `mouseMoveUnified:631-637` applies the camera to non-window surfaces (IME, layers, grabs).
    - `checkInputOnDecos` uses raw coordinates (`Window.cpp:458`; callers `InputManager.cpp:884,1022`,
      `KeybindManager.cpp:1114`, `DragController.cpp:251`).
    - Drag corner selection and tiled-to-floating placement (`DragController.cpp:106-113,185-198`).
    - Warps and constraints (`InputManager.cpp:294-344`, ~1030-1036).
    - Touch (`Touch.cpp:196-197`).
    - Tablet is not patched.
- **Damage:**
  - `damageSurface` and `damageWindow` are transformed.
  - `damageBox` and `damageRegion` are not, and decorations use them (`CHyprBorderDecoration.cpp:147`,
    `CHyprDropShadowDecoration.cpp:72`, `CHyprInnerGlowDecoration.cpp:50`, `CHyprGroupBarDecoration.cpp:95`).
  - The `IS_LAYER` check (2761) misses layer popups and subsurfaces.
  - Frame scheduling uses the untransformed box (2733-2741).
- **Harness (`scripts/run-nested.sh`):**
  - `SO_PEERCRED` check only at start (274-300).
  - `cmd_hyprctl`, `cmd_exec` and `cmd_stop` (361-392, 137-191) only check that the socket answers `version`.
  - `stop` kills the recorded PID without re-check.
  - No `HYPRLAND_NO_SD_VARS`.
- **"14/14 PASS"** (`docs/DRIFT_MVP_REPORT.md:6-20`, `scripts/e2e-spatial-test.sh:89-159`):
  - No pixel, `grim` or pointer checks.
  - #3 and #10 always pass.
  - #9 reads JSON captured before DRIFT was enabled.
  - #4–6 and #11 assert dispatcher `ok`.
  - `.agents/teamwork` gate status: PENDING.
- **Provenance:** `scripts/build.sh:263-266` forces `GIT_COMMIT_HASH=efb5099`, `GIT_DIRTY=clean`.

## R3. Hyprland v0.56.2 seam map (upstream efb5099)

### Plugin API / ABI
- `HYPRLAND_API_VERSION "0.1"` (`PluginAPI.hpp:21`). The loader checks only the API version
  (`PluginSystem.cpp:92-109`). The commit hash must be checked by the plugin
  (`__hyprland_api_get_hash`, `PluginAPI.cpp:21-35`).
- `registerCallbackDynamic` is a **no-op** (`PluginAPI.cpp:36-45`). Use `Event::bus()->m_events.*`.
- Cancellable bus events (`event/EventBus.hpp`):
  - `input.mouse.move/button/axis` (107-111)
  - touch (126-131)
  - `gesture.swipe.*`, `gesture.pinch.*` (144-156)
  - `render.stage(eRenderStage)` (134-138)
- `eRenderStage` (`SharedDefs.hpp:20-30`): `PRE, BEGIN, POST_WALLPAPER, PRE_WINDOWS, POST_WINDOWS, LAST_MOMENT,
  POST, POST_MIRROR, PRE_WINDOW, POST_WINDOW`.
- `createFunctionHook` (trampoline, udis86, x86_64 only: `HookSystem.cpp:147,251`); `findFunctionsByName` uses
  `nm -D` (`PluginAPI.cpp:325-404`).
- Build uses `-fno-lto` and exports all symbols (`CMakeLists.txt:119-122`).
- Exported, real call targets in `/usr/bin/Hyprland` (checked with objdump): `windowAt`, `windowSurfaceAt`,
  `surfaceLocalAt`, `layerSurfaceAt`, `mouseMoveUnified`, `getMouseCoordsInternal`,
  `CSeatManager::sendPointerMotion`, `setPointerFocus`, `CWindow::visibleOnMonitor`, `hasPopupAt`,
  `shouldRenderWindow`, `renderWindow`, `renderWorkspace`, `renderAllClientsForWorkspace`, `damageSurface`,
  `damageWindow`, `damageBox`, `CMonitor::addDamage`, `CRenderPass::simplify`, `CRenderPass::render`,
  `CSurfacePassElement::getTexBox`, `IElementRenderer::drawSurface`, `preDrawSurface`,
  `CDragStateController::mouseMove`, `dragBegin`, `updateDragWindow`.
- `drawHints` is **inlined**, so it cannot be hooked.

### Window transformers (why the V1 plugin refused)
- `render/transformer/Transformer.hpp:11-29`: the window main pass is redirected (`Renderer.cpp:642-653`) and
  rendered into a **monitor-sized** framebuffer (`ElementRenderer.cpp:501-624`, 507, 537), then drawn back.
- Popups are excluded (header line 15; `Renderer.cpp:800`), as are OVERLAY decorations (812-818).
- Cannot zoom out (content outside the monitor is lost) and offers no inverse input.

### Rendering
- `SRenderModifData` (`render/types.hpp:54-70`): ordered SCALE, SCALECENTER, TRANSLATE, ROTATE, ROTATECENTER;
  applied in monitor pixel space (`OpenGL.cpp:2676-2735`).
- `noSimplify` (`types.hpp:106`, checked at `Pass.cpp:174`).
- Culling:
  - `shouldRenderWindow` (`Renderer.cpp:230-231`)
  - `visibleOnMonitor` (`Window.cpp:1189-1196`)
  - damage functions `damageSurface` (2704-2758), `damageWindow` (2760-2779), `damageBox` (2794-2809)
  - all damage ends in `CMonitor::addDamage` (`Monitor.cpp:1138-1160`)
- Window box: `REALPOS = position(CURRENT) + workspace->m_renderOffset->value()` (`Renderer.cpp:562`), plus
  `m_floatingOffset` (629-630). `getTexBox` (`SurfacePassElement.cpp:20-81`).
- **Order inside `renderAllClientsForWorkspace` (1102-1252):**
  1. Background
  2. Background layers
  3. `POST_WALLPAPER`
  4. Bottom layers
  5. Pre-blur (1179)
  6. `PRE_WINDOWS` (438 / 334)
  7. Tiled windows, then tiled popups, then floating (435-524)
  8. Special dim (1187-1202)
  9. Special workspaces
  10. Pinned
  11. `POST_WINDOWS` (1231)
  12. Top layers, then overlay, then layer popups (1246), then drag icon
  13. Then, in `renderMonitor`: lock screen, IME, notifications, software cursor (2216)

  ⇒ A hint spanning PRE_WINDOWS…POST_WINDOWS transforms windows only.
- **Cursor zoom reference** (`renderMonitor` 2034, 2129-2135; `OpenGL.cpp:773-797`;
  `MonitorZoomController.cpp:28-118`): a post-process over the whole output, zoom ≥ 1 only, no input
  remapping, forces full damage. Not usable for Drift.
- **`m_renderOffset`** (`Workspace.hpp:32`): applied consistently in rendering, popups and decorations, but
  **never** in hit-testing. Translation only.
- `CPopup::reposition` constrains to the monitor box (`Popup.cpp:374-381`). `CPopup::onMap` dereferences
  `PMONITOR` without a null check (208, 224): a crash risk if a popup maps outside every monitor.
- Frame callbacks are sent only when a surface is drawn or discarded (`Compositor.cpp:795`).

### Input
- `windowAt` (`ViewHitTester.cpp:29-255`) looks up the monitor **at pos** (line 30) and tests against
  `getWindowBoxUnified` (`Window.cpp:366`) and `hasPopupAt`/`CPopup::at` (`Popup.cpp:637-685`).
- `windowSurfaceAt` (257-279) and `surfaceLocalAt` (281-308); `layerSurfaceAt` and `layerPopupSurfaceAt`
  (310-367) work in screen coordinates.
- `mouseMoveUnified` (`InputManager.cpp:230-746`):
  - IME (352), priority windows (359-364), layers (452-479), windows (486-576), bottom/background (579-588).
  - Cancelled move ⇒ early return (268-271).
  - Fallback local (631). Constraints (294-345).
  - Everything ends in `CSeatManager::setPointerFocus` and `sendPointerMotion` (`SeatManager.hpp:59-60`).
- `getMouseCoordsInternal` (1107-1109) has ~56 screen-coordinate call sites.
- `processMouseDownNormal` (861-940; border test 888).
- `CDragStateController`: `updateDragWindow` (82-127), `mouseMove` (298+, `DELTA` at 322/364/372),
  `dragEnd` (246-248).
- Touch (`Touch.cpp:100,106,193-207`), tablet (`Tablets.cpp:50`).
- Gestures: `onSwipe*`/`onPinch*` (`InputManager.cpp:2251-2317`) emit cancellable bus events first. A custom
  `ITrackpadGesture` can be registered (`TrackpadGestures.hpp:14,47`); see `CursorZoomGesture.cpp:19-80` as a
  pinch example.

### Capability matrix (what each delivery option can do)
| Capability | Official API | Plugin hooks | Patch |
|---|---|---|---|
| Transform surfaces/popups/subsurfaces/decorations | partial (hint at PRE/POST_WINDOWS, but RC1/RC2) | yes (`drawSurface` clipBox, `simplify`/`noSimplify`, or `getTexBox`/`opaqueRegion`) | yes (cleanest) |
| Culling | no | hook `visibleOnMonitor` / `shouldRenderWindow` | yes |
| Damage | coarse only (full frames) | hook `damage*` / `addDamage` | yes |
| Hit-test | no | rewrite `windowAt` + hook `windowSurfaceAt`/`surfaceLocalAt` | yes |
| Surface-local coordinates | no | hook `setPointerFocus` / `sendPointerMotion` | constraints realistically need a patch |
| Drag/resize | no | hook `CDragStateController::*` | cleaner |
| Gestures | **yes** (cancellable bus / `ITrackpadGesture`) | — | — |
| Layer-shell isolation | **yes** (stage order) | — | IME popups need a hook or patch |
| Popup off-monitor safety | no | hook `reposition` | `onMap` null check |

## R4. Caelestia audit (`/etc/xdg/quickshell/caelestia`, caelestia-shell 2.4.0, read-only)
- Window types:
  - `WlrLayershell` × 10 (Background/Bottom/Top/Overlay layers)
  - `PanelWindow` × 4, `StyledWindow` × 4 (wrappers over layer shell)
  - `WlSessionLock` × 3
  - `HyprlandFocusGrab` × 2
  - `FloatingWindow` × 2: `components/filedialog/FileDialog.qml:31`, `modules/nexus/WindowFactory.qml:24`.
    These are xdg_toplevel windows and become world windows.
- `services/Hypr.qml`:
  - Uses `Hyprland.toplevels`, `workspaces`, `activeToplevel`, `focusedWorkspace`, `focusedMonitor`, and
    `Hyprland.dispatch`.
  - `onRawEvent` (lines ~115-136) refreshes on `workspace`, `moveworkspace`, `activespecial`, `focusedmon`,
    `openwindow`, `closewindow`, `movewindow`, `*mon*`, `*workspace*`, `*window*`, `pin`, `fullscreen`,
    `changefloatingmode`, `minimize`.
  - Dynamic binds via `hl.bind` (Lua) for Caps_Lock/Num_Lock.
- Conclusion: Drift as a pure view emits none of these events and changes no ownership, so Caelestia stays
  truthful. Unified-world focus switching legitimately emits `workspace` events, which is correct semantics.

## R5. User input configuration conflicts (read-only)
- `~/.config/hypr/config/inputs.lua:14-17`:
  - `fingers=4 horizontal → workspace`
  - `fingers=3 down → close`
  - `fingers=3 up → fullscreen`
  - `fingers=3 left → float`
- `~/.config/hypr/hyprland/gestures.lua:15-23`: `workspaceSwipeFingers`, `gestureFingers=3` up → special,
  `gestureFingersMore=4` (`variables.lua:18-20`).
- `~/.config/caelestia/hypr-user.lua:7`: touchpad `natural_scroll`, `scroll_factor=0.8`, `clickfinger`,
  `tap_to_click`, `tap_and_drag`, **`drag_3fg = 1`**, `disable_while_typing`.
- ⇒ In Drift, every 3-finger gesture must be cancelled at `gesture.swipe.begin` so a pan can never trigger
  "close". Whether libinput still emits 3-finger swipes with `drag_3fg=1` is unknown (U4: hardware test).

## R6. DriftWM reference (behavior only; GPL-3.0-or-later, Rust/Smithay; code must not be copied)
- **Single canvas, no workspaces.** Bookmarks are exported via `ext-workspace-v1` "for tooling".
- **Gestures:**
  - Pan: 3-finger swipe anywhere, or trackpad scroll on empty canvas.
  - Zoom: 2-finger pinch on canvas, or 3-finger pinch anywhere.
  - Navigation: 4-finger swipe = nearest window; 4-finger pinch = fit / home; 4-finger hold = center.
  - Window actions: 3-finger double-tap-swipe moves a window (hold = cluster).
  - Context (window vs canvas) is decided at gesture start.
  - Mouse: Mod+scroll zooms at the cursor; Alt+LMB moves.
- **Config defaults:**
  - Zoom: `zoom.step 1.1`, `fit_padding 80`, `reset_on_new_window true`, `reset_on_activation true`,
    `interact_min 0`.
  - Navigation: `navigation.trackpad_speed 1.5`, `drift 0.5`, `camera_speed 0.3`.
  - Edge pan: `zone 100`.
  - Snap: `snap.gap 12` (canvas px), `distance 24` (screen px), `break_force 32` (screen px).
  - Gestures: `swipe_threshold 12`, pinch thresholds 0.85 / 1.15.
- **Window kinds:** `pinned_to_screen` (ignores pan/zoom, above normal windows); `widget` (below normal
  windows, immovable).
- **Placement:** `window_placement center | cursor | auto`.
- **Multi-monitor:** independent viewports on one canvas.

## R7. Decisions derived from research
| Decision | Rationale | Alternatives rejected |
|---|---|---|
| Hybrid: patch mechanism + plugin policy | RC1/RC2 and the `windowAt` monitor lookup require patch or hook; gestures/dispatchers have official APIs | Plugin-official-only (infeasible); full fork (V2 maintenance trap); logical-position pan (no zoom, not reversible) |
| `W = L + O`, Hyprland model untouched | Keeps tiling, ownership and Caelestia truthful; enables cross-workspace view without reassignment | Moving windows' logical positions; a physical "drift workspace" (fallback only) |
| One hint across PRE…POST_WINDOWS + per-window offset in `renderdata.pos` | Covers popups, subsurfaces and decorations; no offscreen VRAM | Per-window offscreen transformer (monitor-sized FB, excludes popups) |
| `noSimplify` while camera ≠ identity (MVP) | Removes RC2 with minimal code; optimize in Phase 7 | Camera-aware `boundingBox`/`opaqueRegion` now (more seams, premature) |
| `CameraMath.hpp` shared header | Single transform contract (Constitution IV) | Per-subsystem formulas (V2: 9 copies) |
| Delivery decided at the Phase 2 gate | User-approved; spike compares hook vs patch under the same tests | Committing to a delivery mechanism now |
