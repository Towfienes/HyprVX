# Work Order WO-01 — Codex lane, Sprint 1, batch 1

**Issued by**: Claude (technical lead, integration owner) · **Date**: 2026-10-11
**Base**: this work order is committed on `v3/001-native-spatial-camera`. Your clone's `v3/001-codex` starts at
`76b97aa` (contracts frozen there; later commits only add Claude's log rows and this file). Before your first commit,
run `git fetch origin && git merge --ff-only origin/v3/001-native-spatial-camera` (needs the user's approval because
the sandbox keeps `.git` read-only).
**Approval**: the user approved `tasks.md` at `966e6b6` and authorized Sprint 1 tasks T001–T053 only.

## 0. Who you are and what you may touch

You are Codex, implementer of the Codex lane of HyprVX V3 Feature 001. Claude owns the plan, contracts,
`tasks.md`, compositor patches and every integration decision. You work only in
`/home/towfienes/Projects/HyprVX-v3-codex` on branch `v3/001-codex`.

Read before any change, in this order: `AGENTS.md` (and the files it lists), `.specify/memory/constitution.md`,
`docs/v3/HYPRVX_V3_MASTER_PLAN.md` §I and §O, `specs/001-native-spatial-camera/tasks.md`, all three files in
`specs/001-native-spatial-camera/contracts/`, `plan.md`, `quickstart.md`. Read `research.md` only where a task cites it.

**Files you own** (master plan §O1): `CMakeLists.txt`, `.gitignore`, `scripts/`, `tools/`, `tests/native/`,
`tests/unit/test_camera_math.cpp`, and the evidence index `evidence/<run-id>/{summary.json,SHA256SUMS}`.
**Never edit**: `specs/`, `docs/`, `.ai/`, `.specify/`, `patches/`, `core/include/hyprvx/core/CameraMath.hpp`,
`core/src/Camera.cpp`, `AGENTS.md`, `CLAUDE.md`. Never tick tasks in `tasks.md`. If a contract looks wrong or
incomplete, stop that task and report it; do not work around the contract.

## 1. Hard rules

1. Set `HYPRVX_AGENT=codex` explicitly in every command that runs project scripts.
2. One logical commit per task on `v3/001-codex`, message ending with trailers:
   ```text
   Task: T0xx
   Agent: codex
   Verify: <exact commands to reproduce>
   Result: PASS | FAIL | BLOCKED | NOT RUN (reason)
   Evidence: <run-id> ~/.cache/hyprvx/codex/evidence/<run-id>/   (or "none: <reason>")
   Not-run: <what was not verified>
   ```
   `Evidence:` may only cite a run whose raw directory exists and whose `SHA256SUMS` verifies
   (`contracts/test-tooling.md` §8). `git commit` needs the user's approval (the sandbox keeps `.git` read-only).
3. Never push. Never use `--dangerously-bypass-approvals-and-sandbox` or ask for sandbox protections to be removed.
4. Host safety (Constitution I): never run `hyprctl`, test clients or capture tools against the host session;
   never use `ydotool`/uinput; never touch `/usr`, system packages, `~/.config/hypr` or Caelestia. Nested sessions
   only through `scripts/nested.sh` once it exists.
5. Heavy jobs (Hyprland build, nested session) only under `flock -w 3600 ~/.cache/hyprvx/locks/heavy.lock`, after
   checking `MemAvailable ≥ 6.5 GiB` (builds) or `≥ 2 GiB` (nested); abort on PSI memory `full avg10 > 10`.
   Light jobs use `-j2`.
6. Network, GPU or system permissions: request escalation and explain why; if refused, mark the item BLOCKED.
7. Stop conditions (master plan §I item 9): a build that needs system package changes, a harness that cannot
   verify the nested PID, OOM or swap thrash. Stop and report.
8. Missing environment ⇒ BLOCKED or NOT RUN, never PASS. No test may pass unconditionally.

## 2. Batch 1 — execute in this order

1. **T003 — S0 sandbox smoke test.** Record each item as PASS/BLOCKED with the exact command and output, as an
   evidence run `<YYYYMMDD-HHMMSS>-s0-<short-sha>` under `~/.cache/hyprvx/codex/evidence/`:
   - write and delete a file under `~/.cache/hyprvx/codex/`;
   - `flock -n ~/.cache/hyprvx/locks/heavy.lock true` (the file is mode 0444);
   - `systemd-run --user --scope -p MemoryMax=1G true`;
   - `ls -l /dev/dri` and a read open of `/dev/dri/renderD128`;
   - `git commit` of the S0 evidence index (needs escalation);
   - informational: `git ls-remote https://github.com/hyprwm/Hyprland refs/tags/v0.56.2` (network; expected to
     need escalation).
2. **T004** `.gitignore`, then **T005** CMake options, then **T006** V-05 (headless and fully enabled configure,
   build `-j2`, `ctest`; logs as an evidence run).
3. **T010**, **T011**, **T012**, **T013**: author the build scripts exactly as `tasks.md` and test-tooling §1–§3.
   Self-check each with `bash -n` and a dry-run mode that prints actions without building.
4. **T014** (heavy, network): only if S0 shows `flock` and `systemd-run --user` PASS. Fetch needs network
   escalation. Build variant `stock`; copy `build-info.json` and the build log into an evidence run.

## 3. Batch 1b — continue only if batch 1 has no BLOCKED item

**T015**, **T016**, **T017** (harness; live negative tests need the T014 stock build), **T018** (testclient; its
CMake hunk is a separate commit), **T019** (analyzer + synthetic tests), **T021** (single-transform lint with
self-test), **T020** (config template + `tests/native/lib.sh`).

## 4. Stop points

Stop and hand off before **T009** (needs Claude's T007/T008; WO-02 will release it) and before **T022 GATE-A**
(Claude reviews the tooling first). Also stop at the first BLOCKED item that blocks later work.

## 5. Handoff

When you stop, reply with: the list of commits (sha, task, result), each evidence run-id and raw path, every
BLOCKED/NOT RUN item with its reason, and any contract question. The user will relay this to Claude, who fetches
`v3/001-codex` from outside your clone, verifies and archives evidence, re-runs verification, and merges.
