# Feature Specification: Trackpad-Native Drift UX

**Feature Branch**: `v3/003-trackpad-native-ux`
**Created**: 2026-10-12
**Status**: Draft (Feature 002 gate approved and merged on 2026-10-12; implementation needs the user's approval of
`tasks.md`)
**Input**: Master plan §H Phase 4 "Trackpad UX": gesture arbitration, anchor zoom, momentum, cancel; hardware
validation. Completion gate: L6 checklist signed by a human. Interaction targets: master plan §D.

## Context

Feature 002 (merged into `v3/spec-foundation` at `dd4f9b9`) made Drift Mode usable with keyboard and pointer
controls: the user turns it on and off, pans with Super + drag on empty canvas, zooms with Super + wheel and moves
windows with Super + drag. Steps 3, 5 and 7 of the user's scenario ("pinch outward to zoom out", "swipe on the
trackpad to move across the canvas", "pinch inward to zoom toward Firefox") were done with those stand-in controls.

This feature makes the trackpad the primary way to navigate Drift Mode: two-finger scrolling pans, pinching zooms at
the pointer, a pan released with speed glides to a stop, and the multi-finger gestures that the user's Hyprland
setup maps to window actions can never fire by accident while the canvas is being navigated.

Two facts about the environment shape how this feature can be validated (both checked read-only on 2026-10-12):

1. **A nested session receives two-finger scrolling but no pinch, swipe or hold gestures.** The nested compositor
   gets its input from the host as an ordinary Wayland client. The installed backend library (aquamarine 0.15.1)
   binds the plain pointer interface, including scroll source, scroll stop and high-resolution scroll, and does not
   bind the pointer-gestures interface. Physical pinches and swipes on the real trackpad therefore never reach a
   nested session.
2. **The host compositor acts on 3- and 4-finger gestures before any client sees them.** In the user's host config:
   3 fingers down = close window (and toggle a special workspace), up = fullscreen / special workspace, left = float;
   4 fingers horizontal = workspace switch, 4 fingers down = suspend-then-hibernate. A 3- or 4-finger gesture made over
   a nested session window acts on the host (it can close the nested session window or suspend the machine).

The host config is never changed (Constitution I, VII). The design target is HyprVX running inside the user's own
compositor, where all of these gestures reach the HyprVX policy; nested sessions reproduce that situation with a
nested config that contains the same gesture bindings and a nested-only synthetic gesture input.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Pan the canvas with two fingers (Priority: P1)

In Drift Mode the user scrolls with two fingers over empty canvas and the content follows their fingers. Holding
Super, they can pan with two fingers anywhere, even over a window. Without Super, two-finger scrolling over a window
still scrolls that window, exactly as in Normal Mode.

**Why this priority**: Panning is the most frequent canvas action and the only trackpad input that also reaches a
nested session, so it is both the core value and the first thing that can be validated on real hardware.

**Independent Test**: In a nested Drift session with three test windows, scroll sequences (finger source) over empty
canvas move every window's drawn box by the scroll distance; the same sequence over a window is delivered to that
window and moves nothing; with Super held it pans instead. Repeated by a human on the real trackpad (L6).

**Acceptance Scenarios**:

1. **Given** Drift is on at zoom z, **When** a two-finger scroll of (dx, dy) screen pixels starts and ends over empty
   canvas, **Then** every window's drawn box moves by the panned distance in the direction the fingers moved (±1 px)
   and no window receives a scroll event.
2. **Given** Drift is on, **When** the same scroll starts over a window without Super, **Then** the window receives
   the scroll events unchanged (same values as in Normal Mode) and the camera does not move.
3. **Given** Drift is on, **When** a scroll starts with Super held over a window, **Then** the canvas pans and the
   window receives no scroll event.
4. **Given** a scroll started over empty canvas, **When** the pointer passes over a window before the fingers lift,
   **Then** the whole sequence keeps panning (the target is chosen when the sequence begins and kept until it ends).
5. **Given** Drift is off, **When** the user scrolls anywhere, **Then** behavior and captures are identical to stock.

---

### User Story 2 — Pinch to zoom at the pointer (Priority: P1)

The user pinches outward over empty canvas to zoom out and inward to zoom in; the point under the pointer stays
under the pointer. With Super held, pinching zooms the canvas anywhere. Without Super, a pinch over a window goes to
the application (for example browser zoom).

**Why this priority**: Pinch zoom is steps 3 and 7 of the scenario and the defining trackpad interaction of Drift.

**Independent Test**: Nested synthetic pinch sequences at known pointer positions: the world point under the pointer
before the pinch is under the pointer after it (±1 px), zoom equals the start zoom times the pinch scale within the
range [0.1, 2.0]; a pinch over a window without Super is delivered to that window as a pinch.

