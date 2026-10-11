# Feature Specification: Multi-Window Drift MVP (Option A)

**Feature Branch**: `v3/002-multi-window-drift-mvp`
**Created**: 2026-10-11
**Status**: Draft (user approved starting Feature 002 on 2026-10-11; implementation needs the user's approval of
`tasks.md`)
**Input**: Master plan §H Phase 3 "Multi-Window MVP (Option A)": plugin controller + world offsets `O(w)`, popups,
subsurfaces, XWayland, focus, move in Drift by a drag that changes `O(w)`, clean OFF, camera per workspace. Completion
gate: the user's 12-step scenario from the original V3 request runs in the nested session, except trackpad gestures.

## Context

Feature 001 (merged into `v3/spec-foundation` at `6931c04`) proved that one real window can be drawn and used through
a camera: pixels and pointer input agree, Normal Mode stays stock, and the camera is driven only by a dev-only control.
There is no Drift Mode yet: no way for the user to turn it on or off, one camera per monitor, every window at its
Hyprland position (`O(w) = 0`), and window moves are blocked while the camera is not identity (FR-012 of Feature 001,
DV3-11).

This feature turns that proof into a usable Drift Mode for the windows of one workspace. The user's intended
interaction (original V3 request) is:

1. The user is working in VS Code.
2. They enable Drift Mode.
3. They pinch outward to zoom out.
4. Several application windows become visible.
5. They swipe on the trackpad to move across the canvas.
6. They approach a Firefox window.
7. They pinch inward to zoom toward Firefox.
8. They click, type, scroll and work in Firefox normally.
9. They can move or snap windows together.
10. They can pin a window to the viewport.
11. They turn Drift Mode OFF.
12. Hyprland returns to its ordinary behavior without losing their session.

In this feature, steps 3, 5 and 7 use keyboard and pointer camera controls instead of trackpad gestures (trackpad is
Phase 4). In step 9 moving is in scope; snapping is Phase 5. Step 10 (pin) is Phase 5. The roadmap places snap,
groups and pin in "Phase 5 DriftWM window behavior".

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Turn Drift Mode on and off without losing anything (Priority: P1)

As the desktop owner, I can switch Drift Mode on and off at any time, and leaving it gives me back my ordinary Hyprland
desktop with every window, workspace and layout exactly as Hyprland had them.

**Why this priority**: Steps 2, 11 and 12 of the scenario; Drift Mode is unusable if leaving it can lose or disturb
the session (Constitution III and V).
**Independent Test**: In the nested session, record the window list (workspace, position, size, floating,
fullscreen) and a capture, enter Drift, change the camera and move windows, leave Drift; compare.

**Acceptance Scenarios**:

1. **Given** Drift Mode is off, **When** the user invokes the toggle command, **Then** Drift Mode turns on for the
   focused monitor and the screen shows the active workspace through that workspace's camera.
2. **Given** Drift Mode is on with a non-identity camera and moved windows, **When** the user invokes the toggle
   command or presses Super+Escape, **Then** Drift Mode turns off and the next capture is pixel-identical to a stock
   build showing the same scene.
3. **Given** the same session, **When** Drift Mode is turned off, **Then** every window's workspace, logical
   position, size, floating and fullscreen state equals what it was before Drift Mode was turned on (or what
   Hyprland itself changed meanwhile, and the sizes of floating windows the user resized in Drift, FR-009), and no
   window-move event was emitted by Drift.
4. **Given** Drift Mode was used and turned off, **When** it is turned on again on the same workspace, **Then** the
   windows reappear at the world positions they had and the workspace's last camera is restored.
5. **Given** any state, **When** the user invokes panic, **Then** Drift Mode is off within one frame and the capture
   equals the stock image.
6. **Given** Drift Mode is off, **When** frames are rendered, **Then** no extra render work is added (debug counter
   stays 0) and input is not intercepted.

### User Story 2 — See and navigate several real windows of a workspace (Priority: P1)

As the user, in Drift Mode I can zoom out to see several application windows of my workspace at once, pan across the
canvas and zoom in toward one window, with the zoom anchored at the pointer.

**Why this priority**: Steps 3–7 of the scenario, the core Drift experience.
**Independent Test**: Several test clients on one workspace with distinct world offsets; camera commands; captures
analysed against the transform contract.

**Acceptance Scenarios**:

1. **Given** three windows on the active workspace with distinct world offsets, **When** the camera zooms out to 0.25,
   **Then** each window is drawn at its contract position and size within ± 2 px with all corner markers.
