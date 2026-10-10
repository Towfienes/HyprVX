# Work Order WO-04 — Codex lane: fix the T014 PSI-trigger blocker (T013 follow-up), then re-run T014

**Issued by**: Claude · **Date**: 2026-10-11 · **Base**: `v3/001-native-spatial-camera` (run
`git fetch origin && git merge --ff-only origin/v3/001-native-spatial-camera` first; it contains your `5a92ea6`).
**Mode**: **Auto-review: allowed** (DV3-14, master plan §O3). Allowed escalations: git commit/fetch/merge in your
clone, `systemd-run --user --scope` and the build under `heavy.lock`, the PSI-trigger tests below. Nothing in this
work order starts or controls a compositor. All WO-01 rules apply; add the `Escalations:` trailer to every commit.
No new task IDs, no roadmap, ownership or gate change.

## Root cause (Claude diagnosis, evidence `20261011-013513-psi-1f57245`, archived)

The kernel's `psi_write()` copies the written bytes into a small buffer and sets the **last written byte** to NUL.
`scripts/build-hyprland.sh` writes `b"full 100000 2000000"` with no terminator, so the kernel parses
`full 100000 200000`: a 200 ms window, below the 500 ms minimum and not a multiple of 2 s (required for
unprivileged triggers), hence `EINVAL`. Reproduced on this host (kernel 7.2.9-1-cachyos, uid 1000):

| Bytes written | Result |
|---|---|
| `full 100000 2000000` (script today) | `EINVAL` |
| `full 100000 2000000\0` | OK |
| `full 100000 2000000\n` | OK |
| `full 100000 200000\0` (what the kernel parsed) | `EINVAL` |

## Part A — fix and verify, then STOP for Claude review

Separate commits, each with the WO-01 trailers plus `Escalations:`.

**A1 (T013 follow-up) PSI trigger.** Write the trigger with a terminating NUL (`b"full 100000 2000000\0"`, as in
the kernel PSI documentation example). Keep everything else exactly as contracted:
- `MemAvailable ≥ 6815744 KiB` before the lock and again after acquiring it; `full avg10 > 10` aborts;
  trigger `full 100000 2000000` (100 ms stall in a 2 s window); `flock -w 3600` on `heavy.lock`;
  `systemd-run --user --scope -p MemoryMax=6G`; `-j4`.
- A registration failure for any other reason stays BLOCKED. Do not replace the trigger with polling, do not make
  it optional, do not lower any threshold.
- Move the embedded Python of `build-hyprland.sh` into a module under `tools/` (for example
  `tools/hyprvx_build.py`, invoked with `python3 -I`) so the trigger payload and the watchdog decision are
  importable by tests, with the poller, the avg10 reader and the stop command injectable.

**A1 tests** (in `tools/tests/`, run by the same entry point as `test_build_scripts.py`; no build, no induced memory
pressure):
1. Real kernel: registering the exact payload the script uses on `/proc/pressure/memory` succeeds and the fd can be
   registered for `POLLPRI` and closed. If `/proc/pressure/memory` is absent, report NOT RUN (skip with reason),
   never PASS.
2. Regression: the unterminated payload `b"full 100000 2000000"` returns `EINVAL` on this kernel (documents the root
   cause; if a kernel accepts it, skip with the reason).
3. Watchdog: with injected fakes, a `POLLPRI` event with `full avg10 > 10` stops the scope unit (the stubbed stop
   command receives the unit name) and exits BLOCKED; `avg10 ≤ 10` keeps waiting; `POLLERR`/`POLLHUP`/`POLLNVAL`
   exit BLOCKED and stop the unit.
4. Existing tests (agent required, dry run executes nothing, low-memory guard stops before lock or build) still pass.

**A2 (WO-03, T013 follow-up).** `--variant patched` configures Hyprland with `-DHYPRVX_DEV=ON`; `stock` and `v2`
must not pass it. Add a dry-run assertion for all three variants.

**A3 (review R-3).** Every Python invoked by `scripts/apply-patches.sh` and `scripts/build-hyprland.sh` runs with
`python3 -I`.

Verify: `bash -n`, `--dry-run` for each variant, all `tools/tests/` suites, `git diff --check`. Evidence run
`<YYYYMMDD-HHMMSS>-tooling-<short-sha>` with the test logs. Then **STOP** and hand off (WO-01 §5 format).

## Part B — T014 re-run (only after Claude accepts Part A; the user relays "WO-04 Part B released")

1. Merge the integration branch (`--ff-only` if possible, otherwise `--no-ff`).
2. Preflight `MemAvailable ≥ 6.5 GiB` (unchanged); if lower, BLOCKED as before.
3. `HYPRVX_AGENT=codex scripts/build-hyprland.sh --variant stock`. The live PSI trigger must register; the build
   runs in the scope under the lock.
4. Evidence run `<ts>-stock-<short-sha>`: preflight, build log, `build-info.json` (test-tooling §3), postflight
   (`flock -n heavy.lock true`, worktree still clean at the pin).
5. **STOP** after T014 and hand off. Batch 1b continues under WO-01 rules in a session **without** Auto-review,
   because T015–T017 live tests start nested sessions (DV3-14).

## Not in this work order
Review R-2 (re-applying a changed patch series for T040) comes in a later work order. T009 (WO-02) and the WO-03
T017/T020 items stay queued behind batch 1 as before.
