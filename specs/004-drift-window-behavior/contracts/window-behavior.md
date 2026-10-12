# Contract: Window-behavior model 1.0 (core, compositor-free)

**Contract version**: 1.0 (2026-10-12, Feature 004). New module `core/include/hyprvx/core/WindowBehavior.hpp` and
`core/src/WindowBehavior.cpp`, namespace `hyprvx::drift`. Pure functions and small value types: no Hyprland types, no
clock, no I/O. Units: world px (logical), screen px only where named. Unit tests: `tests/unit/test_window_behavior.cpp`
(research R9).

## Types

- `struct SnapSettings { bool enabled = true; double threshold = 24.0 /*screen px*/; double gap = 12.0;
  double release = 32.0; }` (world px for gap and release). `validSnapSettings(s)`: finite, threshold > 0, gap >= 0,
  release >= threshold.
- `struct WorldBox { WindowId id; cammath::Box box; }` (box = `L(w) + O(w)`, size of the window).
- `struct AxisSnap { bool attached = false; WindowId target = 0; double snapped = 0.0; }` and
  `struct SnapState { AxisSnap x, y; }` (one per drag; `snapped` = the snapped coordinate of the moving set's
  bounding-box origin on that axis).
- `struct SnapResult { cammath::Vec delta; SnapState state; }`.

## Functions

- `SnapResult snapMove(const std::vector<WorldBox>& moving, cammath::Vec candidateDelta,
  const std::vector<WorldBox>& others, double zoom, const SnapSettings&, const SnapState& previous)`.
  - `moving` holds the boxes at the drag start; `candidateDelta` is the unsnapped world delta of the drag.
  - Disabled settings, invalid settings, non-finite input, zoom <= 0 or empty `moving` return
    `{candidateDelta, {}}`.
  - Per axis (x shown; y the same with top/bottom):
    - **Hold**: if `previous.x.attached`, the target is still in `others` and the unsnapped origin is within `release`
      of `previous.x.snapped`, keep `snapped` (delta.x = snapped − start origin x).
    - **Attach**: otherwise find the facing edge pairs between the moving bounding box and each other box that overlap
      on y (open intervals) with `|distance − gap| <= threshold / zoom`, where distance = other.left − moving.right
      (moving to its left) or moving.left − other.right; pick the smallest `|distance − gap|`, ties to
      `previous.x.target`, then to the lower id; set the origin so that the gap is exactly `gap`.
    - **Align**: when y is attached and x is not, x aligns the left or right edges of the moving box with the y-target's
      when within `threshold / zoom` (and y likewise when x is attached).
    - Otherwise free: delta.x = candidateDelta.x, state cleared.
  - Deterministic: the same inputs give the same result.
- `std::vector<WindowId> groupOf(WindowId seed, const std::vector<WorldBox>& boxes, double gap, double tolerance = 1.0)`:
  the connected component of the relation "facing edges at `gap ± tolerance` and overlapping on the other axis",
  sorted by id; `{seed}` when the seed has no neighbour; empty when the seed is not in `boxes`.
- `std::optional<WindowId> neighbour(WindowId from, Direction dir, const std::vector<WorldBox>& boxes)`: adapter over
  V1 `SpatialNavigator::findNeighbor` (default policy); `nullopt` when none.
- `cammath::CameraState frameCamera(const cammath::Box& world, cammath::Vec monitorSize, double margin,
  cammath::ZoomLimits)`: adapter over V1 `frameRectInViewport` (`preserveCurrentZoom = false`, margin =
  `margin × min(monitor w, h)` screen px); the box center maps to the screen center; zoom clamped.
- `cammath::CameraState centerCamera(const cammath::CameraState& c, cammath::Vec monitorSize, cammath::Box world)`:
  same zoom, the box center at the screen center.
- `cammath::CameraState revealCamera(const cammath::CameraState& c, cammath::Vec monitorSize, cammath::Box world,
  double margin, cammath::ZoomLimits)`: unchanged if the box is fully visible; else the smallest pan that shows it;
  if it cannot fit at the current zoom, `frameCamera`.
- `class Bookmarks { void store(int slot, CameraState); std::optional<CameraState> recall(int slot) const; }`, slots
  1–4 (others rejected).
- View history: V1 `CameraHistory` with capacity 32, wrapped so that its `CameraView` maps to `CameraState` exactly
  (`historyRecord`, `historyBack`, `historyForward`).

All camera results come from `CameraMath` operations; no camera arithmetic outside it (Constitution IV).
