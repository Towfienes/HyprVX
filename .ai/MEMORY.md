# HyprVX Project Memory

## Mission

HyprVX is a working-name project for an infinite spatial desktop layer for Hyprland.

The project combines:
- vxwm style spatial desktop philosophy
- modern trackpad-first interaction inspired by projects such as DriftWM
- Hyprland ecosystem and compositor capabilities
- optional Caelestia Shell integration

The goal is not to replace Hyprland.

The goal is to let users place normal windows and window groups on a continuous 2D world while preserving a fast Hyprland workflow.

## Product principles

1. Performance before visual effects.
2. The desktop must remain usable when HyprVX is disabled.
3. Idle HyprVX should consume almost no CPU.
4. No continuous redraw when nothing is changing.
5. Input latency matters more than decorative animation.
6. Free windows remain normal and useful.
7. Organization is optional, never forced.
8. Trackpad, mouse and keyboard must all feel native.
9. Multi-monitor support is a core requirement.
10. Caelestia enhances HyprVX but is not required for the engine.

## Spatial model

The world contains:
- free windows
- islands
- zones
- per-output cameras

A free window exists directly in world space.

An island is a movable group containing windows with local coordinates.

A zone is a named camera location or spatial region. A zone is not a container.

## Safety model

Development order:

world simulator
-> unit tests
-> nested Hyprland
-> integration tests
-> performance tests
-> production Hyprland

Experimental native plugins must not be tested first on the production compositor.

## Current development host baseline

Initial host:
- CachyOS
- Wayland
- Hyprland 0.56.2
- laptop display 2880x1800 at 120 Hz during initial baseline
- Btrfs root and home subvolumes

Initial real-workload idle sample:
- average CPU used approximately 3.75 percent
- average CPU idle approximately 96.25 percent
- approximately 9.76 GiB RAM used during active development workload

These numbers are comparison baselines, not project performance promises.
