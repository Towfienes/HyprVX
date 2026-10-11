# Verification Checklist: Feature 002 — Multi-Window Drift MVP (Option A)

Status legend: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. **Every PASS needs an artifact path** in the
retained archive `~/.local/share/hyprvx/evidence/claude/<run-id>/` whose `SHA256SUMS` verified at import
(`contracts/test-tooling.md` 1.4, base 1.3 §8). A dispatcher `ok` or `hyprctl hyprvx state` is never an artifact.
Native tests take `HVX_FORM=P|S`; until Phase 9 they run in form P (or with the dev control on the patched build).

| ID | Level | Check | Status | Artifact |
|---|---|---|---|---|
| V-01 | L2 | Transform 1.2 unit tests: invariants 8–10 (seeded, ≥ 10⁴ cases) plus all 1.1 invariants | NOT RUN | — |
| V-02 | L2 | `DriftModel` unit tests (transitions, idempotence, cameras, offsets, drags, spawn, mechanismOffset) | NOT RUN | — |
| V-03 | L4 | Identity refusal of the new tools and harness commands (vkeyboard, `xdisplay`, `plugin load`) | NOT RUN | — |
| V-04 | L5 | T-MULTI: three offset windows ± 2 px at z ∈ {0.25, 0.5, 1, 2}, scales 1 and 1.25; top layer unchanged | NOT RUN | — |
| V-05 | L5b | T-INPUT-MW: pointer local ± 1 on offset windows, popup, subsurface; click focus; empty canvas silent | NOT RUN | — |
| V-06 | L5 | T-STALE-MW: 50 random camera/offset states, partial = full damage | NOT RUN | — |
| V-07 | L5/L5b | T-X11: X11 window and override-redirect menu with offset, bbox ± 2 px, pointer ± 1 | NOT RUN | — |
| V-08 | L5 | T-DRIFT-OFF ×3: capture = stock, `hyprctl clients` unchanged, no `movewindow*`, re-entry restores | NOT RUN | — |
| V-09 | L5 | T-PANIC-DRIFT 10/10: first capture after panic = stock | NOT RUN | — |
| V-10 | L5 | T-IDEMP: on/off twice, panic while OFF, special workspace keeps OFF | NOT RUN | — |
| V-11 | L5 | T-MONITOR: output removed in Drift → alive, OFF; re-add stays OFF; plugin unload acts as panic | NOT RUN | — |
| V-12 | L5 | T-ANCHOR: 20 zoom steps (dispatcher, Super + wheel), cumulative drift ≤ 1 px | NOT RUN | — |
| V-13 | L5 | T-PAN: dispatcher pan and Super + canvas drag ± 1 px; drag over top layer is not a pan | NOT RUN | — |
| V-14 | L5 | T-WS-CAMERA: per-workspace cameras, recreated id gets its camera | NOT RUN | — |
| V-15 | L5 | T-WS-ANIM: switch in Drift shows no intermediate frame; control animated with Drift OFF | NOT RUN | — |
| V-16 | L5b | T-KEYS: key taps reach only the clicked offset window | NOT RUN | — |
| V-17 | L5b | T-SCROLL: scroll reaches the window; Super + scroll zooms and the client gets nothing | NOT RUN | — |
| V-18 | L5 | T-FULLSCREEN: fullscreen in Drift = stock fullscreen capture; leaving restores the Drift view | NOT RUN | — |
| V-19 | L5 | T-MOVE: drag d at z → bbox moves d ± 1, O by d/z, `L` unchanged, no `movewindow*` | NOT RUN | — |
| V-20 | L5 | T-RESIZE: floating border drag / resize bind by worldDelta ± 1; tiled blocked, one log line | NOT RUN | — |
| V-21 | L5 | T-DRAG-ABORT: panic, off, client quit, workspace change during a drag; no stuck grab | NOT RUN | — |
| V-22 | L5 | T-SPAWN: mapped in Drift centred ± 2 px; mapped when OFF as stock | NOT RUN | — |
| V-23 | L5 | Form P/S parity: every Feature 002 capture byte-identical between forms, scales 1 and 1.25 | NOT RUN | — |
| V-24 | L5 | Feature 001 regression (Sprint 1 + Phase 2 suites) with the plugin loaded and Drift OFF, both forms | NOT RUN | — |
| V-25 | — | T-SCENARIO image set, both forms: human review and signature (SC-008) | NOT RUN | — |
| V-26 | L4 | Host-safety audit of every Feature 002 nested session | NOT RUN | — |
| V-27 | L1 | Reproducibility: clean-room rebuild (patched + both plugin forms), same results | NOT RUN | — |
| V-28 | L1 | `scripts/check-single-transform.sh` PASS on 0001–0013 and the policy | NOT RUN | — |
