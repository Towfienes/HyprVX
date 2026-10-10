# Verification Checklist: Feature 001 — Native Spatial Camera

Status legend: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. **Every PASS needs an artifact path** in the
retained archive `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` whose `SHA256SUMS` verified at import
(`contracts/test-tooling.md` §8).
A dispatcher `ok` is never an artifact.

| ID | Level | Check | Status | Artifact |
|---|---|---|---|---|
| V-01 | L1 | Clean fetch verifies HEAD == efb50993… | NOT RUN | |
| V-02 | L1 | `git am` series applies cleanly; patchset-id recorded | NOT RUN | |
| V-03 | L1 | Stock and patched builds succeed from clean dirs; `build-info.json` truthful (no forced `clean`) | NOT RUN | |
| V-04 | L1 | Rebuild twice ⇒ same patchset-id, same test results | NOT RUN | |
| V-05 | L2 | V1 GTest suite in a fresh `build-v3/` | NOT RUN | |
| V-06 | L2 | CameraMath invariants 1–6 (≥ 10⁴ random cases) | NOT RUN | |
| V-07 | L4 | Harness refuses a host-like target (negative test: wrong PID / signature / runtime dir) | NOT RUN | |
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
| V-25 | L2 | V1 equivalence: `Camera` (M = 0) equals `CameraMath` forward/inverse; existing camera tests unchanged | NOT RUN | |
