# Feature Specification: DriftWM Window Behavior

**Feature Branch**: `v3/004-drift-window-behavior`
**Created**: 2026-10-12
**Status**: Draft (Feature 003 gate approved and merged on 2026-10-12; implementation needs the user's approval of
`tasks.md`)
**Input**: Master plan §H Phase 5 "DriftWM window behavior": snap, implicit cluster (from V1 Island), pin,
navigation, bookmarks, zoom-to-fit. Completion gate: L2 + L5 tests for snap/cluster. Main risk: snap oscillation.
Interaction targets: master plan §D rows Move window, Snap, Pin, Navigation. Reference only (DV3-18): phantomat's
navigation rules (`docs/v3/EXTERNAL_PROJECTS_REVIEW.md`).

## Context

Features 002 and 003 (merged into `v3/spec-foundation`) made Drift Mode a navigable canvas: the user turns it on and
off, pans and zooms with keyboard, pointer and trackpad, and moves windows with Super + drag. Each move is free: the
window lands exactly where the pointer leaves it. There is no way to line windows up, to move a group of related
windows together, to keep a window in view while the canvas moves, or to jump to a window or a remembered place.

This feature adds the window behavior of a canvas window manager on top of Drift Mode: windows snap to each other
while being moved, windows snapped together form a group that can be moved as one, a window can be pinned to the
screen, and the keyboard can jump between windows, frame a window or a group, go home, store and recall views, and go
back and forward through views.

All of it happens in Drift Mode only and only changes what HyprVX owns: each window's drawn offset, the camera, the
pin state, bookmarks and view history (master plan §C5). Hyprland's own window model (workspace membership, window
positions, tiled/floating state, layout, focus except where the user asks to focus a window) is never written. With
Drift off the desktop behaves exactly as Hyprland does.

## Clarifications

### Session 2026-10-12

- Q: Which keys trigger the actions that have no key in the master plan (bookmarks, history, center, zoom-to-fit,
  pin)? → A (Claude's default under the user's "continue without extra stops" instruction, 2026-10-12; the user can
  change it at the `tasks.md` approval gate): every action is a HyprVX dispatcher, and Drift-only built-in keys are
  added **only on combinations the user's Hyprland config does not bind** (checked read-only: Super + 1–0, Super + Alt
  / Ctrl + Super + digits, Super + P and the other listed binds stay the user's). Super + arrow keeps the master plan
  §D meaning on a monitor in Drift only (focus the nearest window in that direction, as the user's own Super + arrow
  focus binding does, plus bringing it into view); with Drift off it reaches the user's binding unchanged. The exact
  key table is chosen in the plan and recorded in the contract.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Windows snap into place while being moved (Priority: P1)

While moving a window with Super + drag in Drift Mode, the user brings it near another window; it snaps so that their
edges sit a fixed small gap apart and line up, instead of landing a few pixels off. Pulling it further away releases
it again. The snap never flickers between two positions.

**Why this priority**: Snapping is what turns a free canvas into an arrangement the user can work with; it is the
behavior the master plan names first and the one its gate tests. It is also the basis of groups (US2).

**Independent Test**: In a nested session with Drift on and three windows, move one window by Super + drag towards
another at several zoom levels; captures and the window's offset show the snapped gap and alignment, the release
distance, and no back-and-forth between positions over a slow approach.

**Acceptance Scenarios**:

1. **Given** Drift on at zoom 1 and window B 100 px to the right of window A, **When** the user drags B left until its
   left edge is within the snap threshold of A's right edge, **Then** B snaps so the gap between A's right edge and
   B's left edge equals the snap gap, and the drag continues from there.
2. **Given** the same at zoom 0.5 and at zoom 2, **When** the user approaches A, **Then** snapping starts at the same
   on-screen distance (the threshold is in screen pixels) and the resulting gap is the same world distance.