2. **Given** Drift Mode on, **When** the user zooms with the zoom commands or Super + scroll wheel, **Then** the world
   point under the pointer stays under the pointer (anchor zoom) within 1 px, and the zoom stays within the allowed
   range.
3. **Given** Drift Mode on, **When** the user pans with the pan commands or by dragging the empty canvas with Super
   held, **Then** the content moves with the pointer or by the requested screen distance within 1 px.
4. **Given** two workspaces each used in Drift Mode, **When** the user switches between them, **Then** each workspace
   shows its own camera; the camera of one workspace never changes another's.
5. **Given** a camera change or a client redraw in Drift Mode, **When** partial-damage frames are compared with
   forced full-damage frames of the same state, **Then** they match (no stale regions) for windows with non-zero
   world offsets.

### User Story 3 — Work in a real window inside Drift Mode (Priority: P1)

As the user, I can click, type, scroll and use menus in any window drawn in Drift Mode, including its popups,
subsurfaces and X11 (XWayland) windows, exactly where I see them.

**Why this priority**: Step 8 of the scenario; Drift must show real, responsive applications, not thumbnails.
**Independent Test**: The virtual pointer on the verified nested display and the test clients' event logs; a test
client with a popup and a subsurface; an X11 test client; keyboard input from a nested-only virtual keyboard.

**Acceptance Scenarios**:

1. **Given** a window with a non-zero world offset under a non-identity camera, **When** the pointer moves over it,
   **Then** the window receives enter and motion at local = inverse(S) − world origin of the surface, ± 1 px.
2. **Given** that window, **When** the user clicks it, **Then** it becomes the focused window and subsequent key
   presses are delivered to it.
3. **Given** that window, **When** the user scrolls over it, **Then** it receives the scroll events.
4. **Given** a window that opens a popup and has a subsurface, **When** drawn in Drift, **Then** the popup and the
   subsurface are drawn attached to the window at its world position and receive input at their drawn location.
5. **Given** an X11 window (including an override-redirect menu of it), **When** drawn in Drift, **Then** it follows
   the world offset of its window and receives input at its drawn location.
6. **Given** a fullscreen window in Drift Mode, **When** it is drawn, **Then** it is drawn with the identity camera
   (as stock), and leaving fullscreen returns it to the Drift view.

### User Story 4 — Move windows in the canvas (Priority: P2)

As the user, in Drift Mode I can drag a window to a new place on the canvas. Moving it changes only where HyprVX
places it in the world; Hyprland's own idea of the window (workspace, position, size, tiling) is untouched.

**Why this priority**: Step 9 (move part) of the scenario; completes Feature 001 FR-012, which blocked moves.
**Independent Test**: Super + left-button drag on a window under several cameras; captures, window list and event log.

**Acceptance Scenarios**:

1. **Given** Drift Mode on with zoom z, **When** the user drags a window by a screen distance d (Super + left button
   on the window), **Then** the window's world position changes by d / z within 1 px and it is drawn under the
   pointer during and after the drag.
2. **Given** the same drag, **When** it ends, **Then** the window's logical position, size, workspace and tiling
   state in Hyprland are unchanged and no window-move event was emitted.
3. **Given** a floating window in Drift Mode, **When** the user drags its border, **Then** the window is resized for
   real (the application receives the new size) and the resize follows the pointer through the camera. Tiled windows
   are not resized by border drags in Drift Mode (documented limitation).
4. **Given** a drag in progress, **When** Drift Mode is turned off, panic is invoked or the window closes, **Then**
   the drag ends cleanly with no crash and no stuck pointer grab.

### User Story 5 — New windows appear where I am looking (Priority: P3)

As the user, a window that opens while Drift Mode is on appears at the centre of my current view, not at a far-away
layout position.

**Why this priority**: Without it, new windows can open off-screen in a zoomed or panned canvas.
**Independent Test**: Open a test client while the camera is panned and zoomed; capture.

**Acceptance Scenarios**:

1. **Given** Drift Mode on with a panned and zoomed camera, **When** a new window maps on the active workspace,
   **Then** its centre is drawn at the centre of the viewport within 2 px.
2. **Given** Drift Mode off, **When** a new window maps, **Then** it is placed exactly as stock Hyprland places it.

### Edge Cases

- A window moved to another workspace by Hyprland or by the user keeps its world offset; it is shown in Drift only
  when its workspace is the active one (Option A). A drag of that window ends when it leaves the scene.
