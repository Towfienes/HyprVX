# HyprVX Architecture Decisions

## D001 - Hyprland remains the compositor

Status: accepted

HyprVX extends Hyprland rather than replacing it.

## D002 - Hybrid architecture

Status: accepted

Use separate layers:

- shared spatial/world logic
- daemon or service for state, persistence and control
- native Hyprland plugin for compositor hot-path capabilities when required
- Caelestia integration for optional UI

## D003 - Simulator before compositor integration

Status: accepted

World coordinates, camera transforms, island behavior and navigation must be testable without a real Hyprland session.

## D004 - Free windows are first-class

Status: accepted

Applications are not automatically forced into islands.

## D005 - Island and Zone are different concepts

Status: accepted

Island:
a group of windows.

Zone:
a named location or spatial region.

## D006 - One-level islands for V1

Status: accepted

No nested islands.

## D007 - Independent per-output cameras first

Status: accepted

All monitors observe the same world but may have different camera positions.

Linked camera behavior may be added later.

## D008 - Screen-space and world-space are separate

Status: accepted

Caelestia bar, launcher, notifications, lockscreen and other layer-shell UI remain screen-space.

Normal managed applications normally live in world-space.

## D009 - True zoom is not an MVP requirement

Status: accepted

Pan and world semantics must become stable before compositor-level true zoom.

True zoom is experimental until Hyprland render hooks and compatibility are proven stable.

## D010 - License is not frozen yet

Status: accepted

Project license will be selected after a third-party license audit.

## D011 - Read-only event-driven Hyprland runtime bridge

Status: accepted

Keep Hyprland wire parsing and socket access outside `hyprvx_core`. A semantic
adapter translates verified socket2 events into structured runtime events, and
an eventually consistent `RuntimeMirror` observes those events without mutating
`World`. The socket2 reader blocks for input; it does not poll Hyprland or issue
`hyprctl` requests.

The initial runtime policy targets one active output: the laptop output is
active alone, an external DP output takes over exclusively while connected,
and the laptop output resumes when it disconnects. Simultaneous multi-monitor
behavior is deferred. Typed monitor hotplug events remain deferred until their
payloads are verified on hardware.

## D012 - One-way runtime spatial projection

Status: accepted

`RuntimeMirror` is compositor observation state. The separate
`hyprvx_runtime_projection` integration layer reconciles it one-way into
`hyprvx_core::World`; there is no compositor control path in Phase 2C.
Projected geometry is owned by HyprVX and is never represented as geometry
reported by Hyprland. Workspace regions are spatial zones, not Islands, and
observed windows stay free windows by default. Window IDs are allocated
monotonically per projector epoch from canonical addresses and are removed on
close; workspace and placement slot assignments remain reserved for the epoch
to keep unrelated window lifecycle events from shifting existing geometry.
The primary runtime target remains one active output.

## D013 - Versioned durable spatial projection state

Status: accepted

Persisted state is HyprVX-owned spatial memory, not compositor truth. Schema v1
stores workspace regions, next workspace/placement counters, projected geometry,
the active `InitialPlacementPolicy`, WindowIds, and metadata hints. Restores
continue using that saved placement policy so later windows cannot overlap old
regions when process configuration changes. `RuntimeMirror` remains the source of truth for
which windows are currently alive. Recovery never creates World windows from
disk alone. Same-compositor restoration uses the instance signature plus
canonical address, excludes windows observed opening in the new connection,
and rejects conflicting class/workspace hints. Cross-address restoration uses
an exact, unique class/title/workspace fingerprint only; ambiguous candidates
receive fresh placements. Socket2 does not expose a stable app-instance ID, so
indistinguishable windows after missed close/open events remain a documented
limit.

State writes use a same-directory temporary file, fsync, atomic rename, and a
last-good `.bak`. Corrupt or future-version state is preserved; replacement
requires an explicit caller decision. Connection epochs are explicit in
`RuntimeBridge`, and the projector preserves dormant geometry across epoch
reset while requiring live mirror observations before restoring World entries.
No compositor control is introduced.

Unsigned 64-bit IDs and counters are serialized as canonical decimal strings
to preserve exact values in double-based JSON consumers. Persisted world
coordinates are limited to 1e12 units to bound geometry arithmetic; runtime
world coordinates remain double-based. Corruption snapshots use collision-safe
suffixes, and debounced writes have a configurable maximum dirty age.

## D014 - Semantic runtime camera navigation

Status: accepted

Physical and simulated inputs map to shared semantic camera actions in the
runtime navigation layer. `RuntimeCameraController` reuses core Camera,
CameraController motion, CameraHistory, SpatialNavigator, and ViewportFraming;
it never issues compositor focus, window, or workspace commands. HyprVX
selection is distinct from compositor focus: observed focus may seed selection
until explicit user navigation takes ownership. Direct pan interrupts a fly,
and a new explicit destination replaces the previous flight. Workspace jumps
change only the HyprVX camera. A continuous pan is one history transaction.
The single-active-output/external-exclusive policy remains the runtime target;
trackpad input can later adapt into the same semantic actions.
