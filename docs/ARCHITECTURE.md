# HyprVX Architecture

## Layers

Caelestia integration
    |
    | control IPC and state events
    v
HyprVX control and state layer
    |
    | semantic actions and world state
    v
HyprVX compositor integration
    |
    v
Hyprland

## World model

World-space objects use world coordinates independent of a monitor.

A Camera maps part of the world into an output viewport.

Conceptually:

screenX = (worldX - cameraX) * zoom
screenY = (worldY - cameraY) * zoom

The mathematical model must not depend on shell commands.

## Hot path

The following eventually belong close to compositor integration:

- camera transforms
- render transforms
- frame-synchronized animation
- low-latency input

The following do not belong in the render hot path:

- JSON state persistence
- search indexing
- configuration parsing after startup
- Caelestia UI
- logging
- diagnostics

## IPC

IPC is semantic.

Good:
camera.pan
camera.goto
island.join
island.detach
zone.goto

Bad:
move window 6 by 2 pixels repeated hundreds of times per second

## Failure containment

The engine must have a mode where disabling HyprVX returns the user to normal Hyprland without rebuilding the desktop configuration.

## Read-only Hyprland runtime observation

The Phase 2B runtime bridge stays outside `hyprvx_core`:

Hyprland socket2 -> `EventParser` -> `SemanticEventAdapter` -> `RuntimeMirror`

The socket reader blocks for newline-framed events. The semantic adapter
prefers verified v2 events when a legacy equivalent is also emitted. The
mirror is an eventually consistent, read-only observation and does not mutate
`World`. Live input and capture replay use the same parser, adapter, and mirror.

The mirror stores metadata only; socket2 does not provide geometry. Addresses
are canonicalized as lowercase hexadecimal without a `0x` prefix. A bounded
recently-closed set prevents delayed metadata events from recreating closed
windows, while a real open event permits address reuse. Empty
`activewindowv2` payloads represent focus loss.

The initial output policy is single-active-output with external DP taking over
from eDP-2 while connected. Simultaneous multi-monitor behavior and typed
monitor hotplug events are deferred until verified on hardware.

## One-way spatial projection

Phase 2C adds an integration-side `SpatialProjector`:

`RuntimeMirror` -> `hyprvx_runtime_projection` -> `hyprvx_core::World`

The parser and mirror remain in the Hyprland IPC adapter. The projection target
depends on both the adapter and compositor-agnostic core; core has no runtime
or Hyprland dependency. Reconciliation only mutates the in-memory HyprVX
`World`. It never sends commands to Hyprland.

Socket2 provides no window geometry. The projector applies an explicit
HyprVX-owned initial placement policy (default 1000x700 windows, 200-unit
spacing, and 3600-unit workspace stride). Workspace names, including numeric,
named, and special names, receive stable non-overlapping regions for one
projector epoch. Windows are laid out on a per-workspace grid and remain free
windows. A workspace region is a spatial zone, not an Island.

Canonical Hyprland addresses map through a monotonic collision-checked
`WindowId` allocator. Metadata-only partials receive a stable projected record
but defer World insertion until workspace name is known. A real close removes
the World window and mapping; a later real open starts a new ID lifetime.
Workspace moves relocate projected geometry to the destination region and
return a grouped member to free-window ownership. Repeated reconciliation is
idempotent. Framing helpers use the core `ViewportFraming` logic but do not
drive a camera automatically. The one-active-output policy remains unchanged.
