# Verification Checklist: Feature 003 — Trackpad-Native Drift UX

Status legend: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. **Every PASS needs an artifact path** in the
retained archive `~/.local/share/hyprvx/evidence/claude/<run-id>/` whose `SHA256SUMS` verified at import
(`contracts/test-tooling.md` 1.5, base 1.3 §8). Only Claude's runs produce evidence (DV3-17). A dispatcher `ok`,
`hyprctl hyprvx state` or an `hvxtest` reply is never an artifact. Native tests take `HVX_FORM=P|S`; until Phase 7
they run in form P.

| ID | Level | Check | Status | Artifact |
|---|---|---|---|---|
| V-01 | L2 | `GestureModel` unit tests (`tests/unit/test_gesture_model.cpp`, contract gesture-model 1.0), two seeds | NOT RUN | — |
| V-02 | L1/L4 | Identity refusal of `fscroll`, `hvxtest` and `--interactive` (no capture) outside a verified session | NOT RUN | — |
| V-03 | L4 | GATE-A after the gesture-binding config change: stock determinism, baseline hash unchanged | NOT RUN | — |
| V-04 | L5 | T-TP-PAN | NOT RUN | — |
| V-05 | L5b | T-TP-WINDOW | NOT RUN | — |
| V-06 | L5b | T-TP-SUPER | NOT RUN | — |
| V-07 | L5b | T-TP-LOCK | NOT RUN | — |
| V-08 | L5b | T-TP-SURFACES (scroll and 3-finger parts) | NOT RUN | — |
| V-09 | L5 | T-TP-OFF (scroll, pinch, 3/4-finger parts) | NOT RUN | — |
| V-10 | L5 | T-PINCH | NOT RUN | — |
| V-11 | L5 | T-CANCEL (pinch and 3-finger swipe) | NOT RUN | — |
| V-12 | L5b | T-ARBITRATE | NOT RUN | — |
| V-13 | L5 | T-GLIDE | NOT RUN | — |
| V-14 | L5 | T-GLIDE-STOP (incl. output removal) | NOT RUN | — |
| V-15 | L5b | T-TP-MODES (incl. output removal) | NOT RUN | — |
| V-16 | L5b | T-TP-DRAG | NOT RUN | — |
| V-17 | L5 | Form P/S parity over all Feature 003 captures | NOT RUN | — |
| V-18 | L1/L4 | T-FR020: delivery builds refuse `hvxtest`; no `hvxtest` in delivery plugins | NOT RUN | — |
| V-19 | L5 | Feature 002 regression (both forms, Feature 003 plugin) | NOT RUN | — |
| V-20 | L5 | Feature 001 regression (Sprint 1 + Phase 2, both forms) | NOT RUN | — |
| V-21 | L1 | Clean-room rebuild and reproducibility (R1) | NOT RUN | — |
| V-22 | L1 | Single-transform lint incl. the new policy and core files | NOT RUN | — |
| V-23 | L4 | Host-safety audit of every Feature 003 nested session (incl. L6) | NOT RUN | — |
| V-24 | L6 | L6 physical checklist, human-signed; pinch / 3-finger / 4-finger-in-Drift / U4 items BLOCKED (environment) by design | NOT AUTOMATABLE | — |