3. **Given** B is snapped to A, **When** the user keeps dragging, **Then** B stays snapped until the pointer has moved
   past the release distance, and then follows the pointer again.
4. **Given** B is dragged slowly back and forth around the snap threshold, **When** frames are recorded, **Then** B's
   position changes only at the snap and at the release, never alternating between snapped and free positions on
   consecutive frames.
5. **Given** B snaps beside A, **When** the edges perpendicular to the snap edge (tops or bottoms) are within the
   threshold of each other, **Then** they line up as well.
6. **Given** Super + drag ends, **When** the user releases the button, **Then** B stays at its last position (snapped or
   free) and Hyprland's own position of B is unchanged.

---

### User Story 2 — Snapped windows move together as a group (Priority: P1)

Windows snapped to each other form a group without any extra step. Holding Shift while starting a Super + drag on one
of them moves the whole group; a plain Super + drag still moves only that window, which leaves the group.

**Why this priority**: Moving related windows (an editor and its terminal, a browser and its notes) together is the
reason to arrange them; the master plan's gate tests clusters together with snap.

**Independent Test**: In a nested session, snap A and B together and leave C apart; Shift + Super + drag on A moves A
and B by the same distance and leaves C; a plain Super + drag on B afterwards moves B alone and B no longer moves with
A.

**Acceptance Scenarios**:

1. **Given** A and B snapped together and C apart, **When** the user Shift + Super + drags A by (dx, dy), **Then** A and
   B move by exactly (dx, dy) on screen and C does not move.
2. **Given** A–B and B–C snapped in a chain, **When** the user Shift + Super + drags C, **Then** all three move together.
3. **Given** A and B snapped, **When** the user plain Super + drags B away past the release distance, **Then** B leaves
   the group and a later Shift + Super + drag of A moves only A.
4. **Given** a group is moved, **When** the moved group comes near another window, **Then** the group snaps as one
   (using the dragged window's edges and the other members' edges).
5. **Given** groups exist, **When** the user turns Drift off and on again, **Then** the same windows are still grouped
   (groups follow from the snapped arrangement, which is kept).

---

### User Story 3 — Jump between windows and places from the keyboard (Priority: P1)

In Drift Mode, Super + arrow focuses the nearest window in that direction and brings it into view. The user can also
center the view on the focused window, frame the focused window or its whole group so it fills the screen, return to
the home view, store up to four views as bookmarks and recall them, and go back and forward through the views they
jumped between.

**Why this priority**: On an infinite canvas, finding and reaching a window is the everyday task; without it the user
pans by hand every time. The master plan lists every one of these actions.

**Independent Test**: In a nested session with windows placed around the canvas, each action is triggered by its key
or dispatcher; captures, the focused window and the camera show the expected result.

**Acceptance Scenarios**:

1. **Given** focused window A with B to its right and C below, **When** the user presses Super + Right, **Then** B is
   focused and the camera moves so B is fully visible; Super + Down from A focuses C.
2. **Given** no window lies in the pressed direction, **When** the user presses Super + arrow, **Then** focus and camera
   stay as they are.
3. **Given** a focused window anywhere on the canvas, **When** the user asks to center it, **Then** the camera moves so
   the window's center is at the center of the screen at the current zoom.
4. **Given** a focused window or its group, **When** the user asks to frame it, **Then** the camera's zoom and position
   make the window (or the whole group) fit the screen with a margin, within the zoom range.
5. **Given** any camera, **When** the user asks for home, **Then** the camera returns to the identity view (as when Drift
   was turned on with no pan or zoom).
6. **Given** the user stores the current view in bookmark 2 and moves away, **When** they recall bookmark 2, **Then** the
   camera returns to the stored view; recalling an empty bookmark changes nothing.
7. **Given** the user made several jumps (directional focus, center, frame, home, bookmark), **When** they go back,
   **Then** the camera returns to the view before the last jump; going forward redoes it; a new jump clears the
   forward list.
