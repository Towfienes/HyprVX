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
