# Contract delta: Mechanism API 1.1 (screen-pinned windows)

**Contract version**: 1.1 (2026-10-12, Feature 004). Extends `specs/002-multi-window-drift-mvp/contracts/mechanism-api.md`
1.0. Form P: patch `0014-hyprvx-screen-pinned-windows.patch`. Form S: the same functions in the plugin's mechanism part
plus the regenerated copied bodies (DV3-16). The policy compiles unchanged against both.

| Function | Rules |
|---|---|
| `bool setWindowPin(const PHLWINDOW&, std::optional<Vector2D> screen)` | `screen` = monitor-local logical top-left; `nullopt` unpins. Rejects non-finite values (unchanged, logged). Damages the old and new drawn boxes. |
| `std::optional<Vector2D> windowPin(const PHLWINDOW&)` | the value set; destroyed windows drop it |
| `void resetWindowPins()` | every pin cleared (dev `panic`, plugin unload) |

## Rules

- **Applies only** while the window's monitor has an active camera (`activeCamera(monitor)` set by the policy, i.e. in
  Drift, identity camera included) and its active workspace has no fullscreen window; otherwise a pinned window is
  drawn and hit-tested exactly as stock.
- **Drawing**: pinned windows are skipped in the tiled and floating loops of `renderWorkspaceWindows` and drawn after
  the floating pass, in pin order, under identity camera with translation `M + pin(w) − L(w)`; popups and subsurfaces
  follow.
- **Hit-testing and input**: pinned windows are tested before every other window (last pinned first); surface-local
  coordinates, warps, constraints and popups use the same translation.
- **Culling and damage**: the pinned screen box is used; a pin change damages old and new boxes.
- No function writes Hyprland's window position, size, workspace, floating state, stacking order or layout
  (Constitution V).

## Dev control

`hyprctl hyprvx-dev pin <address> <x> <y>` / `unpin <address>`; `state` lists pins; `panic` also resets pins.