8. **Given** Drift is off, **When** the user presses Super + arrow, **Then** the key reaches Hyprland unchanged (the
   user's own focus binding acts as without HyprVX).

---

### User Story 4 — Pin a window to the screen (Priority: P2)

The user pins a window (for example a video or a reference document) so that it stays at the same place and size on
the screen while the canvas is panned and zoomed under it, drawn above the canvas windows. Unpinning puts it back on
the canvas at the top-left where it is visible at that moment.

**Why this priority**: Useful, but the canvas is usable without it; it builds on the same per-window state as the
other stories.

**Independent Test**: In a nested session, pin window A, pan and zoom; captures show A unchanged on screen and above
the other windows while the rest moves; clicks on A reach A at its pinned position; unpin and pan; A moves with the
canvas from where it was.

**Acceptance Scenarios**:

1. **Given** A is pinned, **When** the user pans or zooms, **Then** A's drawn box on screen does not change (position and
   size), and the other windows move as usual.
2. **Given** A is pinned and overlaps B on screen, **When** a capture is taken, **Then** A is drawn above B; a click on
   the overlap reaches A.
3. **Given** A is pinned, **When** the user unpins it, **Then** its drawn top-left at that moment is unchanged and from
   then on it moves and zooms with the canvas (its drawn size becomes its size times the zoom).
4. **Given** A is pinned, **When** another window is moved near it, **Then** nothing snaps to A and A belongs to no group.
5. **Given** A is pinned, **When** Drift is turned off, **Then** A is drawn exactly as Hyprland places it; turning Drift
   on again restores the pin.

---

### Edge Cases

- A window is snapped while the zoom changes (pinch or Super + wheel during a drag): the snap threshold follows the
  new zoom; the gap stays in world units.
- Two candidate snap targets at once (a gap between two windows): the nearest edge wins; ties go to the window that was
  snapped already, so the choice does not flip between frames.
- Snapping to a window that is only partly visible or off screen: allowed (the canvas extends beyond the screen).
- A group member is closed, moved to another workspace or becomes fullscreen: it leaves the group; the rest stays
  grouped if still snapped to each other.
- A drag is cancelled (Escape, Drift off, panic, monitor loss, workspace change): the window or group returns to where
  it was before the drag, as for Feature 002 drags.
- Directional focus with overlapping windows, windows in line and diagonal neighbours: the rules choose one window
  deterministically (nearest in the pressed direction, ties broken by the smaller cross-direction distance).
- Framing a window larger than the screen at the minimum zoom, or a very small window at the maximum zoom: the zoom is
  clamped to the zoom range and the window is centered.
- Bookmarks and history are per workspace, like the camera; switching workspace shows that workspace's own.
- A pinned window that becomes fullscreen: fullscreen wins while it lasts (identity, Feature 002 rule); the pin stays.
- Pinned windows and directional focus: a pinned window is never the target of directional focus (it is not on the
  canvas).
- Panic and plugin unload: every pin, offset and camera is shown as identity at once (Feature 002); stored state
  follows the Feature 002 rules.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: With Drift off on a monitor, nothing in this feature acts on that monitor: no snap, no group move, no
  pin drawing, no key consumed; the desktop is pixel-identical to stock Hyprland.
- **FR-002**: During a Super + drag move in Drift, the moved window MUST snap to another window's edge when the
  on-screen distance between the facing edges is at most the snap threshold (default 24 screen px); the snapped gap
  is a world distance (default 12 world px). Snap targets are the other non-pinned scene windows of the same
  workspace, whether visible on screen or not; there are none while the workspace shows a fullscreen window.
- **FR-003**: When snapped, perpendicular edges within the threshold MUST also align (top with top, bottom with
  bottom, or left with left, right with right).
- **FR-004**: A snapped window MUST stay snapped until the drag has moved it more than the release distance (default
  32 world px) away from the snapped position (hysteresis); its position MUST NOT alternate between snapped and free on
  consecutive frames for any slow drag path.
