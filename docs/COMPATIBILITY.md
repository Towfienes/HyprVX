# Compatibility

## Initial development baseline

Hyprland:
0.56.2

Platform:
Wayland on CachyOS

Shell:
Caelestia and Quickshell environment

## Native plugin warning

Hyprland native plugin compatibility may be version-sensitive.

Never assume a plugin compiled for one Hyprland version works safely on another.

## Compatibility strategy

Before native plugin release:
- record supported Hyprland versions
- build in CI against supported versions where feasible
- fail safely on unsupported ABI or version
- document exact install and rollback process

## XWayland

XWayland applications require dedicated integration tests.

## Other shell environments

Core engine design should avoid a hard Caelestia dependency.

## Phase 3 nested plugin prototype

The plugin target builds for installed Hyprland 0.56.2 headers at commit
`efb50993780079460b0cbed1363e2166a2de1d9f`. Initialization checks the
compiled and running commit plus the nested instance contract. Hyprland's
plugin API version check runs before initialization. Unsupported builds stay
inactive; production installation is unsupported. The overlay uses Wayland
core, wlr layer shell, and optional pointer gestures. The wlr protocol XML
license and pinned source are recorded in `docs/THIRD_PARTY.md`.
