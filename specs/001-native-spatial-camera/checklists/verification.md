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
| V-08 | L5 | U1: V2-as-is shows clip/stale artifacts (confirms RC1/RC2) | NOT RUN | |
| V-09 | L5 | T-ID: patched identity vs stock pixel-identical (×3) | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-031143-patched-1531f90/` (stock vs patched x3, diff 0 px; all 6 captures sha256 6d7f6bd6…) |
| V-10 | L5 | T-Z025 / T-Z05 / T-Z2 ± 2 px, 4 markers | NOT RUN | |
| V-11 | L5 | T-PAN displacement ± 2 px | NOT RUN | |
| V-12 | L5 | T-STALE 50 states, 0 mismatches | NOT RUN | |
| V-12b | L5 | T-STALE-STATIC: fixed non-identity camera, deterministic client partial updates, 0 mismatches vs full damage | NOT RUN | |
| V-13 | L5 | T-OFF / panic ⇒ identity image (10/10) | NOT RUN | |
| V-14 | L5 | Layer-shell test surface unchanged under camera | NOT RUN | |
| V-15 | L5 | Scale 1.25 repeats V-10..V-12 | NOT RUN | |
| V-16 | L5b | Surface-local ± 1 at ≥ 20 points per state | NOT RUN | |
| V-17 | L5b | Click focuses the drawn window; empty canvas gets no motion | NOT RUN | |
| V-18 | L5 | Popup drawn attached and hit-testable when zoomed | NOT RUN | |
| V-19 | L5 | Popup mapped outside all monitors: no crash | NOT RUN | |
| V-20 | — | Human review of the image set (no clipping/stair-step/detached deco) | NOT AUTOMATABLE | reviewer + date |
| V-21 | — | Delivery spike report (hooks vs patch) written | NOT RUN | `delivery-decision.md` |
| V-22 | — | Host untouched: no host hyprctl/plugin/config change during the run | NOT RUN | harness audit log |
| V-23 | L5 | GATE-A: stock vs stock ×3 pixel-identical with the shared nested config (precondition for V-09) | PASS | `~/.local/share/hyprvx/evidence/claude/20261011-030116-stock-401d74b/` (3 identical PNGs sha256 6d7f6bd6…; diff 0 px) |
| V-24 | L5 | GATE-N per `contracts/test-tooling.md` §6.1: setup valid (S1), camera applied (S2), analyzer PASS on stock and good fixtures (P), analyzer FAIL with expected reasons on V2-as-is (N), artifact visually confirmed (H). Any other outcome is FAIL or BLOCKED with its classification, never PASS | NOT RUN | |
| V-25 | L2 | V1 equivalence: `Camera` (M = 0) equals `CameraMath` forward/inverse; existing camera tests unchanged | NOT RUN | |
