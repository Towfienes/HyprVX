# Feature Specification: Native Spatial Camera (proof of correct real-window pan/zoom)

**Feature Branch**: `v3/001-native-spatial-camera` (to be created on approval; specs live on `v3/spec-foundation`)
**Created**: 2026-10-10
**Status**: Draft — architecture approved; replan P1/P2 applied 2026-10-10
(`docs/v3/HYPRVX_V3_COLLABORATION_REPLAN_PROPOSAL.md`); implementation NOT yet approved
**Input**: "Prove that Hyprland can pan and zoom real application windows correctly (pixels and pointer input
agree), while Normal Mode stays stock-equivalent, in an isolated nested session." (Master plan §H Phases 0–2, §I.)

## Context

Two earlier attempts failed to show real window content transforming correctly. V1 only drew rectangles; V2
claimed "14/14 PASS" without any visual check. The source-level root causes RC1 and RC2 are documented in
`research.md` R2. This feature is the critical-path proof that every later Drift Mode feature depends on.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Normal Mode stays stock (Priority: P1)
As the desktop owner, I can run the HyprVX-enabled compositor with Drift Mode off and see exactly what stock
Hyprland shows, so adopting HyprVX never degrades my normal desktop.

**Why this priority**: Normal-mode equivalence is the precondition for ever running HyprVX daily (Constitution III).
**Independent Test**: Run the same scripted scene in a stock-from-source build and in the patched build with
identity camera, inside the nested harness; compare captures.

**Acceptance Scenarios**:
1. **Given** the identical scene (same clients, positions, config) in stock and patched nested sessions,
   **When** both outputs are captured, **Then** the images are pixel-identical.
2. **Given** the patched build with Drift off, **When** a frame is rendered, **Then** no extra render pass
   element has been added (debug counter = 0).

### User Story 2 — Real window visibly zooms and pans correctly (Priority: P1)
As the user, when the camera zooms to 0.25 or pans, a real application window is drawn at the size and position
the camera equation predicts. It is complete, with no clipping, stale rectangles or detached decorations.

**Why this priority**: This is the exact failure observed in V2.
**Independent Test**: The calibrated test client (corner markers, grid) in the nested session; a dev-only camera
control; captures analyzed automatically, plus human review of the same images.

**Acceptance Scenarios**:
1. **Given** an 800×600 test window and camera z=0.25, **When** captured, **Then** its bounding box is 200×150 ± 2 px
   at the position given by the transform contract, and all four corner markers are present.
2. **Given** z = 0.5 (800×600 window) or z = 2.0 (400×300 window, so the transformed box fits the 1280×800
   output), and pans of ±300 px, **When** captured, **Then** size and displacement match the contract within ± 2 px.
3. **Given** 50 random camera states applied in sequence, **When** each partial-damage frame is compared with a
   forced full-damage frame of the same state, **Then** they match (no stale regions).
4. **Given** any non-identity camera, **When** captured, **Then** borders and shadows enclose the transformed
   surface (no disconnected decorations), and layer-shell surfaces are unchanged.
5. **Given** a fixed non-identity camera, **When** the client updates a small region in deterministic order,
   **Then** each partial-damage frame matches a forced full-damage frame of the same client state
   (T-STALE-STATIC; a camera change always damages the whole output, so scenario 3 alone never exercises the
   partial-damage path).

### User Story 3 — Pointer input lands where the window is drawn (Priority: P1)
As the user, clicking or moving the pointer over a zoomed or panned window delivers events to that window at
the matching surface-local coordinates.

**Independent Test**: A virtual pointer (`zwlr_virtual_pointer_v1`, on the verified nested display only) moves to
screen points S; the test client logs `wl_pointer` coordinates.

**Acceptance Scenarios**:
1. **Given** z=0.25 and a pan, **When** the pointer is at S over the window, **Then** the client receives `enter`/
   `motion` with local = inverse(S) − W(surface origin) ± 1 px.
2. **Given** the pointer over empty canvas, **When** it moves, **Then** no window receives motion.
3. **Given** a click at S, **When** focus is evaluated, **Then** the window drawn under S becomes focused.
4. **Given** a popup opened by the client (Phase 2 scope), **When** zoomed, **Then** the popup is drawn
   attached to its parent and receives input at its drawn location.

### User Story 4 — Safe return to normal (Priority: P2)
As the user, I can always return to identity/Normal instantly. Invalid camera states can never trap me.