**Acceptance Scenarios**:

1. **Given** Drift is on at zoom z, **When** a pinch with final scale s happens over empty canvas, **Then** the zoom
   becomes z·s clamped to [0.1, 2.0] and the world point under the pointer at the start stays under it (±1 px).
2. **Given** Drift is on, **When** a pinch happens over a window without Super, **Then** the window receives the
   pinch and the camera does not change.
3. **Given** Drift is on, **When** a pinch happens with Super held over a window, **Then** the canvas zooms and the
   window receives nothing.
4. **Given** a pinch in progress, **When** the system cancels the gesture, **Then** the camera returns to its state at
   the start of the pinch.

---

### User Story 3 — Multi-finger gestures never trigger window actions while navigating (Priority: P1)

While Drift is on, 3-finger gestures that the user's compositor maps to window actions (close, fullscreen, float,
special workspace) never perform those actions on a Drift monitor; 3-finger swipe pans the canvas and 3-finger pinch
zooms it. 4-finger horizontal swipes keep switching workspaces. With Drift off every gesture behaves exactly as
configured.

**Why this priority**: A pan that closes a window is data loss; master plan §D makes cancelling these gestures a hard
requirement before trackpad navigation ships.

**Independent Test**: A nested session whose config carries the same 3- and 4-finger bindings as the user's host
config. Synthetic 3-finger swipes in every direction and 3-finger pinches in Drift: no window closes, changes
fullscreen or floating state, no special workspace toggles, and the camera pans or zooms; the same gestures with
Drift off perform the bound actions (control). 4-finger horizontal swipes switch workspaces in both modes.

**Acceptance Scenarios**:

1. **Given** Drift is on and a 3-finger "down = close" binding exists, **When** a 3-finger swipe down happens,
   **Then** no window closes and the canvas pans by the swipe distance.
2. **Given** Drift is on, **When** 3-finger swipes up and left happen, **Then** fullscreen, special workspace and
   floating state are unchanged and the canvas pans.
3. **Given** Drift is on, **When** a 3-finger pinch happens, **Then** the canvas zooms at the pointer as in User
   Story 2.
4. **Given** Drift is on, **When** a 4-finger horizontal swipe happens, **Then** the workspace switches as configured
   and the camera of each workspace is kept (Feature 002 camera per workspace).
5. **Given** Drift is off, **When** the same gestures happen, **Then** the configured actions run exactly as stock.

---

### User Story 4 — Released pans glide to a stop (Priority: P2)

When the user lifts their fingers while panning quickly, the canvas keeps moving and slows down smoothly, like a
physical surface. Touching the trackpad again, clicking, pressing a key or leaving Drift stops it at once. A setting
controls how far the canvas glides, including turning gliding off.

**Why this priority**: Momentum makes long distances cheap to travel, but navigation works without it.

**Independent Test**: Nested scroll and swipe sequences with known release speed: the glide distance and duration
match the configured friction within tolerance, any new input stops the glide within one frame, frames stop being
produced once the glide ends, and the setting 0 produces no glide.

**Acceptance Scenarios**:

1. **Given** momentum is enabled, **When** a pan is released at speed v, **Then** the canvas keeps moving in the same
   direction, slowing continuously, and stops after a distance that matches the configured friction (±2 px).
2. **Given** a glide in progress, **When** a new scroll, gesture, click or key press happens, **Then** the glide stops
   within one frame and the new input is handled normally.
3. **Given** a glide in progress, **When** Drift is turned off, panic is triggered or the workspace changes, **Then**
   the glide stops and nothing moves afterwards.
4. **Given** momentum is set to 0, **When** a pan is released at any speed, **Then** the canvas stops at release.
5. **Given** a pan released slower than the minimum glide speed, **Then** no glide starts.

---

### Edge Cases

- Scroll sources without a stop signal (a mouse wheel or a device that sends no "fingers lifted" event): the sequence
  ends after a short idle time; wheels without Super keep their Feature 002 behavior (delivered to windows, Super +
  wheel zooms) and never glide.
- Kinetic scrolling generated by a client or by the host after finger lift is not a gesture HyprVX starts; HyprVX
  momentum applies only to sequences it consumed.
- A gesture begins over a layer surface (bar, launcher, notification): it belongs to that surface and never moves the
  camera (Constitution VII).
- A gesture begins on a monitor that is not in Drift, or Drift is turned on or off mid-gesture: the gesture keeps the
  meaning it had at its start; nothing is committed to a monitor that left Drift.
- Pinch scale reaching the zoom limit: zoom clamps and the anchor stays fixed; scale changes beyond the limit have no
  effect until the pinch reverses.
- Pinch combined with finger movement (rotation or translation): only scale is used; rotation is ignored; the anchor
  is the pointer position at the start of the pinch.