- Enabling Drift while the monitor's active workspace is a special workspace keeps Drift OFF (`err:state`); special
  workspaces never get a camera.
- Unloading the policy plugin while Drift is on acts as panic (identity camera, mechanism offsets 0) and the
  compositor keeps running.
- A monitor that is removed while in Drift leaves Drift; when it is added again, Drift stays OFF until the user turns
  it on.
- A workspace that is destroyed and recreated with the same id gets back the camera stored for that id during the
  session.
- A pointer constraint or lock held by a client stays in force across entering and leaving Drift; its region follows
  the camera after the change (Feature 001 warp/constraint seam).
- Hyprland re-tiles windows (new window, closed window, layout change) while Drift is on: logical positions change as
  stock; world offsets are kept, so windows may overlap in the canvas.
- The focused monitor is removed or disabled while Drift is on: Drift turns off for it without a crash.
- An invalid camera request (non-finite, zoom outside the range) is rejected and the camera stays valid.
- A popup positioned outside every monitor must not crash.
- Special workspaces opened while Drift is on are drawn without the camera (as stock, documented).
- Direct scanout and solitary-client optimisations stay blocked while the camera is not identity.
- The nested test runs at output scale 1 and 1.25.
- Turning Drift on or off twice in a row (or invoking panic while off) has no further effect (idempotent).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide a Drift Mode per monitor with the states OFF, ENTERING, DRIFT and EXITING, and
  user-bindable commands to toggle it, turn it off and panic. Every transition MUST be idempotent and MUST complete
  within the command or event that triggers it, so no input is handled in ENTERING or EXITING. Entering MUST require a
  regular active workspace and MUST validate that the running compositor matches the build HyprVX was made for
  (Hyprland commit; patchset id in the patched form; plugin API hash in the stock form). A mismatch MUST keep Drift
  off and show a notification that names the expected and the running build.
- **FR-002**: Super+Escape MUST turn Drift Mode off. Panic MUST turn Drift off on every monitor within one frame,
  measured as: the first capture taken after the panic command returns equals the stock image (frame counts are not
  measured in this project).
- **FR-003**: With Drift Mode off, the compositor MUST behave as stock: pixel-identical output for the same scene, no
  extra render pass elements, and no input interception, meaning no Drift listener cancels any event (Constitution
  III).
- **FR-004**: Each workspace MUST have its own camera while in Drift Mode (Option A: the scene of a monitor is its
  active workspace). Leaving Drift MUST show the identity view; the cameras and world offsets MUST be kept for the
  session and restored when Drift is entered again.
- **FR-005**: HyprVX MUST own a world offset per window. A window is drawn, culled, damaged and hit-tested at its
  logical box plus its world offset, through the single camera transform (Constitution IV). Drift MUST NOT write any
  window's workspace, logical geometry, tiled/floating state or layout (Constitution V).
- **FR-006**: Popups, subsurfaces, decorations and X11 windows MUST follow the world offset of the window they belong
  to, for drawing and for input. An X11 override-redirect window MUST take the offset of its transient-for parent,
  otherwise of the focused X11 window of the same client, otherwise 0. Layer surfaces and their popups never get an
  offset.
- **FR-007**: The camera MUST be controllable in Drift Mode by user-bindable commands (pan by a screen distance, zoom
  by a factor anchored at the pointer, reset to identity), by Super + scroll wheel (anchor zoom, one step per wheel
  detent) and by Super + left button drag on empty canvas (pan). Empty canvas means no window content and no
  top/overlay layer surface under the pointer. Zoom MUST stay within the MVP range [0.1, 2.0].
- **FR-008**: In Drift Mode, Super + left button drag on a window MUST move that window by changing only its world
  offset, by the screen distance divided by the zoom.
