# Contract: Mechanism API (policy ↔ mechanism)

**Contract version**: 1.0 (2026-10-11, Feature 002). The functions the policy plugin calls. In form P they are defined by
the patch series and exported by the patched Hyprland binary (headers installed under the prefix's
`include/hyprland/src/hyprvx/`). In form S the plugin defines the same functions, with the same names and signatures,
in its mechanism part (research R3). The policy compiles unchanged against both.

All geometry is in global logical units. Every function runs on the compositor thread. No function writes a window's
workspace, logical geometry, tiled/floating state or layout (Constitution V).

## Camera (unchanged from Feature 001, patch 0001)

| Function | Rules |
|---|---|
| `cammath::Vec monitorOrigin(const CMonitor&)` | `M` |
| `cammath::CameraState cameraOf(const CMonitor&)` | identity when inactive |
| `bool cameraIsIdentity(const CMonitor&)` | |
| `bool setCamera(CMonitor&, const cammath::CameraState&)` | rejects invalid values (state unchanged, logged); a change damages the whole output |
| `void resetCamera(CMonitor&)` | exact identity |

## World offsets (new; patches 0010–0011)

| Function | Rules |
|---|---|
| `bool setWindowOffset(const PHLWINDOW&, const Vector2D&)` | Rejects non-finite values (unchanged, logged). A change damages the window's old and new drawn boxes (camera image of `L + O`, plus decorations). Setting the current value does nothing. |
| `Vector2D windowOffset(const PHLWINDOW&)` | The value set, or 0 if never set. Destroyed windows drop their entry. |
| `Vector2D contentOffset(const SP<CWLSurfaceResource>&)` | The offset of the window that owns a surface (main surface, subsurfaces, popups); 0 for layer, lock, IME and cursor surfaces and for surfaces without a window |
| `void resetWindowOffsets()` | Every offset → 0 (dev `panic` and plugin unload) |

### Rules the mechanism applies with offsets

- **Drawing**: in the window pass, a window with a non-identity camera or a non-zero offset is drawn under
  `cameraRenderModif(M, offsetCamera(C, O(w)))`. With identity camera and `O(w) = 0` nothing is added (FR-003).
- **Culling, damage, hit-testing, surface-local coordinates, warps, constraints, decoration hits and popup placement**
  all use `L + O` through the same camera (research R8).
- **Fullscreen**: while the active workspace has a fullscreen window, the camera and every offset of that workspace are
  ignored (identity drawing and input), as stock.
- **Drags**:
  - A compositor move drag stays blocked and logged while the camera is not identity or the window's offset is not 0
    (Feature 001 FR-012).
  - A compositor resize drag on a floating window runs through the inverse transform: the delta is divided by `z` and
    the anchor uses `L + O`.
  - On a tiled window, a resize drag is blocked and logged once per drag.

## Dev control additions (dev builds only; contract `test-tooling.md` §dev)

`hyprctl hyprvx-dev offset <address> <x> <y>` sets an offset directly, for tests that must not depend on the policy.
`state` lists non-zero offsets. `panic` also calls `resetWindowOffsets()`.
