# R1 source verification (T044), Feature 001 Sprint 1

Written by Claude on 2026-10-11 **before** the clean-room re-run results were read (DV3-15: author and verifier are
the same agent, so the source review is fixed first). Source: `patches/hyprland/0001–0005` at repo commit `fbd98a9`
(patchset `47b6baa12904`), applied with `scripts/apply-patches.sh` to Hyprland `efb5099`; line numbers refer to that
applied tree (`~/.cache/hyprvx/claude/src/hyprland-efb5099/v3-dev`, HEAD `2e1b93f3`).

## (a) FR-002: identity camera takes the stock path

| Path | Source | Identity behaviour |
|---|---|---|
| Single gate | `src/hyprvx/SceneCamera.cpp:13-19` `activeCamera()` | returns `nullopt` when `cameraIsIdentity()` (`src/hyprvx/MonitorCamera.cpp:39`: exact `cammath::isIdentity`) |
| Camera hint | `src/render/Renderer.cpp:1182-1188` | `pushHyprvxCamera` adds an element only `if (HYPRVXCAMERA)`; no element, no counter increment otherwise |
| `noSimplify` | `src/render/Renderer.cpp:2179-2185` | set only when `activeCamera()` has a value; restored by the scope guard |
| Clip/damage/rounding branches | `src/render/ElementRenderer.cpp:281, 300, 414`; `src/render/OpenGL.cpp:2231, 2326` | all gated on `SRenderModifData::hyprvxCamera`, which only `cameraRenderModif()` sets (`src/hyprvx/SceneCamera.cpp:42`; default `false` at `src/render/types.hpp:71`) |
| Culling | `src/desktop/view/Window.cpp:1196-1197` | camera branch only when `activeCamera()`; otherwise the stock monitor-rect test |
| Damage | `src/render/Renderer.cpp:2751, 2774-2775, 2805-2806, 2837-2838` | `addCameraDamage()` / `cameraShows()` return immediately without a camera (`src/hyprvx/SceneCamera.cpp:50, 59, 69`); stock damage is always added |
| Direct scanout / solitary | `src/output/Monitor.cpp:1816-1817, 2037-2038` | blocked only `if (!cameraIsIdentity())` |

Native evidence for the same claim is T-ID (pixel identity with stock, US1.1); this table is the source half of US1.2.

## (b) Constitution IV: one transform

- `scripts/check-single-transform.sh` (self-tested) reports no camera arithmetic outside the vendored
  `src/hyprvx/CameraMath.hpp` for all five patches.
- Manual review of the non-vendored HyprVX code: `cameraRenderModif()` takes the translation from
  `cammath::forward(camera, M, M)` and the scale as a copy of `camera.zoom`; damage uses `cammath::damageBox`;
  culling uses `cammath::visibleWorldRect`; validation uses `cammath::validate`. The only remaining arithmetic is
  logical-to-pixel conversion (`* monitor.m_scale`) and integer rounding of regions (`roundOutBox`/`roundInBox`).

## (c) RC1 / RC2 coverage

- RC1 (surface scissor in untransformed space): `drawSurface` intersects damage with the modified, rounded-out
  window box and transforms the visible-region clip (`ElementRenderer.cpp:281-288`); `preDrawSurface` transforms
  the window clip box (`ElementRenderer.cpp:414-417`); `renderBorder` skips the modified interior and scissors the
  rounded-out ring (`OpenGL.cpp:2231-2238, 2326-2333`).
- RC2 (pass simplification with untransformed bounds): simplification is off while a camera applies
  (`Renderer.cpp:2179-2185`); measured cost is informational (T045).

## (d) FR-004: lock screen, IME, notifications and cursor are outside the camera

The camera hint is pushed before the window pass and restored to the outer modifier before `RENDER_POST_WINDOWS`
(`Renderer.cpp:1188, 1199, 1248-1250`); top/overlay layers and layer popups follow it inside
`renderAllClientsForWorkspace`. `renderMonitor` then draws `renderLockscreen` (`:2205`), `renderIME` (`:2208`), the
notification overlay (`:2211`) and software cursors (`:2245`) after `renderWorkspace` returned, i.e. with no camera
hint in the pass. These are not exercised natively in Sprint 1 (source evidence, as `tasks.md` T044 requires).

## Tests changed after patches existed (from `integration-log.md`)

| Test | Change | Reason |
|---|---|---|
| `tests/native/gate_n.sh` S2 | judge every V2 capture; compare only on-screen border edges | first version used z = 0.5 only, where V2 draws no border; partly off-screen borders were compared as clipped rectangles |
| `tests/native/t_layer.sh` | `--region` for the window box | the layer testclient uses the same marker colors |
| `t_zoom`, `t_pan`, `t_layer`, `run_sprint1` | expectations in physical pixels; same logical output at every scale | the first scale-1.25 run mixed logical and physical pixels |
| `tests/native/lib.sh` | unique run-ids; stop own session on exit | colliding run directories; a BLOCKED test left a session running |

No tolerance or threshold was changed.