- **FR-009**: In Drift Mode, resize drags on floating windows (border drags and Hyprland's own resize bind) MUST
  resize the window for real, through the inverse transform; resize drags on tiled windows MUST be ignored (and logged
  once per drag).
- **FR-010**: Pointer focus, click focus, keyboard focus and scrolling MUST reach the window drawn under the pointer,
  with surface-local coordinates computed through the inverse transform and the window's world offset. The only input
  Drift consumes is its own controls in DRIFT (Super + Escape, Super + wheel, Super + left button).
- **FR-011**: A window that maps while Drift Mode is on MUST get a world offset that puts its centre at the centre of
  the viewport.
- **FR-012**: Fullscreen windows MUST be drawn with the identity camera while fullscreen.
- **FR-013**: Layer-shell, lock-screen, IME, notification and cursor surfaces MUST stay untransformed, and HyprVX MUST
  emit no synthetic compositor events (Constitution VII).
- **FR-014**: The mechanism MUST be built reproducibly from the pinned upstream commit plus the ordered patch series,
  and MUST also be deliverable as the plugin on stock Hyprland selected by DV3-16; both forms MUST pass the same
  suite with byte-identical captures.
- **FR-015**: All verification MUST run in the nested compositor with identity re-verification before every mutating
  command; nothing runs against the host compositor. Every new channel into the nested session (virtual keyboard,
  XWayland display resolution, plugin loading) MUST refuse anything that is not the verified nested session.
- **FR-016**: The dev-only camera control of Feature 001 MUST keep working in dev builds and MUST stay absent from
  non-dev builds.

### Key Entities

- **Drift session (per monitor)**: mode state (OFF / ENTERING / DRIFT / EXITING) and the workspace whose camera is
  shown.
- **Workspace camera**: per workspace: world point at the monitor's top-left `P`, zoom `z`. Identity outside Drift.
- **Window world offset**: per window: `O(w)`, owned by HyprVX, default 0; the window's world box is its logical box
  plus `O(w)`. Kept for the session (not across compositor restarts in this feature).
- **Drag**: an in-progress window move or floating resize in Drift Mode (window, start pointer position, start world
  offset or size).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After entering Drift, moving windows, changing the camera and leaving Drift, the capture is
  pixel-identical to the stock capture of the same scene in 3/3 runs, and the window list (workspace, position, size,
  floating, fullscreen) is unchanged (floating test windows, so Hyprland does not re-tile during the test).
- **SC-002**: Windows with distinct non-zero world offsets, Wayland and X11, are drawn within ± 2 px of the contract
  position and size at zoom 0.25, 0.5, 1 and 2, at output scale 1 and 1.25.
- **SC-003**: Pointer surface-local error is ≤ 1 px for windows with non-zero world offsets, their popups and
  subsurfaces, and X11 windows, at every sampled point.
- **SC-004**: A window drag of screen distance d at zoom z changes the world offset by d / z within 1 px, with 0
  window-move events and an unchanged logical box.
- **SC-005**: Anchor zoom keeps the world point under the pointer within 1 px, cumulatively, over 20 zoom steps.
- **SC-006**: Panic returns to the stock image within one frame in 10/10 trials; toggling twice in a row changes
  nothing after the first toggle.
- **SC-007**: Partial-damage frames equal forced full-damage frames for 50 random camera/offset states with three
  windows (0 stale regions).
- **SC-008**: The 12-step scenario, with steps 3, 5 and 7 performed by keyboard/pointer camera controls and steps 9
  (snap part) and 10 deferred to Phase 5, completes in the nested session for both delivery forms; the user reviews
  the captured sequence and signs it in `checklists/verification.md` (NOT AUTOMATABLE).

## Assumptions

- Feature 001's camera transform, seams, test tooling and evidence workflow are the baseline; its contracts are
  extended, not replaced.
- Delivery follows DV3-16: plugin on stock Hyprland, with the patch series as the source of truth for the mechanism.
  The spike's open risks (copied functions, private-member access, ABI matching) remain open and are not resolved by
  this feature.
- Keyboard input in nested tests comes from a nested-only virtual keyboard; `ydotool` and any uinput device stay
  forbidden (host safety).
- World offsets and cameras live for the compositor session only (restored on re-entering Drift in the same session);
  persistence across restarts is later work.
- The per-frame cost of the per-window camera hints in Drift is measured, not budgeted, in this feature (Phase 7
  sets budgets).
- The open risks of DV3-16 stay open; Feature 002 adds no new private-member access beyond what the copied functions
  already need, and records every new hook target.
- Snap, groups/clusters, pin, navigation, bookmarks, trackpad gestures and momentum are later phases; step 10 of the
  scenario (pin) and the snap part of step 9 are therefore not part of this feature's gate.

## Out of Scope

Trackpad gestures and momentum (Phase 4); snap, clusters, pin, directional navigation, zoom-to-fit and bookmarks
(Phase 5); Caelestia compatibility layer (Phase 6); performance optimisation beyond the budgets already enforced
(Phase 7); unified world across workspaces (Milestone U; the U0 spike is evidence only and its code is not used);
multi-monitor and HDR (Phase 8); packaging and installation into the host (Phase 9); any host-compositor interaction.
