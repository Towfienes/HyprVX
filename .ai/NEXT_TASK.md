# HyprVX Next Task

## V3 status (2026-10-11, Claude-only since DV3-15)

`specs/001-native-spatial-camera/tasks.md` was approved at `966e6b6`. Claude Code is the only implementation agent
(DV3-15); the Codex lane and work orders are retired and the Codex clone is a read-only historical reference.

- Sprint 1 T001–T052 are done with archived evidence (`~/.local/share/hyprvx/evidence/claude/<run-id>/`, every
  repo index verified with `sha256sum -c`). Patch series 0001–0005, patchset `47b6baa12904`.
- Gates: GATE-A PASS (V-23), GATE-N PASS with H (V-24; RC1 detected, RC2 not reproduced, V-08 PARTIAL), T-ID PASS,
  suite 16/16 at scales 1 and 1.25 three times (`…-2bfe8a4-sprint1`, R1 `…-03c469e-sprint1`, `…-197511e-sprint1`),
  V-04 two clean rebuilds with byte-identical captures. T045 cost measured (informational).
- **Next: T053 G1.** Present the results and STOP. Phase 2 (T054–T071, including `tools/vpointer`) and U0 start only
  after the user approves G1.
- Waiting on the user: V-20 / SC-005 human review of the image set in `checklists/verification.md`.

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
