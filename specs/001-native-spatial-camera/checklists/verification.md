# Verification Checklist: Feature 001 — Native Spatial Camera

Status legend: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. **Every PASS needs an artifact path** in the
retained archive `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` whose `SHA256SUMS` verified at import
(`contracts/test-tooling.md` §8).
A dispatcher `ok` is never an artifact.

| ID | Level | Check | Status | Artifact |
|---|---|---|---|---|
| V-01 | L1 | Clean fetch verifies HEAD == efb50993… | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-024500-stock-514f8d8/` (fetch.log, postflight.log: stock/v2/v3-dev at efb5099) |
| V-02 | L1 | `git am` series applies cleanly; patchset-id recorded | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-051254-patched-fbd98a9/` (apply.log: 0001–0005 applied, patchset_id 47b6baa12904) |
| V-03 | L1 | Stock and patched builds succeed from clean dirs; `build-info.json` truthful (no forced `clean`) | PASS | stock `~/.local/share/hyprvx/evidence/claude/20261011-024500-stock-514f8d8/` (GIT_DIRTY clean-upstream); patched `~/.local/share/hyprvx/evidence/claude/20261011-051254-patched-fbd98a9/` (GIT_DIRTY hyprvx-47b6baa12904); binary sha256 matches build-info.json in both; patched also rebuilt clean twice (V-04) |
| V-04 | L1 | Rebuild twice ⇒ same patchset-id, same test results | PASS | builds `~/.local/share/hyprvx/evidence/claude/20261011-064306-patched-5e28970-r1build/` and `~/.local/share/hyprvx/evidence/claude/20261011-080725-patched-0adbc02-rebuild2/` (both patchset 47b6baa12904, fresh worktree and build dir, no ccache); suites `20261011-074421-patched-03c469e-sprint1` and `20261011-090525-patched-197511e-sprint1` (16/16 each; 346/346 captured PNGs byte-identical). Binary sha256 reported as observed: 7d1c4bcb… / d0b87400… (embedded upstream commit hash and date differ) |
| V-05 | L2 | V1 GTest suite in a fresh `build-v3/` | PASS | `~/.local/share/hyprvx/evidence/codex/20261011-004633-core-72cd72a/`, `~/.local/share/hyprvx/evidence/codex/20261011-005226-tooling-7363be0/` (headless + enabled, re-verified by Claude 287/287 and 298/298); `~/.local/share/hyprvx/evidence/claude/20261011-023218-unit-729ce2d/` (297/297 with T009) |
| V-06 | L2 | CameraMath invariants 1–6 (≥ 10⁴ random cases) | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-023218-unit-729ce2d/` (10/10 for two seeds, 10^4 cases per property, 3/3 mutants killed) |
| V-07 | L4 | Harness refuses a host-like target (negative test: wrong PID / signature / runtime dir) | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-030107-tooling-6c3606f-t017/` (22/22) |
| V-08 | L5 | U1: V2-as-is shows clip/stale artifacts (confirms RC1/RC2) | PARTIAL: RC1 confirmed, RC2 not reproduced | `~/.local/share/hyprvx/evidence/claude/20261011-032522-v2-02a59d1/` (V2 identity, z 0.5/0.25/1.5, pans, fills with full-damage references) |
| V-09 | L5 | T-ID: patched identity vs stock pixel-identical (×3) | PASS | scale 1: `20261011-061806-patched-2bfe8a4-t_id`; scale 1.25: `20261011-062925-patched-2bfe8a4-t_id` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-10 | L5 | T-Z025 / T-Z05 / T-Z2 ± 2 px, 4 markers | PASS | scale 1: `20261011-061924-patched-2bfe8a4-t_zoom`; scale 1.25: `20261011-063042-patched-2bfe8a4-t_zoom` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-11 | L5 | T-PAN displacement ± 2 px | PASS | scale 1: `20261011-062038-patched-2bfe8a4-t_pan`; scale 1.25: `20261011-063208-patched-2bfe8a4-t_pan` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-12 | L5 | T-STALE 50 states, 0 mismatches | PASS | scale 1: `20261011-062157-patched-2bfe8a4-t_stale`; scale 1.25: `20261011-063349-patched-2bfe8a4-t_stale` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-12b | L5 | T-STALE-STATIC: fixed non-identity camera, deterministic client partial updates, 0 mismatches vs full damage | PASS | scale 1: `20261011-062624-patched-2bfe8a4-t_stale_static`; scale 1.25: `20261011-063854-patched-2bfe8a4-t_stale_static` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-13 | L5 | T-OFF / panic ⇒ identity image (10/10) | PASS | scale 1: `20261011-062742-patched-2bfe8a4-t_off`; scale 1.25: `20261011-064019-patched-2bfe8a4-t_off` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-14 | L5 | Layer-shell test surface unchanged under camera | PASS | scale 1: `20261011-062716-patched-2bfe8a4-t_layer`; scale 1.25: `20261011-063952-patched-2bfe8a4-t_layer` (archived under `~/.local/share/hyprvx/evidence/claude/`) |
| V-15 | L5 | Scale 1.25 repeats V-10..V-12 | PASS | suite `~/.local/share/hyprvx/evidence/claude/20261011-061805-patched-2bfe8a4-sprint1/` (16/16 at scales 1 and 1.25; patchset 47b6baa12904) |
| V-16 | L5b | Surface-local ± 1 at ≥ 20 points per state | NOT RUN | |
| V-17 | L5b | Click focuses the drawn window; empty canvas gets no motion | NOT RUN | |
| V-18 | L5 | Popup drawn attached and hit-testable when zoomed | NOT RUN | |
| V-19 | L5 | Popup mapped outside all monitors: no crash | NOT RUN | |
| V-20 | — | Human review of the image set (no clipping/stair-step/detached deco) | NOT AUTOMATABLE | reviewer + date |
| V-21 | — | Delivery spike report (hooks vs patch) written | NOT RUN | `delivery-decision.md` |
| V-22 | — | Host untouched: no host hyprctl/plugin/config change during the run | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-053517-audit-4411e95-v22/` (95 nested sessions, none with the host signature, all private runtime dirs, agent-prefix binaries only; host hypr/caelestia config unchanged) |
| V-23 | L5 | GATE-A: stock vs stock ×3 pixel-identical with the shared nested config (precondition for V-09) | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-030116-stock-401d74b/` (3 identical PNGs sha256 6d7f6bd6…; diff 0 px) |
| V-24 | L5 | GATE-N per `contracts/test-tooling.md` §6.1: setup valid (S1), camera applied (S2), analyzer PASS on stock and good fixtures (P), analyzer FAIL with expected reasons on V2-as-is (N), artifact visually confirmed (H). Any other outcome is FAIL or BLOCKED with its classification, never PASS | PASS | S1/S2/P/N `~/.local/share/hyprvx/evidence/claude/20261011-032750-v2-02a59d1/`; H `~/.local/share/hyprvx/evidence/claude/20261011-032829-v2-02a59d1-h/` (RC1 detected on v2_pan_right; RC2 not reproduced in this scene) |
| V-25 | L2 | V1 equivalence: `Camera` (M = 0) equals `CameraMath` forward/inverse; existing camera tests unchanged | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-003807-core-e94ffd2/` (ctest unchanged, 200 000-case bit-exact self-check) and `~/.local/share/hyprvx/evidence/claude/20261011-023218-unit-729ce2d/` (invariant 7 with ==) |

## T045 cost measurement (informational, NOT a gate)

Run `20261011-092815-mixed-a4e8fbf-m_cost` (`tests/native/m_cost.sh`, archived under
`~/.local/share/hyprvx/evidence/claude/`, `sha256sum -c` OK): nested compositor `utime+stime` (all threads) per window,
5 sessions per variant, stock `927f6f64…` and patched `d0b87400…` (patchset 47b6baa12904). Conditions (`conditions.json`,
master plan §E1, read only): AC power, governor and EPP `performance`, initial 39 °C, one 800×600 testclient, headless
HVX-1 1280×800@60 scale 1.

| Variant | redraw (50 client redraws, 10 s) | camera (50 dev-control sets) | idle (60 s) |
|---|---|---|---|
| stock | 130 ms [120, 150] | NOT RUN (no dev control in stock) | 10 ms [10, 20] |
| patched, identity | 120 ms [120, 130] | 30 ms [30, 40] (identity re-sets) | 20 ms [20, 20] |
| patched, non-identity (z = 0.5) | 120 ms [120, 130] | 120 ms [110, 120] (P-zoom sweep, full damage per change) | 20 ms [10, 20] |

Median [min, max]. Reading: no difference between stock and patched is visible at the 10 ms tick resolution for client
redraws or idle; each non-identity camera change costs about 2 ms of compositor CPU (full-output damage, `noSimplify`).
Limits: one tick = 10 ms, so idle values sit at the resolution limit; the camera window ran ~22 s (identity
re-verification in `scripts/nested.sh` per call), not 10 s; **frames presented NOT MEASURED** (stock 0.56.2 has no
presented-frame counter; a client frame-callback loop would itself force renders, `src/render/Renderer.cpp` "hack:
schedule frame events"; scheduler trace lines exist only with `render:new_render_scheduling`, default off).

## V-20 image set for human review (T048)

SC-005 is signed by the user (NOT AUTOMATABLE). All images are in the read-only archive
`~/.local/share/hyprvx/evidence/claude/`; verify a run with `sha256sum -c SHA256SUMS` inside its directory first.
Look for: surface clipped or cut at an edge, stale "stair-step" regions, borders detached from or missing around the
window, layer surfaces moving with the camera.

| # | What it shows | Image |
|---|---|---|
| 1 | Stock reference scene (GATE-A) | `20261011-030116-stock-401d74b/stock1.png` |
| 2 | Patched, identity camera (identical to 1) | `20261011-061806-patched-2bfe8a4-t_id/patched1.png` |
| 3 | z = 0.25, scale 1 | `20261011-061924-patched-2bfe8a4-t_zoom/z025.png` |
| 4 | z = 0.5, scale 1 | `20261011-061924-patched-2bfe8a4-t_zoom/z05.png` |
| 5 | z = 2.0 with a 400×300 window, scale 1 | `20261011-061924-patched-2bfe8a4-t_zoom/z2.png` |
| 6 | z = 0.5, scale 1.25 | `20261011-063042-patched-2bfe8a4-t_zoom/z05.png` |
| 7 | Pan +300 px at z = 0.5 | `20261011-062038-patched-2bfe8a4-t_pan/pan_z0.5_300_0.png` |
| 8 | z = 1.5 with a TOP layer (layer must stay fixed) | `20261011-062716-patched-2bfe8a4-t_layer/cam_100_50_1.5.png` |
| 9 | z = 0.25 at scale 1.25 with the layer (the case fixed by the round-out patch) | `20261011-063952-patched-2bfe8a4-t_layer/cam_-400_-300_0.25.png` |
| 10 | Partial client updates under a fixed camera (T-STALE-STATIC) | `20261011-062624-patched-2bfe8a4-t_stale_static/fill3.png` |
| 11 | A random camera before panic (T-OFF) | `20261011-062742-patched-2bfe8a4-t_off/case1-camera.png` |
| 12 | Negative control: V2-as-is, surface cut at the old window edge (RC1) | `20261011-032522-v2-02a59d1/v2_pan_right.png` |

Reviewer: ______  Date: ______  Result (PASS/FAIL + notes): ______
