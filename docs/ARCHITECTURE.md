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
