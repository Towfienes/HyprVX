# Integration Log — Feature 004

Append-only. Owner: Claude (DV3-15). One row per run, integration, contract change or gate decision. Evidence rules:
`contracts/test-tooling.md` 1.6 (base 1.5/1.4/1.3 §8) — a task is ticked only after its evidence run is archived under
`~/.local/share/hyprvx/evidence/claude/<run-id>/` and `sha256sum -c SHA256SUMS` passes. Dispatcher replies,
`hyprctl hyprvx state` and `hvxtest` replies are never evidence.

## Frozen interfaces (tasks approval, 2026-10-12)

| Contract | Version | Frozen at |
|---|---|---|
| `contracts/window-behavior.md` | 1.0 | tasks approval commit `759f089` |
| `contracts/drift-control.md` | 1.2 | tasks approval commit `759f089` |
| `contracts/mechanism-api.md` | 1.1 | tasks approval commit `759f089` |
| `contracts/test-tooling.md` | 1.6 | tasks approval commit `759f089` |

## Log

| Date | Task | Agent | Branch@sha | Verify command | Result | Evidence run-id | Archive path | `sha256sum -c` | Review verdict | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 2026-10-12 | planning | claude | v3/004-drift-window-behavior@a5060da..de42c96 | `/speckit-specify`, `/speckit-plan`, `/speckit-checklist`, `/speckit-tasks`, `/speckit-analyze` | DONE | — | — | — | — | 34 tasks; first analyze: 1 HIGH (C10), 4 MEDIUM (C5, C1, C2, C4), 0 CRITICAL |
| 2026-10-12 | analyze remediation | claude | v3/004-drift-window-behavior@759f089 | `/speckit-analyze` re-run | PASS (0 CRITICAL, 0 HIGH) | — | — | — | — | C10: pin keeps the drawn top-left, pinned windows drawn at their own size; C5: each press acts once, holding does not repeat (efb5099 `InputManager.cpp:1630` emits physical key events only); C1: snap switch is a setting; C2: T-PIN popup case; C4: T-GROUP member-closed case; scope unchanged (34 tasks) |
| 2026-10-12 | approval | user | v3/004-drift-window-behavior@759f089 | — | APPROVED (conditional, met) | — | — | — | — | User approved `tasks.md` with the dispatcher + unbound Drift-only keys choice and instant camera jumps, on the condition of no CRITICAL/HIGH, MEDIUM contract/test items fixed, no architecture or scope change |
| 2026-10-12 | T001 | claude | v3/004-drift-window-behavior@759f089+wip | — (documents) | DONE | — | — | — | — | `integration-log.md` and `checklists/verification.md` (V-01..V-17, all NOT RUN) created; reviewer checklist `window-behavior.md` 0/30 ticked (its gaps were closed in the spec before approval, same as Feature 003) |
| 2026-10-12 | T002 | claude | v3/004-drift-window-behavior@759f089+wip | fresh cmake build + `ctest` | PASS (367/367) | — | — | — | — | `core/src/WindowBehavior.cpp` and `tests/unit/test_window_behavior.cpp` registered (one comment line each); the only build warning is the existing `-Wunused-result` in the unchanged `tests/unit/test_viewport_framing.cpp:131` |
