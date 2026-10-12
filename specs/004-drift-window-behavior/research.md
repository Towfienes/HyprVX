# Research: DriftWM Window Behavior (Feature 004)

Read-only findings of 2026-10-12. Source references are to this repository, to the patched draft tree
(`~/.cache/hyprvx/claude/src/hyprland-draft`, patches 0001–0013 on efb5099) or to the user's host config (read only).

## R1. Reuse of the V1 core (Constitution VIII)

- **Decision**:
  - `CameraHistory` (`core/include/hyprvx/core/NavigationState.hpp:27-70`) is reused for view history (record /
    back / forward, bounded capacity), one per workspace, capacity 32.
  - `frameRectInViewport` (`core/include/hyprvx/core/ViewportFraming.hpp:21-27`) is reused for "frame", with a
    policy of margin = 5% of the smaller screen side, zoom limits [0.1, 2.0] and `preserveCurrentZoom = false`. Its
    camera convention is mapped to the Drift camera through `CameraMath` in one adapter function, unit-tested
    against hand values.
  - `SpatialNavigator::findNeighbor` (`core/include/hyprvx/core/SpatialNavigation.hpp:37-56`) is reused for
    directional focus: its cone (65°), angular and lateral penalties and overlap bonus give a deterministic choice.
    The adapter builds a `World` of the scene's non-pinned windows from their world boxes `L(w)+O(w)`.
  - `IslandInteractionController` (`core/include/hyprvx/core/IslandInteraction.hpp:9-89`) is **not** reused: it joins
    after a 0.25 s dwell inside a 220 px margin and stores membership in `Island`, while the spec requires an
    immediate snap with hysteresis (FR-002, FR-004) and groups derived from the arrangement (FR-006). Its idea of a
    release margin larger than the join margin is kept (R2).
- **Rationale**: these pieces are pure, unit-tested in V1 and match the spec. The island controller encodes a
  different interaction (dwell-to-join).
- **Alternatives considered**: a new navigator (no gain over the tested V1 rules); reusing `Island` storage for
  groups (needs join/leave bookkeeping that can disagree with what is drawn).

## R2. Snap rule with hysteresis

- **Decision**: a pure function `snapMove(input) -> {adjusted world delta, new snap state}` in a new core module
  `WindowBehavior`. Input: the moving set's world boxes at the unsnapped candidate position, the other scene windows'
  world boxes, zoom, settings and the current snap state.
  - **Attach**: per axis, if a facing edge pair is within `threshold / zoom` world units (threshold in screen px) and
    the boxes overlap on the other axis, the candidate shifts so that the gap is `gap` world units. The nearest pair
    wins; ties keep the current target.
  - **Align**: with an attached axis, perpendicular edges within `threshold / zoom` align (start with start, end with
    end).
  - **Hold**: while attached, the snapped position is kept until the unsnapped candidate is more than `release` world
    units from it on that axis (release 32 > threshold 24 at zoom 1, so attach and release never trade places on
    consecutive frames).
- **Rationale**: per-axis state with a larger release than attach distance is the classic anti-oscillation rule
  (master plan §D "có hysteresis"). The state lives in the drag, so the result depends on the path, as hysteresis
  needs; L2 tests drive slow back-and-forth paths (SC-002).
- **Alternatives considered**: time-based dwell (latency, frame-rate dependence); snapping only at release (no live
  feedback).

## R3. Groups

- **Decision**: `groupOf(window, boxes, gap, tolerance)` returns the connected component of the "snapped neighbour"
  relation: facing edges at `gap ± 1` world unit and overlapping on the other axis. It is computed from current world
  boxes when a Shift + Super drag begins and when "frame group" runs. Nothing is stored, so Drift off/on, closing a
  window or moving it away updates groups by construction.
- **Rationale**: the spec makes groups implicit; derived state cannot drift from what is drawn.
- **Alternatives considered**: storing islands (R1).

## R4. Group drag on the Feature 002 drag controller

- **Decision**: `DriftModel::beginMove` gains a list form: a drag holds `{window, startOffset}` for every member.
  Plain Super + left drag passes one window; Shift + Super + left drag passes `groupOf(target)`. `dragMotion` applies
  the same world delta to every member after `snapMove` (moving set = members; others = rest of the scene). `dragEnd`
  keeps the offsets; cancellation restores every start offset (FR-021). Shift is read at drag begin only.
- **Rationale**: Feature 002 already moves one window through `O(w)` with cancellation (`DriftModel.hpp:106-128`,
  `platform/plugin/src/policy/Input.cpp:99-121`); a list keeps one code path.
- **Alternatives considered**: a separate group-drag state machine (duplicates cancellation and monitor checks).

## R5. Pin needs a mechanism extension (patch 0014)

- **Finding**: the mechanism draws every scene window under the workspace camera with an optional per-window world
  offset (draft `src/render/Renderer.cpp:437-452`: `renderSceneWindow` pushes `HyprVX::windowRenderModif(monitor,
  OFFSET)`). An offset can move a window but cannot keep its **size** fixed while the camera zooms, and floating
  windows are drawn in Hyprland's stacking order, so "above every non-pinned window" (FR-016) is out of reach of the
  policy as well.