- **FR-005**: Snapping only changes the moved window's drawn offset; it MUST NOT write Hyprland's window position,
  size, workspace, floating state or layout, and MUST NOT emit Hyprland move-window events.
- **FR-006**: Windows whose edges are snapped to each other (facing edges at the snap gap within 1 world px and
  overlapping along the other axis) MUST form a group; groups are transitive and are derived from the arrangement, not
  stored separately, so they exist whether snapping is switched on or off.
- **FR-007**: Shift + Super + drag started on a grouped window MUST move every member of its group by the same screen
  distance; the group snaps as one to windows outside it. On a window in no group it moves that window alone.
- **FR-008**: A plain Super + drag of a grouped window MUST move only that window; once it is no longer snapped it
  leaves the group.
- **FR-009**: Super + arrow in Drift MUST focus the nearest non-pinned window in that direction on the same workspace
  and move the camera so it is fully visible with the frame margin (FR-011) around it (smallest pan; zoom changes only
  if it cannot fit at the current zoom); with no window in that direction, nothing changes and the key is still
  consumed.
- **FR-010**: "Center" MUST move the camera so the focused window's center is at the screen center at the current zoom.
- **FR-011**: "Frame" MUST set zoom and position so the focused window, or its whole group, fits the screen with a margin
  (default 5% of the smaller screen side), zoom clamped to the Drift zoom range [0.1, 2.0].
- **FR-012**: "Home" MUST return the camera of the current workspace to identity.
- **FR-013**: Bookmarks 1–4 per workspace MUST store and recall the camera; recalling an empty bookmark changes nothing.
- **FR-014**: Every camera jump made by FR-009–FR-013 that changes the camera MUST push the previous camera onto a
  per-workspace history (a jump that leaves the camera unchanged records nothing);
  "back" and "forward" MUST move through it like a browser history (a new jump clears forward; at most 32 entries).
- **FR-015**: Camera jumps MUST either apply at once or finish a short transition within 300 ms; any pan, zoom, drag or
  new jump during a transition MUST end it at once.
- **FR-016**: A pinned window MUST be drawn at a fixed screen position and size while Drift is on, independent of the
  camera, above every non-pinned window of the workspace (the most recently pinned window on top); pointer input on it
  MUST reach it at that drawn position. A pin follows its window to another workspace and applies there in Drift; a
  pinned box that lies partly or fully off screen (for example after an output change) stays as it is until unpinned.
- **FR-017**: Pinning MUST keep the window's drawn top-left at that moment as its fixed screen position; while pinned it
  is drawn at its own size, not zoomed. Unpinning MUST place the window on the canvas so its drawn top-left at that
  moment is unchanged; its drawn size then follows the zoom. (At zoom 1 the whole drawn box is unchanged both ways.)
- **FR-018**: Pinned windows MUST take no part in snapping, groups or directional focus.
- **FR-019**: Every action (group move, center, frame, home, bookmark store/recall, history back/forward, pin/unpin)
  MUST be available as a HyprVX dispatcher, and key handling MUST follow the clarification above. The snap on/off
  switch is a user setting (FR-022), not a dispatcher.
- **FR-020**: Key handling in Drift MUST consume only the keys this feature defines, only on a monitor in Drift; every
  other key and every key with Drift off reaches Hyprland unchanged. Each key press acts once; holding a key does not
  repeat the action; key releases are not consumed.
- **FR-021**: Drag cancellation (Escape, Drift off, panic, monitor loss, workspace change) MUST restore the window or
  group to its position before the drag (Feature 002 rule) and leave no snap or group half-applied. A member closed
  during a drag leaves the drag (the others continue); a snap target closed during a drag releases that axis.
- **FR-022**: Snap threshold, snap gap, release distance and a snap on/off switch MUST be user settings with the
  defaults above; invalid values fall back to defaults with one log line.