**Acceptance Scenarios**:
1. **Given** any camera, **When** `panic` is invoked, **Then** the next frame is pixel-identical to the US1 baseline.
2. **Given** an invalid camera request (NaN, z ≤ 0, z out of range), **When** applied, **Then** it is rejected
   and the camera stays valid.

### Edge Cases
- A window entirely outside the monitor's logical rectangle but inside the camera's visible world rect must be
  rendered (culling uses world visibility).
- A popup positioned outside every monitor must not crash (`CPopup::onMap` null monitor).
- Direct scanout / solitary client must be blocked while the camera is non-identity.
- Fractional output scale (e.g. 1.25) must not break the ± 2 px criteria (nested test at scale 1 and 1.25).
- Special-workspace dim and blur are out of scope for P1; they are documented if wrong (Phase 7).
- A compositor-initiated window move or resize drag (Super+LMB, border drag) while the camera is non-identity
  is blocked and logged in this feature. Moving windows in Drift changes the world offset `O(w)` and belongs to
  Feature 002; Drift must never move a window by writing its logical geometry.

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: The system MUST define exactly one camera transform (forward/inverse) shared by rendering, damage,
  culling and hit-testing (`contracts/transform-contract.md`).
- **FR-002**: With identity camera, the render path MUST add no pass elements and behave as stock.
- **FR-003**: With a non-identity camera, window surfaces, subsurfaces, popups and window decorations MUST be
  drawn transformed and clipped to their *transformed* bounds.
- **FR-004**: Layer-shell, lock-screen, IME, notification and cursor rendering MUST remain untransformed.
- **FR-005**: Damage for window-owned content MUST be mapped world→screen. A camera change MUST damage the
  whole output.
- **FR-006**: Window culling MUST use the camera's visible world rectangle.
- **FR-007**: Pointer hit-testing and surface-local coordinates MUST use the inverse transform. Monitor
  selection is by screen position.
- **FR-008**: The camera MUST be controllable only through a dev-only interface in this feature, available only
  in nested dev builds.
- **FR-009**: Invalid camera values MUST be rejected. A `panic` reset to identity MUST always be available.
- **FR-010**: Builds MUST be reproducible from upstream `efb5099` plus the ordered patch series, with truthful
  provenance.
- **FR-011**: All verification MUST run against a nested compositor whose identity is re-verified before every
  mutating command.
- **FR-012**: While the camera is non-identity, pointer drags delivered to a client surface (button held) MUST use
  `surfaceLocal` coordinates, and decoration/corner hit selection MUST use the inverse transform.
  Compositor-initiated window move/resize drags MUST be blocked and logged; they never write logical geometry.

### Key Entities
- **Camera**: per (monitor, scene) — world point at the monitor's top-left `P`, zoom `z`.
- **WindowWorldState**: per window — world offset `O` (zero in this feature).
- **DriftScene**: set of included workspaces (the active workspace only in this feature).
See `data-model.md`.

## Success Criteria *(mandatory)*
- **SC-001**: US1 pixel-identity holds in 3/3 repeated runs.
- **SC-002**: US2 geometry is within ± 2 px for z ∈ {0.25, 0.5, 2.0} (z = 2.0 with a 400×300 window) and pans of
  ±300 px, with 0 stale-frame mismatches over 50 random states and 0 mismatches over the static-camera
  partial-damage sequence (US2.5).
- **SC-003**: US3 surface-local error ≤ 1 px at ≥ 20 sampled points per camera state.
- **SC-004**: `panic` returns to the identity image within one frame in 10/10 trials.
- **SC-005**: A human reviewer confirms the captured image set shows no clipping, stair-step or detached
  decoration artifacts (NOT AUTOMATABLE; signed in `checklists/verification.md`).

## Assumptions
- The nested Hyprland (Wayland backend) renders and captures via screencopy (`grim`) faithfully enough for
  geometric checks. Physical-output effects (HDR, VRR, scanout) are verified in later phases.
- Physical trackpad behavior is out of scope (Phase 4).
- World offsets `O(w)` stay zero; multi-window, unified world and gestures belong to later features.

## Out of Scope
Gestures, momentum, snapping, clusters, pinning, unified world, multi-monitor, production deployment, and any
host-compositor interaction. Window move/resize by drag in Drift (world-offset controller) belongs to Feature 002.
