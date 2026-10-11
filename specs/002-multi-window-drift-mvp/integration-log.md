# Integration Log — Feature 002

Append-only. Owner: Claude (DV3-15, the only implementation agent). One row per run, integration, contract change,
seam decision or gate decision. Evidence rules: `contracts/test-tooling.md` 1.4 (base 1.3 §8) — a task is ticked only
after its evidence run is archived under `~/.local/share/hyprvx/evidence/claude/<run-id>/` and
`sha256sum -c SHA256SUMS` passes. Dispatcher replies and `hyprctl hyprvx state` are never evidence.

## Frozen interfaces (tasks approval, 2026-10-11)

| Contract | Version | Frozen at |
|---|---|---|
| `contracts/drift-control.md` | 1.0 | `3d1e23c` |
| `contracts/mechanism-api.md` | 1.0 | `3d1e23c` |
| `contracts/transform-contract.md` | 1.2 | `3d1e23c` |
| `contracts/test-tooling.md` | 1.4 | `3d1e23c` |

## Log

| Date | Task | Agent | Branch@sha | Verify command | Result | Evidence run-id | Archive path | `sha256sum -c` | Review verdict | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 2026-10-11 | approval | user | v3/002-multi-window-drift-mvp@3d1e23c | — | APPROVED | — | — | — | — | User approved `tasks.md` (T001–T062) and authorized `/speckit-implement` up to the T062 gate; commits on this branch authorized |
| 2026-10-11 | T001 | claude | v3/002-multi-window-drift-mvp@3d1e23c | — (documents) | DONE | — | — | — | — | `integration-log.md` and `checklists/verification.md` (V-01..V-28, all NOT RUN) created |
| 2026-10-11 | T002 | claude | v3/002-multi-window-drift-mvp@3d1e23c | `git status` (renames) | DONE | — | — | — | — | `git mv platform/plugin-spike/src platform/plugin/src/mechanism`, `git mv …/gen_copies.py platform/plugin/gen_copies.py`, spike `CMakeLists.txt` moved and rewritten as `platform/plugin/CMakeLists.txt` (`HYPRVX_PLUGIN_FORM=P\|S`; form S keeps the spike's flags and sources; `HYPRVX_PLUGIN_DEV` adds the dev control to form S for nested tests only, FR-016). Feature 001 documents cite `platform/plugin-spike/` at their own commits, which stay valid. `docs/THIRD_PARTY.md` copied-functions entry updated (new path; nested-only until T052) |
| 2026-10-11 | T003 | claude | v3/002-multi-window-drift-mvp@3d1e23c+wip | `python3 -I tools/tests/test_build_scripts.py build-plugin.sh` (and build-hyprland.sh, apply-patches.sh, fetch-hyprland.sh) | PASS (5/5 incl. 1 skip; 20/20 incl. 2 skips; 7/7 incl. 3 skips; 5/5 incl. 3 skips) | — (unit test, no compositor) | — | — | — | `scripts/build-plugin.sh --form P\|S [--dev] [--dry-run]` replaces `scripts/build-plugin-spike.sh` (removed). Output `~/.cache/hyprvx/claude/plugin/<form>[-dev]/`; `--dev` (form S only) gives the test build with the dev control (FR-016: the delivered form S has none). Real builds are verified in T015 |
| 2026-10-11 | T004 | claude | v3/002-multi-window-drift-mvp@3d1e23c+wip | `sha256sum` of source and copy | DONE | — | — | — | — | `tools/vkeyboard/protocols/virtual-keyboard-unstable-v1.xml` vendored byte-identical from Hyprland `efb5099` (sha256 `7ad78700…`); MIT notice recorded in `docs/THIRD_PARTY.md` |
