# Verification Checklist: Feature 004 — DriftWM Window Behavior

Status legend: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. **Every PASS needs an artifact path** in the
retained archive `~/.local/share/hyprvx/evidence/claude/<run-id>/` whose `SHA256SUMS` verified at import
(`contracts/test-tooling.md` 1.6). A dispatcher `ok` or `hyprctl hyprvx state` is never an artifact. Native tests take
`HVX_FORM=P|S`; until Phase 7 they run in form P.

| ID | Level | Check | Status | Artifact |
|---|---|---|---|---|
| V-01 | L2 | Unit: `WindowBehavior` (contract window-behavior 1.0) and `DriftModel` group drag, two seeds, mutation check | NOT RUN | — |
| V-02 | L1/L4 | Identity refusal incl. the new vkeyboard keys outside a verified session | NOT RUN | — |
| V-03 | L5 | T-SNAP | NOT RUN | — |
| V-04 | L2/L5 | T-SNAP-OSC (frame-by-frame, SC-002) | NOT RUN | — |
| V-05 | L5 | T-GROUP | NOT RUN | — |
| V-06 | L5 | T-NAV | NOT RUN | — |
| V-07 | L5 | T-WB-OFF | NOT RUN | — |
| V-08 | L5 | T-PIN | NOT RUN | — |
| V-09 | L5 | P/S parity (`compare_forms.py`) | NOT RUN | — |
| V-10 | L5 | Feature 003 regression on the new prefix (both forms) | NOT RUN | — |
| V-11 | L5 | Feature 002 regression on the new prefix (both forms) | NOT RUN | — |
| V-12 | L5 | Feature 001 regression (Sprint 1, Phase 2; both forms) | NOT RUN | — |
| V-13 | R1 | Clean-room rebuild + suite + reproducibility (two clean builds) | NOT RUN | — |
| V-14 | L4 | Host-safety audit since the feature start | NOT RUN | — |
| V-15 | L1 | Single-transform lint incl. patch 0014 and `WindowBehavior` | NOT RUN | — |
| V-16 | L4 | GATE-A on the new patched prefix (stock baseline unchanged) | NOT RUN | — |
| V-17 | L4 | Plugin inert (both forms) | NOT RUN | — |
