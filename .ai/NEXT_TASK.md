# HyprVX Next Task

## Phase 1B.2

Build an interactive visual simulator for the spatial world.

The simulator remains completely independent from the production Hyprland session.

## Required behavior

Display:
- viewport
- free windows
- islands
- island members
- world coordinates
- camera position
- zoom level

Interaction:
- keyboard camera pan
- mouse middle-button drag pan
- mouse wheel or modifier zoom
- smooth camera inertia
- reset/Home action
- target camera flight demonstration

## Engineering requirements

Rendering and physics must remain separated.

The visual simulator is only a frontend over libhyprvx-core.

Do not place GUI toolkit types into the core library.

Do not use a busy loop when the window is idle if the selected toolkit provides event waiting.

## Pass criteria

- smooth pan
- predictable inertia
- cursor-centered zoom
- world objects retain correct spatial positions
- islands render from member local coordinates
- camera coordinate display matches core state
- no Hyprland integration
- no modification to production Caelestia
- no compositor reload

## Not yet

Do not implement:
- Hyprland plugin
- real global trackpad capture
- island join/detach
- Caelestia minimap
- production persistence
- compositor true zoom
