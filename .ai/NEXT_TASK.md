# HyprVX Next Task

## V3 status (2026-10-10)

The architecture is approved. The next step is **Sprint 1** (master plan §I; `specs/001-native-spatial-camera/`).
It requires explicit user approval before any code or build work.

GitHub handoff: select `Towfienes/HyprVX`, branch `v3/spec-foundation`, in Claude Code Cloud and read
`CLAUDE.md` first. A fresh checkout can select the feature with `SPECIFY_FEATURE_DIRECTORY=specs/001-native-spatial-camera`.

## Sprint 1 goal

Prove that a real window surface is scaled and panned pixel-correctly in a nested, patched Hyprland while the
identity camera stays pixel-identical to stock-from-source.

## Order

1. On approval, create branch `v3/001-native-spatial-camera` from `v3/spec-foundation`; run `/speckit-tasks`
   to generate `tasks.md`; run `/speckit-analyze`.
2. Fresh `build-v3/` for the V1 core with optional SDL/plugin; GTest.
3. `scripts/fetch-hyprland.sh`, `apply-patches.sh`, `build-hyprland.sh` (stock + V2-as-is + V3), with `-j4`
   and `MemoryMax=6G`.
4. Test infrastructure first: testclient, vpointer, analyzer, hardened nested harness.
5. Confirm U1 visually with V2-as-is. Then patches 0001–0005 and T-ID / T-Z* / T-PAN / T-STALE / T-OFF.

## Stop conditions

- T-ID not pixel-identical.
- The build would require system package changes.
- The harness cannot verify the nested PID.
- A seam is needed outside the mapped list.
- OOM or swap thrash.

## Constraints

- Constitution `.specify/memory/constitution.md` (I–X) governs.
- Never touch the host compositor, Caelestia or production config.
- Never use ydotool/uinput.
- Do not commit unless the user asks.
