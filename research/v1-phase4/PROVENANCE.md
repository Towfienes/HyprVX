# Provenance — V1 Phase 4 draft patch

- Source: `/home/towfienes/Projects/HyprVX/platform/patches/hyprland-v0.56.2-spatial.patch`
  (UNTRACKED in V1 `dev`; the original was copied, not moved — it remains in V1).
- Size: 24471 bytes, 437 lines; mtime 2026-09-27 21:23 +0700.
- SHA-256: `542ea17987d014383c84385027414ffe4f64457ff5def8ef881ffdea98709603` (see `SHA256SUMS`).
- Companion: V1 `docs/NATIVE_PHASE4.md` is untracked and **empty** (0 bytes, SHA-256 of empty input `e3b0c442…b855`); not copied.
- Target base: Hyprland v0.56.2 (`efb50993780079460b0cbed1363e2166a2de1d9f`).

## Status (audit 2026-10-10, source-only)

**INERT / never built / never exercised.**
- Adds `CMonitor::m_spatialCamera {enabled, pan, zoom}` but nothing sets it (no dispatcher, IPC or plugin path).
- The V1 nested harness runs the unpatched `/usr/bin/Hyprland`.
- Known defects: `onMouseWheel` warps with a logical (untransformed) position; the spatial render hint replaces
  rather than composes with the geometry modifier; inconsistent monitor selection between `windowAt` and
  `windowSurfaceAt`; no coverage of touch/tablet, pointer constraints, xdg-popup constraint box, decoration input,
  `damageBox/damageRegion`, special workspaces, blur, screencopy; the surface clip (RC1) and pass-simplify (RC2)
  bugs documented in `docs/v3/HYPRVX_V3_MASTER_PLAN.md` are not addressed.
- Adding a `CMonitor` member changes struct layout without changing `GIT_COMMIT_HASH` (ABI hazard).

Use as **reference only** (equations are consistent with V2 and with the V3 transform contract).