- **Decision**: a narrow mechanism patch 0014 "screen-pinned windows" and the matching form S copies (DV3-16):
  - Mechanism state: per window, an optional screen position `pin(w)` in monitor-local logical px. API:
    `setWindowPin(window, optional<Vector2D>)`, `windowPin(window)`, `resetWindowPins()`.
  - Render: in `renderWorkspaceWindows` pinned windows are skipped in the regular loops and drawn after the floating
    pass under a hint of identity camera with translation `pin(w) − L(w)`. With Drift off or fullscreen nothing
    changes (identity, no pin).
  - Hit-test: the view hit-tester checks pinned windows first, mapping the point with the same translation;
    surface-local coordinates and popups use the same mapping (as patch 0011 does for offsets).
  - Damage and culling: the pinned window's screen box is damaged on pin changes and its own commits; culling uses
    the pinned box.
- **Rationale**: the master plan defines pin as "bỏ qua camera, nằm trên các cửa sổ khác" (§D); offsets alone do
  neither. The change follows the Feature 002 pattern (0010/0011 for offsets).
- **Alternatives considered**: Hyprland's own `pin` (writes Hyprland's window model and means "shown on every
  workspace": Constitution V and a different meaning); offset-only pin (size scales with zoom, violates FR-016).

## R6. Camera jumps are instant

- **Decision**: center, frame, home, bookmark recall, history and directional focus set the camera at once
  (`DriftModel::setCamera`); a running glide is stopped first (Feature 003 `stopGlide`). FR-015 allows an instant
  jump; a timed transition is left to later work.
- **Rationale**: deterministic end states for tests and no frame loop.

## R7. Keys and dispatchers (spec clarification, option B)

- **Finding** (read-only: `~/.config/hypr/variables.lua`, `~/.config/hypr/hyprland/keybinds.lua:92-125`): the user
  binds Super + arrows (focus), Super + Shift + arrows (move window), Super + digits (workspaces), Super + Alt and
  Ctrl + Super + digits, Super + P (Hyprland pin), Super + Page_Up/Down and many Super + letter keys. Unbound: Super +
  Home/End, Super + F1–F4, Super + Shift + F1–F4, Super + bracketleft/bracketright, Super + Shift + P,
  Ctrl + Super + End, Super + Shift + End.
- **Decision**: Drift-only keys (consumed only on a monitor in Drift):
  - Super + Left/Right/Up/Down: directional focus (master plan §D).
  - Super + Home: home.
  - Super + End: center the focused window.
  - Super + Shift + End: frame the focused window.
  - Ctrl + Super + End: frame its group.
  - Super + F1–F4: recall bookmark 1–4.
  - Super + Shift + F1–F4: store bookmark 1–4.
  - Super + bracketleft / bracketright: history back / forward.
  - Super + Shift + P: pin / unpin the focused window.
  - Shift + Super + left drag: group move.

  Each action is also a dispatcher: `hyprvx:focus l|r|u|d`, `hyprvx:center`, `hyprvx:frame window|group`,
  `hyprvx:home`, `hyprvx:bookmark store|recall N`, `hyprvx:history back|forward`, `hyprvx:pin`, `hyprvx:unpin`. Key
  detection extends the Feature 002 key listener (keysym and modifiers at key press).
- **Rationale**: no binding the user has is taken, except Super + arrow on a Drift monitor, which the master plan
  assigns and whose meaning (focus in a direction) is kept.

## R8. Settings

- **Decision**: config values `plugin:hyprvx:snap` (bool, 1), `snap_threshold` (screen px, 24, > 0), `snap_gap`
  (world px, 12, ≥ 0), `snap_release` (world px, 32, ≥ threshold), `frame_margin` (fraction, 0.05, [0, 0.4]). Invalid
  values fall back with one log line (Feature 003 pattern).

## R9. Test approach

- **L2**: `tests/unit/test_window_behavior.cpp` covers snap attach / align / hold / release on scripted slow paths
  (SC-002), zoom scaling, tie-breaking, groups (chains, release), the framing and navigator adapters, history and
  bookmarks; two seeds and a mutation check (Feature 002/003 practice).
- **L5**: nested tests `t_snap.sh`, `t_snap_osc.sh` (frame by frame), `t_group.sh`, `t_nav.sh`, `t_pin.sh`,
  `t_wb_off.sh` (Drift off: stock captures, keys pass through), each with `hyprctl clients` and socket2 `movewindow`
  checks (SC-006). Keys come from `hyprvx-vkeyboard`, whose fixed key table (`tools/vkeyboard/main.cpp:56-`) gains
  Home, End, F1–F4, bracketleft/right, arrows, Left Shift and Left Ctrl; drags come from vpointer. Both forms, both
  scales; parity with `compare_forms.py`.
- **Mechanism**: form P runs patch 0014; form S runs the regenerated copied bodies; the single-transform lint covers
  the new patch and the new core module.
