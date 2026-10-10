# HyprVX

HyprVX is an experimental infinite spatial desktop layer for Hyprland.

Working goals:
- continuous 2D world
- free windows
- window islands
- trackpad, mouse and keyboard navigation
- multi-monitor spatial cameras
- optional Caelestia integration
- aggressive performance and battery discipline

## Status

V3 architecture and specification foundation, built on the existing core and nested prototype.
See [the V3 master plan](docs/v3/HYPRVX_V3_MASTER_PLAN.md) and [current state](.ai/CURRENT_STATE.md).

No production Hyprland plugin is installed yet.

## Claude Code Cloud

GitHub repository: `Towfienes/HyprVX`. Default branch: `v3/spec-foundation`.
The local folder can stay named `HyprVX-v3`; the GitHub repository name is independent.

Read [CLAUDE.md](CLAUDE.md) and [AGENTS.md](AGENTS.md) before working.
The active feature is `specs/001-native-spatial-camera/`; Sprint 1 still requires user approval.
The Spec Kit skills are included in `.claude/skills/`.
For a fresh checkout, select the feature explicitly when checking its prerequisites:

```bash
SPECIFY_FEATURE_DIRECTORY=specs/001-native-spatial-camera bash .specify/scripts/bash/check-prerequisites.sh --json --paths-only
```

Compositor and physical trackpad verification requires the documented nested/hardware environment.

## Development rule

Simulator first.
Nested Hyprland second.
Production compositor last.

See:
- AGENTS.md
- .ai/MEMORY.md
- docs/DESIGN_FREEZE.md
- docs/ARCHITECTURE.md
