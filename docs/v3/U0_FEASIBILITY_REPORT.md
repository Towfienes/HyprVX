# U0 unified-world feasibility spike (DV3-12): report

**Date**: 2026-10-11. **Scope** (master plan roadmap row U0, §C5, U3): answer U3 with evidence. The spike puts an
`isDriftVisible` predicate into `shouldRenderWindow` and `windowAt` on a throwaway branch and observes frame callbacks,
`wl_surface.enter`, focus → workspace switch, and warp/animation side effects. It does not implement Milestone U, does
not change ownership, and **its code is not merged**.
**Where**: spike code is on the throwaway branches `v3/u0-spike` (repo: patch `0010`, test tooling) and `u0-spike`
(draft tree). Only this report and the evidence index are on `v3/spec-foundation`. Everything ran in the verified
nested session (headless output HVX-1, 1280x800, scale 1, host GPU profile Integrated). Nothing touched the host.

## Result

**U3 is feasible with five seams, not two.** 7 of 8 questions PASS. Q7 (workspace-switch animation while drifting)
FAILs the pixel-exact criterion: windows no longer slide, but sub-pixel shifts remain. The first run was BLOCKED by a
seam the master plan did not list (commit damage, Q3); the patch was extended once and the run repeated.

| Q | Question | Verdict | Observation |
|---|---|---|---|
| Q1 | Windows of ≥ 3 workspaces drawn at once at their world positions (U-1) | **PASS** | Without drift only workspace 1 is drawn (0 marker pixels in the right half). With drift A, B and C are drawn at (47,33) 200x134, (747,187) 100x66, (900,33) 200x134, which are the expected boxes for P=(0,0), z=1/3 within ±2 px |
| Q2 | Hit-test and surface-local coordinates of a window of another workspace | **PASS** | Pointer on B (workspace 2) → B local (148,100); on C (workspace 3) → (300,200); on A → (298,200). All exact (tolerance ±1) |
| Q3 | Frame callbacks of drift-visible windows | **PASS** (2nd run) | 1st run: **no** frame callback ever reached B or C. Cause: `CWindow::commitWindow` returns before any damage when the window's workspace is not visible, so the window never schedules a frame. With that gate using `isDriftVisible`, every commit (no-damage `sync` and damage `redraw`) got its `frame_done` at once, even without the spike's extra `frames` flag, because the spike damages the whole output in drift |
| Q4 | xdg_toplevel `suspended` state | **PASS** | Stock keeps windows of hidden workspaces suspended (states `[…, 9]`) even while drift draws them. Using `isDriftVisible` in `updateSuspendedStates` (flag `awake`) sends a configure without `suspended` at once |
| Q5 | `wl_surface.enter` / `leave` for windows of hidden workspaces | **PASS** | One `enter` per window at map, no `leave` through workspace switches and drift. Enter/leave are per output in Hyprland (`CWindow` sends them on monitor change only), so Option B needs nothing here |
| Q6 | Focus → workspace switch; camera does not jump; no cursor warp (U-2) | **PASS** | Click on C → active workspace 3, active window C; click on A → workspace 1. After each switch the capture is identical to the reference (0 pixels differ), and the cursor stays where it was. **Side effect:** with `follow_mouse = 1` (default) merely **hovering** B focused it and switched to workspace 2 |
| Q7 | Workspace-switch animation while drifting | **FAIL** | Slide animation (1 s), captures at 77/162/243 ms after the switch: 1112–1330 pixels differ from the static image, all on 10–14 pixel columns at vertical grid lines (64→56..59, 32→37..40): a sub-pixel horizontal shift, not a slide. Control without drift: 3 of 3 captures were mid-animation, so the measurement is valid. Likely cause: the animated render offset is rounded with the window box before the camera hint undoes it |
| Q8 | Ownership unchanged (C5, U-3, U-4) | **PASS** | No `movewindow`/`movewindowv2` event in the 52 socket2 events between drift on and off (10 `workspace`/`workspacev2` events from the focus changes). `hyprctl -j clients` before and after: workspace.id, at, size, floating, fullscreen identical for all 3 windows. After drift off the image equals the camera-only image; after panic it equals the stock start image |

