# HyprVX Next Task

## V3 status (2026-10-12, Claude-only since DV3-15)

`specs/001-native-spatial-camera/tasks.md` was approved at `966e6b6`. Claude Code is the only implementation agent
(DV3-15); the Codex lane and work orders are retired and the Codex clone is a read-only historical reference.

- Sprint 1 T001–T052 are done with archived evidence (`~/.local/share/hyprvx/evidence/claude/<run-id>/`, every
  repo index verified with `sha256sum -c`). Patch series 0001–0005, patchset `47b6baa12904`.
- Gates: GATE-A PASS (V-23), GATE-N PASS with H (V-24; RC1 detected, RC2 not reproduced, V-08 PARTIAL), T-ID PASS,
  suite 16/16 at scales 1 and 1.25 three times (`…-2bfe8a4-sprint1`, R1 `…-03c469e-sprint1`, `…-197511e-sprint1`),
  V-04 two clean rebuilds with byte-identical captures. T045 cost measured (informational).
- **G1 approved by the user on 2026-10-11 (T053 ticked).** Phase 2 T054–T069 are done (patches 0006–0009, US3
  8/8, delivery spike, Phase 2 R1); see `.ai/CURRENT_STATE.md` "Phase 2 status at the Feature 001 gate".
- **Feature 001 gate T071 approved by the user on 2026-10-11** (T070 V-20 Phase 2 PASS; delivery DV3-16: plugin on
  stock Hyprland, spike not production-ready). All of T001–T071 are ticked.
- Merge into `v3/spec-foundation` done (`6931c04`); U0 spike done (`docs/v3/U0_FEASIBILITY_REPORT.md`).
- Feature 002 started (user approval 2026-10-11) on branch `v3/002-multi-window-drift-mvp`: spec, plan, research,
  data model, contracts (drift-control 1.0, mechanism-api 1.0, transform 1.2, test-tooling 1.4), quickstart,
  checklists and `tasks.md` (T001–T062) are written; `/speckit-analyze` found 1 CRITICAL (license record of copied
  functions) and 3 MEDIUM gaps, all fixed in `tasks.md` (`8f1ac97`).
- `tasks.md` approved by the user (2026-10-11); `/speckit-implement` ran T001–T061 (Claude only). Final code `420efff`.
- **Feature 002 gate T062 approved by the user on 2026-10-12** (V-25 signed). Merged into `v3/spec-foundation`
  (`dd4f9b9`, post-merge checks PASS); see `.ai/CURRENT_STATE.md` "Feature 002 merged". No push.
- **Feature 003 `specs/003-trackpad-native-ux/` (Phase 4, trackpad UX)**: spec, plan, contracts and `tasks.md`
  (T001–T048) written; `tasks.md` approved by the user on 2026-10-12; `/speckit-implement` in progress (Claude), Claude only
  (the Antigravity trial DV3-17 was ended before WO-01 started). Next gate: T048 (Feature 003 gate, incl. the
  L6 session with the user at T045).
- Antigravity experiment: ended by the user on 2026-10-12 before any work; Claude Code is the only agent for all of
  V3 (Features 003–009). The clone `HyprVX-v3-agy` is kept untouched as a reference.
- V-20 / SC-005: PASS, signed by the user (Towfienes) on 2026-10-11 in `checklists/verification.md`.

## Sprint 1 goal

Prove that a real window surface is scaled and panned pixel-correctly in a nested, patched Hyprland while the
identity camera stays pixel-identical to stock-from-source.

## Stop conditions

- T-ID not pixel-identical.
- GATE-A fails (stock not deterministic): fix the harness, never loosen T-ID.
- The build would require system package changes.
- The harness cannot verify the nested PID.
- A seam is needed outside the mapped list.
- OOM or swap thrash.

## Constraints

- Constitution `.specify/memory/constitution.md` (I–X) governs.
- Never touch the host compositor, Caelestia or production config.
- Never use ydotool/uinput.
- Resource rules (master plan §O5): heavy lock, MemAvailable thresholds, PSI watchdog, `MemoryMax=6G`, `-j4`.
- Commits: authorized on `v3/001-*` branches only (DV3-13). No push, no destructive cleanup, no merge into
  `dev`, `main` or `v3/spec-foundation` without approval.
