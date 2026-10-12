# Contract delta: Drift Mode control 1.1 (trackpad)

**Contract version**: 1.1 (2026-10-12, Feature 003). Extends `specs/002-multi-window-drift-mvp/contracts/drift-control.md`
1.0; everything in 1.0 stays valid (dispatchers, Super + Escape, Super + wheel for wheel sources, Super + drag,
`hyprctl hyprvx state`). Implemented by the policy plugin `hyprvx-drift` in both delivery forms (research R1, R10).

## Built-in trackpad inputs

Active only for sequences that **begin** on a monitor in DRIFT; the target is fixed at begin
(`contracts/gesture-model.md` `decideTarget`). A consumed sequence has every event cancelled (no keybind, no
`gesture =` binding, no client delivery) until it ends, even if Drift is left meanwhile; after the monitor leaves
Drift, consumed events change nothing.

| Input (at begin) | Target | Effect |
|---|---|---|
| Two-finger scroll on empty canvas | Pan | `C = panByScreen(C, −(dx, dy) · f)`, `f = input:touchpad:scroll_factor × pan_factor` |
| Super + two-finger scroll, anywhere | Pan | as above |
| Two-finger scroll over a window, no Super | PassThrough | stock delivery (same values) |
| Pinch (2 fingers) on empty canvas, or Super + pinch anywhere | Zoom | `C = pinchCamera(C0, M, a0, scale)` per update; `a0` = pointer at begin |
| Pinch (2 fingers) over a window, no Super | PassThrough | stock delivery (browser zoom etc.) |
| 3-finger swipe, anywhere | Pan | `C = panByScreen(C, delta · pan_factor)` per update (content follows the fingers) |
| 3-finger pinch, anywhere | Zoom | as pinch |
| 4+-finger swipe or pinch | PassThrough | stock (`gesture = 4, horizontal, workspace` keeps working) |
| 3-finger swipe or pinch over a top/overlay layer or layer popup, with a fullscreen window on the scene, or during a Super + drag | Ignore | consumed, no effect (no `gesture =` binding fires) |
| 2-finger scroll or pinch over a top/overlay layer or layer popup, or with a fullscreen window on the scene | PassThrough | stock |
| 2-finger scroll or pinch while a Super + drag is in progress | Ignore | consumed, no effect |

- Cancelled gesture (`end` with `cancelled`): camera back to its value at begin; no glide.
- Pan end (scroll stop on both axes, or swipe end): if `plugin:hyprvx:glide > 0` and the release speed
  (`ReleaseVelocity`) is at least `glide_min_speed`, a glide starts (`Glide`); otherwise the camera stays.
- A glide stops at once on any axis event, gesture begin, mouse button, key, `hyprvx:off`/`toggle`/`panic`,
  workspace change on its monitor, or monitor removal. It advances only on `render.preChecks` of its monitor (frame attempts while it moves).
- Wheel, continuous and tilt scroll sources keep the 1.0 behavior (Super + wheel zooms; otherwise delivered).

## Config values (added)

| Key | Type | Default | Rule (invalid value → default, one log line) |
|---|---|---|---|
| `plugin:hyprvx:pan_factor` | float | 1.0 | finite, > 0 |
| `plugin:hyprvx:glide` | float | 0.5 | finite, in [0, 1]; 0 disables glide |
| `plugin:hyprvx:glide_min_speed` | float | 100 | finite, ≥ 0 (px/s) |

`plugin:hyprvx:zoom_step` (1.0) is unchanged and applies only to wheel zoom.

## State query

`hyprctl hyprvx state` adds `"glide": true|false` per monitor (bookkeeping only; never a pass condition,
Constitution II).