- Fullscreen window on the Drift monitor: Feature 002 renders it at identity; trackpad pans and pinches are not
  consumed while it is fullscreen, so the application gets them.
- A 3-finger gesture while a Super + drag (pan or move) is in progress: the drag keeps priority; the gesture is
  consumed and ignored.
- Invalid values from a device (non-finite deltas or scale ≤ 0): ignored, camera unchanged, one log line.
- With the user's real setting "three-finger drag" enabled, the input stack is expected to turn 3-finger motion into
  a pointer drag and send no 3-finger swipe (open question U4): 3-finger swipe pan is then unavailable and 2-finger
  pan is the supported path; 3-finger drag must never be turned into a window action either.

## Requirements *(mandatory)*

### Functional Requirements

**Scope of consumption**

- **FR-001**: The system MUST NOT consume, delay or alter any scroll or gesture event on a monitor that is not in
  Drift Mode; with Drift off behavior and captures stay identical to stock (Constitution III).
- **FR-002**: The decision of what a scroll sequence or gesture does (pan or zoom the canvas, deliver to a window,
  deliver to a layer surface, or leave to the compositor) MUST be made when it begins, from the surface under the
  pointer and the modifier state at that moment, and MUST be kept until it ends or is cancelled.
- **FR-003**: Sequences that begin over a layer surface or over a fullscreen window MUST be delivered as in stock and
  never move the camera.

**Pan**

- **FR-004**: In Drift, a two-finger (finger-source) scroll sequence that begins over empty canvas MUST pan the
  camera so that content moves by the scrolled screen distance, following the user's scroll direction setting
  (natural scrolling moves content with the fingers).
- **FR-005**: In Drift, a two-finger scroll sequence that begins with Super held MUST pan the canvas wherever it
  begins and MUST NOT be delivered to any window.
- **FR-006**: In Drift, a two-finger scroll sequence that begins over a window without Super MUST be delivered to
  that window exactly as in Normal Mode.
- **FR-007**: In Drift, a 3-finger swipe MUST pan the canvas like a two-finger pan, if the input stack delivers
  3-finger swipes (U4).

**Zoom**

- **FR-008**: In Drift, a pinch that begins over empty canvas, or anywhere with Super held, or any 3-finger pinch,
  MUST set zoom = start zoom × pinch scale, clamped to [0.1, 2.0], anchored so that the world point under the
  pointer at the start stays under the pointer.
- **FR-009**: In Drift, a 2-finger pinch that begins over a window without Super MUST be delivered to that window.

**Arbitration and cancel**

- **FR-010**: On a monitor in Drift, 3-finger swipes and pinches MUST NOT trigger any compositor gesture binding
  (close, fullscreen, float, special workspace or any other 3-finger action); they are consumed by Drift navigation.
- **FR-011**: 4-finger horizontal swipes MUST keep their configured workspace behavior in Drift; the camera of each
  workspace is kept as in Feature 002.
- **FR-012**: A cancelled gesture MUST restore the camera to its state at the start of that gesture and MUST NOT
  start a glide.
- **FR-013**: Leaving Drift, panic, a workspace change or loss of the monitor during a gesture or glide MUST end it
  cleanly: no camera change is applied after the monitor leaves Drift, no input stays captured, and the next input
  behaves normally.
- **FR-014**: While a Super + drag (pan or move) of Feature 002 is in progress, trackpad gestures MUST NOT change the
  camera or the dragged window.

**Momentum**

- **FR-015**: When a consumed pan (two-finger scroll or 3-finger swipe) ends at a speed above a minimum, the camera
  MUST keep panning in the release direction with speed decaying continuously to zero; the total glide distance MUST
  follow from the release speed and the configured friction by a closed-form rule (deterministic for given inputs).
- **FR-016**: A glide MUST stop within one frame on any new scroll, gesture, button, key, mode change, panic or
  workspace change, and MUST NOT produce frames after it stops (Constitution VI).
- **FR-017**: A user setting in [0, 1] MUST control glide strength; 0 disables gliding. Zoom gestures do not glide.

**Safety and tooling**

- **FR-018**: All camera changes from gestures MUST go through the same camera operations as the Feature 002
  dispatchers (pan by screen distance, anchor zoom); no new camera equation (Constitution IV).
- **FR-019**: Automated tests MUST drive scrolls and gestures only into the verified nested session (nested-only
  input; never uinput or ydotool) and MUST include a nested config that reproduces the user's 3- and 4-finger
  bindings, so that arbitration is tested against the real conflict.
- **FR-020**: Any synthetic gesture input used for tests MUST exist only in dev builds and MUST be refused by
  non-dev builds, like the dev control of Features 001 and 002.
