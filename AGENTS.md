# HyprVX Agent Instructions

Every coding agent MUST read these files before modifying the project:

1. .ai/MEMORY.md
2. .ai/RULES.md
3. .ai/DECISIONS.md
4. .ai/CURRENT_STATE.md
5. .ai/NEXT_TASK.md
6. docs/DESIGN_FREEZE.md
7. docs/ARCHITECTURE.md
8. docs/PERFORMANCE.md

Core rules:

- Do not develop experimental compositor code directly on the production Hyprland session.
- Use simulator first, nested Hyprland second, production session last.
- Do not introduce polling when an event-driven source exists.
- Do not use repeated hyprctl calls as a frame loop.
- Do not modify the base Caelestia configuration destructively.
- Every risky integration must have a rollback path.
- Performance regressions block feature development.
- Free windows remain first-class.
- Islands are not workspaces.
- Zones are not islands.
- Caelestia is UI and control plane, not the canvas engine.
- Trackpad, mouse and keyboard are all first-class inputs.
- Never copy third-party source until its license has been audited and recorded.
- Update CURRENT_STATE.md and NEXT_TASK.md after meaningful milestones.

## V3 (worktree `HyprVX-v3`, branch `v3/spec-foundation`)

V3 work additionally requires reading, before any change:

1. `.specify/memory/constitution.md` (non-negotiable principles I–X)
2. `docs/v3/HYPRVX_V3_MASTER_PLAN.md`
3. The active feature in `specs/NNN-*/` (`spec.md`, `plan.md`, `research.md`, `contracts/`)

Workflow: Spec Kit skills `/speckit-specify` → `/speckit-clarify` → `/speckit-plan` → `/speckit-tasks` →
`/speckit-analyze` → `/speckit-implement`. Do not run `/speckit-tasks` or `/speckit-implement` for a feature
until the user approves it. `research/` is read-only reference material (V1 Phase 4 patch, V2 patch).
