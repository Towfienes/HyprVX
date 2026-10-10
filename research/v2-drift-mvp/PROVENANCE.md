# Provenance — V2 "drift-mvp" Hyprland patch series

- Source: `/home/towfienes/Projects/HyprVX-v2/patches/0001-hyprvx-v2-spatial-drift-mode.patch`
  (tracked in V2 commit `31086d7`, branch `v2/drift-mvp`). Copied read-only; V2 was not modified.
- Size: 33852 bytes; mtime 2026-10-07 18:10 +0700.
- SHA-256: `bc05d46c42b0bc61c4687d1ff1b93dd5b0690435bca20643f70b292e61e88581` (see `SHA256SUMS`).
- It is a 3-commit `format-patch` series on upstream `efb50993780079460b0cbed1363e2166a2de1d9f` (v0.56.2):
  - `d211555` feat(v2): add reversible drift mode state and spatial camera dispatchers
  - `46ed3a5` feat(v2): transform application rendering in spatial world
  - `14d1232` feat(v2): inverse-transform pointer hit testing and drag controller
    (`14d12322ee0fc956f40c02284a5e2714b0ee1908`)
- Verified 2026-10-10: `git -C HyprVX-v2/hyprland format-patch --stdout efb5099..HEAD` is byte-identical to this file.
  The nested repo `HyprVX-v2/hyprland/` is untracked by the V2 root repo but has a clean working tree.

## Status

**NOT a foundation. Reference only.**
- The equations are mutually consistent (render / inverse / damage / drag).
- Visual failure root causes (confirmed by source reading, to be confirmed visually in Sprint 1):
  - RC1: `drawSurface` scissors to damage ∩ the *untransformed* `windowBox` (`src/render/pass/ElementRenderer.cpp` draw sites).
  - RC2: `CRenderPass::simplify` culls/subtracts using untransformed `boundingBox()`/`opaqueRegion()`.
- The "14/14 PASS" in `docs/DRIFT_MVP_REPORT.md` contains no pixel or pointer assertions; criteria 3 and 10 pass
  unconditionally; criterion 9 reads state captured before DRIFT was enabled.
- `scripts/build.sh` forces `GIT_COMMIT_HASH=efb5099` and `GIT_DIRTY=clean`, so the patched binary misreports provenance.