- **FR-021**: Physical-hardware validation (L6) MUST be done by a human with the real trackpad following a written
  checklist; [NEEDS CLARIFICATION: how to cover pinch and 3-finger gestures in L6, given that a nested session cannot
  receive them and the host acts on 3- and 4-finger gestures — see Assumptions].
- **FR-022**: Feature 001 and Feature 002 suites MUST still pass with the feature built in (both delivery forms).

### Key Entities

- **Gesture session**: one scroll sequence or gesture from begin to end: kind (scroll, swipe, pinch), finger count,
  target chosen at begin (canvas pan, canvas zoom, window, layer, compositor), monitor, camera at begin, anchor point,
  accumulated delta or scale, and whether it was cancelled.
- **Glide**: release velocity, start time, friction, the camera at release; ends at rest or on interruption.
- **Trackpad settings**: glide strength (0–1), minimum glide speed, pan speed factor; defaults chosen so that 2-finger
  pan moves content exactly with the fingers.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In nested tests, two-finger pans over empty canvas move every visible window's drawn box by the scrolled
  distance within 1 px at scales 1 and 1.25, in both delivery forms.
- **SC-002**: In nested tests, after any pinch the world point that was under the pointer at the start is still under
  it within 1 px, and zoom equals start zoom × scale within the range [0.1, 2.0].
- **SC-003**: Across all 3-finger directions and pinches in Drift (at least 20 gestures per run), 0 windows close,
  change fullscreen or floating state, and 0 special-workspace toggles happen; the Drift-off control performs every
  bound action.
- **SC-004**: Scrolls and pinches over windows without Super reach the window with the same values as in Normal Mode
  in 100% of test sequences.
- **SC-005**: Glide distance matches the closed-form rule within 2 px; any interrupting input stops the glide within
  one frame; no frame is produced after the glide ends (measured by frame events in the nested session).
- **SC-006**: With Drift off, captures stay pixel-identical to stock and no scroll or gesture is consumed.
- **SC-007**: A human completes the L6 checklist on the real trackpad and signs it; every item is PASS, FAIL, BLOCKED
  or NOT RUN with a reason, and none is counted as PASS without being performed.
- **SC-008**: Feature 001 and Feature 002 suites pass with the feature built in, in both delivery forms.

## Assumptions

- Feature 002's Drift Mode, policy plugin, delivery forms (P and S, DV3-16), camera per workspace, test tooling and
  evidence workflow are the baseline; its contracts are extended, not replaced.
- The V1 compositor-agnostic core already contains camera motion with damping (`CameraMotion`); it is reused where
  suitable (Constitution VIII), and any replacement needs source-level evidence.
- Gesture events reach the policy through Hyprland's own gesture and scroll event path (no host-level input
  access); the design target is HyprVX inside the user's compositor, which receives every gesture.
- **Nested validation limits (verified 2026-10-12)**: a nested session receives two-finger scroll with source and
  stop, but no pinch, swipe or hold from the physical trackpad; 3- and 4-finger gestures made over the nested window
  are executed by the host first. During L6 in a nested session the human must not perform 3- or 4-finger gestures
  over the nested window (host actions include close window and suspend-then-hibernate).
- **Recommended answer to the FR-021 clarification**: L6 covers in a nested session everything that can physically
  reach it (two-finger pan over canvas and windows, Super + two-finger pan, glide and interruption, Drift off = stock
  scrolling); physical pinch and 3-finger items are recorded BLOCKED (environment) with the automated nested evidence
  attached, and are carried forward to the first approved session with HyprVX in a compositor that owns the trackpad
  (Phase 9 production candidate, or an earlier hardware session the user approves separately). The Phase 4 gate is
  then signed with those items open, as Feature 001 kept V-08 PARTIAL.
- U4 (does the input stack still deliver 3-finger swipes with three-finger drag enabled) cannot be answered in a
  nested session; it is answered in the same hardware session as above. Until then 3-finger swipe pan is implemented
  and tested with synthetic gestures only, and 2-finger pan is the supported physical path.
- Gesture settings live for the compositor session (config values); persistence is later work.
- Performance of gesture handling and glide is measured, not budgeted, in this feature (Phase 7 sets budgets).

## Out of Scope

Snap, clusters, pin, directional navigation, zoom-to-fit and bookmarks (Phase 5); zoom momentum; touchscreen input;
hold gestures; changing the user's host Hyprland, Caelestia or input configuration (including three-finger drag);
loading HyprVX into the host compositor or running a compositor outside the nested harness (needs separate approval);
Caelestia compatibility layer (Phase 6); performance optimisation beyond existing budgets (Phase 7); unified world
across workspaces (Milestone U); multi-monitor and HDR (Phase 8); packaging and installation (Phase 9).
