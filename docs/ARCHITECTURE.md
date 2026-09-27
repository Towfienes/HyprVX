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

## Durable projection state

The runtime integration stores versioned JSON in `~/.local/state/hyprvx/state.json`
by default; callers and tests can provide another path. Schema v1 contains
HyprVX workspace region assignments, placement counters, projected geometry,
the active placement policy, WindowIds, and metadata hints. Restores retain that
policy so future windows continue the saved grid and workspace spacing. It excludes socket buffers, descriptors,
semantic event temporaries, cameras, and animation state.

`ProjectionStateStore` writes a same-directory temporary file, fsyncs it,
atomically renames it over the canonical state, and syncs the directory. A
valid prior file becomes `.bak`. Invalid or unsupported state is not overwritten
without an explicit replacement choice. A valid backup can recover a corrupt
canonical file while the latter remains available for diagnosis. Corrupt
inputs are preserved under collision-safe `.corrupt`, `.corrupt.1`, and
subsequent names.

All persisted unsigned 64-bit IDs and counters use canonical decimal strings,
so JSON consumers with double-only number support cannot round them. The
decoder accepts only unsigned decimal strings with no leading zeros (except
`"0"`) and rejects signs, whitespace, fractions, exponents, overflow, and
trailing characters. Persisted coordinates and extents are limited to a
magnitude of `1e12` world units, an explicit storage safety bound; the live
world model itself retains double-based large and negative coordinate support.

Disk state never makes a window live. Restore candidates remain dormant until
the current `RuntimeMirror` reports the address. Same-compositor restore uses
the compositor instance signature and canonical address, rejects new-open
events and conflicting known class/workspace hints, and ignores title changes.
If that identity is unavailable, a candidate may match only when exact
class/title/workspace metadata uniquely identifies one saved and one live
window. Ambiguous candidates get fresh placement. Socket2 provides no stable
application-instance identifier; a missed close and address reuse with
indistinguishable metadata cannot be resolved perfectly.

Connection epochs in `RuntimeBridge` reset transient mirror state on EOF and
reconnect. `SpatialProjector::endEpoch` removes live windows from World while
retaining dormant layout memory; a later reconciliation restores only windows
seen in the new mirror. Reconnect is caller-driven, with no retry loop. State
saving is explicit or debounced through `ProjectionStateManager` when the
projector's spatial revision changes; title and focus churn do not schedule
writes. The debounce has a configurable maximum dirty age, so continuous
spatial changes cannot defer the save indefinitely; callers can save
immediately during graceful shutdown.