- **FR-023**: Both delivery forms (patched build with plugin, plugin on stock Hyprland) MUST behave identically
  (byte-identical captures in the shared tests).
- **FR-024**: Feature 001, 002 and 003 suites MUST still pass with the feature built in, in both delivery forms.

### Key Entities

- **Window placement (Drift)**: Hyprland's position and size of the window (read only) plus HyprVX's offset; the drawn
  box is their sum under the camera.
- **Snap state**: during a drag, the window or group being moved, the snapped target edge (if any) and the release
  distance left.
- **Group**: the set of windows connected by snapped edges on one workspace; derived, not stored.
- **Pin**: per window, pinned or not and the fixed screen box while pinned.
- **Bookmark**: per workspace, slots 1–4, each empty or a camera.
- **View history**: per workspace, a back list and a forward list of cameras (at most 32 entries in total).
- **Window-behavior settings**: snap on/off, threshold (screen px), gap and release distance (world px), frame margin.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In nested tests at zoom 0.5, 1 and 2 and output scales 1 and 1.25, a window dragged to within the
  threshold of another lands with the configured gap and aligned edges within 1 px, in both delivery forms.
- **SC-002**: Over a slow drag path that crosses the snap threshold back and forth at least 10 times, the snapped
  position changes only at snap and release events (0 alternating frames).
- **SC-003**: A group move moves every member by the same distance within 1 px and no non-member moves.
- **SC-004**: Each navigation action ends with the expected focused window and camera (exact for home, bookmarks and
  history; the target fully visible for directional focus; within 1 px for center and frame).
- **SC-005**: A pinned window's drawn box stays within 1 px over pans and zooms, is drawn above overlapping windows,
  and receives the clicks on it.
- **SC-006**: Across every test, Hyprland's window model (workspace, position, size, floating, fullscreen) is
  unchanged by snap, group moves, navigation and pin, and no move-window event is emitted.
- **SC-007**: With Drift off, captures stay pixel-identical to stock and every key reaches Hyprland unchanged.
- **SC-008**: Unit tests of the snap, group, navigation, framing and history rules (L2) and the nested tests above
  (L5) pass; Feature 001–003 suites pass in both delivery forms.

## Assumptions

- Features 002 and 003 are the baseline: Drift Mode, per-window offsets, camera per workspace, Super + drag move and
  pan, drag cancellation, the two delivery forms (DV3-16), test tooling and the evidence workflow; their contracts
  are extended, not replaced.
- Default numbers come from the master plan §D (threshold 24 screen px, gap 12 and release 32 world px) and the Drift
  zoom range [0.1, 2.0]; the frame margin (5%) and history size (32) are reasonable defaults.
- The V1 core's Island interaction state machine (dwell, hysteresis) and its navigation pieces (spatial navigator,
  camera history, viewport framing) are evaluated for reuse (Constitution VIII); replacing them needs source-level
  evidence.
- Groups are implicit: there is no command to create or break a group other than snapping and unsnapping.
- Bookmarks, history and pins live for the compositor session; persistence across restarts is later work.
- All tests run in nested sessions with keyboard and pointer input; no physical-trackpad (L6) item is required for this
  feature's gate (master plan gate: L2 + L5 for snap and clusters). Host Super combinations do not reach an
  interactive nested window (T045), so manual trials use the harness keyboard, not the host keyboard.
- phantomat is used as a reference for navigation rules only (DV3-18); no code is copied.

## Out of Scope

Type-to-find search, minimap, recent-window switcher and other navigation UI (not in the master plan's Phase 5 list);
snapping to screen edges or a grid; tiling inside groups; persistence of bookmarks, pins and groups across restarts;
unified world across workspaces (Milestone U); Caelestia compatibility layer (Phase 6); performance budgets (Phase 7);
multi-monitor and HDR (Phase 8); packaging (Phase 9); any change to the user's host Hyprland or Caelestia config.