Evidence (archived read-only, `sha256sum -c` OK, index in `evidence/<run-id>/`):

- `20261011-152554-patched-c15c101-u0_spike` (patchset `1dc84c182863`): **BLOCKED**. Q1 PASS, Q4 PASS, Q3 FAIL;
  the pointer steps then timed out on frame callbacks.
- `20261011-153555-patched-8ef32a4-u0_spike` (patchset `96efea72afea`, binary `7a00725e…`): **FAIL**, 7/8 PASS,
  Q7 FAIL. `answers.jsonl` has every verdict with its measurements.

## The seams Option B needs (from the spike)

The spike uses the active workspace as the rendering frame. Its windows have offset 0, so patches 0001–0009 keep
working unchanged. A window of workspace k gets the world offset `O(w) = R(k) − R(active)`, with `R(k) = ((k − 1) ·
monitor width, 0)`. When focus changes the active workspace, the effective camera shifts by the same amount, so the
image does not move (Q6). Patch 0010 adds 303 lines, 162 of them in `DriftSpike.cpp`.

| # | Seam | Stock behavior | Spike change |
|---|---|---|---|
| 1 | `IHyprRenderer::shouldRenderWindow` (both overloads) | Hidden workspaces are not rendered | `isDriftVisible(w, m)` first (culled against the camera's visible world rect) |
| 2 | Window pass (`renderWorkspaceWindows`) | One camera hint per pass | In drift, one camera hint per window with origin `P − O(w) + renderOffset(ws)`; same single transform |
| 3 | `CViewHitTester::windowAt`, `windowSurfaceAt`, `surfaceLocalAt` | Only the active workspace is hit-tested | Fallback over drift-visible windows in world coordinates; surface-local minus `O(w)` |
| 4 | `CWindow::commitWindow` | Commits of hidden-workspace windows damage nothing, so no frame and no frame callback | Gate uses `isDriftVisible` (found by the first run) |
| 5 | `CGlobalWindowController::updateSuspendedStates` | Hidden-workspace windows are suspended | Not suspended while drift-visible |

Also needed or found:

- **Contract**: the effective camera needs a world-space origin shift. The Constitution IV lint correctly rejected
  doing it by hand, so the spike added `cammath::panByWorld` to the vendored `CameraMath.hpp`. Milestone U needs it in
  `core` and in the transform contract (v1.1 → next).
- **Damage**: the spike damages the whole output on any damage while drifting. Milestone U needs the damage-union rule
  with `O(w)` (stale pixels, U-5, were not tested).
- **Frame scheduling**: `damageSurface` schedules a frame only if the surface's *layout* box overlaps the monitor (or
  the camera shows it). A drift window whose layout box is off-monitor would need this check through `O(w)`. Not
  tested: all test windows have on-monitor layout boxes.
- **Focus-follows-mouse**: hover switches workspaces (Q6). Option B needs a policy, e.g. focus on click only for
  drift windows, or no workspace switch on hover.
- **Animation**: the master plan (§C5) already says the workspace animation is suppressed in drift. Compensating it
  per window (what the spike does) leaves sub-pixel shifts (Q7). Suppressing it is the recommended direction for
  Milestone U.

## Not run

- Caelestia showing the active workspace. There is no shell in the nested session; the socket2 `workspace`/
  `workspacev2` events that Caelestia consumes were recorded instead (Q8).
- U-5 (stale pixels while panning between workspace regions); the spike uses full-output damage.
- Popups and subsurfaces of drift windows (the single-argument `shouldRenderWindow` covers them, but they were not
  tested), XWayland windows, fullscreen windows, special workspaces, multi-monitor, scale ≠ 1, render cost with many
  windows, physical hardware.

## What this means for the roadmap

Nothing in the roadmap changes; this is input for the user's decisions. U3 is not refuted: drawing, hit-testing,
focusing and frame delivery for windows of other workspaces work without moving any window to another workspace, and
the fallback "physical drift workspace" (§C5, which would need separate user approval) is not needed on this evidence.
Milestone U must budget for the five seams, the contract addition, damage through `O(w)`, a focus-on-hover policy and
animation suppression. Phase 3 (Option A) does not depend on any of this.
