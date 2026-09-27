# Third-party Reference Audit

HyprVX is inspired by existing spatial desktop and Wayland projects.

Before copying or adapting source code, record:
- repository
- exact commit
- license
- copied or adapted files
- attribution requirements
- compatibility with the eventual HyprVX license

Projects to audit:
- vxwm
- DriftWM
- hyprland-canvas
- CanvasWM
- Hyprland
- Caelestia
- Quickshell

Until an entry is audited, treat the project as a conceptual reference only.

Do not copy source code based only on assumptions about its license.

## wlr-layer-shell protocol definition (Phase 3)

- Repository: https://github.com/swaywm/wlroots
- Audited revision: `0855cdacb2eeeff35849e2e9c4db0aa996d78d10`
- File: `protocol/wlr-layer-shell-unstable-v1.xml`
- License: permissive X/MIT style permission notice embedded in the XML;
  copyright © 2017 Drew DeVault. It permits use, copying, modification,
  distribution, and sale with the copyright and permission notice retained.
- Use: vendored protocol XML only; `wayland-scanner` generates client stubs at
  build time. The complete original notice stays in the vendored XML.
- Compatibility: permissive with the project's eventual license provided the
  attribution and permission text remain with distributed copies.
