# HyprVX Next Task

## Phase 1B

Build the first interactive spatial simulator without connecting to Hyprland.

## Goals

Implement:

- camera velocity
- frame-rate-independent camera movement
- acceleration
- damping
- target-based camera flight
- zoom limits
- cursor-centered zoom behavior
- viewport representation
- world object rendering in simulator
- free window visualization
- island visualization
- input-independent semantic camera actions

## Input simulation

The simulator may emulate:

- keyboard camera movement
- mouse-style drag pan
- zoom input

Do not read real Hyprland input yet.

## Performance rule

Physics updates must use delta time.

Do not implement a permanent busy loop.

Interactive rendering may run only while the simulator is active.

## Pass criteria

- deterministic physics tests
- equivalent motion at different simulated frame rates
- no unstable overshoot
- camera can move in all directions
- camera can smoothly travel to a target
- zoom anchor remains stable
- free windows and islands map correctly through camera transforms
- headless core remains independent of Hyprland

## Not yet

Do not implement:

- native Hyprland plugin
- production trackpad capture
- Caelestia UI
- Island join/detach UX
- persistence daemon
- true compositor rendering zoom
