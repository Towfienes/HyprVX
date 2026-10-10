# Contract: Test Tooling, Evidence and Resource Locks (Feature 001)

**Contract version**: 1.0 (2026-10-10; replan P5; revised 2026-10-11 before the K0 freeze: evidence handoff §8,
GATE-N decision rules §6.1). Owner: Claude (contract). Implementers: see `tasks.md`.
Changes require a version bump and an entry in `../integration-log.md`. Consumers code against this contract,
never against another agent's unfinished branch.

Results use the Constitution II vocabulary: PASS / FAIL / BLOCKED / NOT RUN / NOT AUTOMATABLE. No tool may report
PASS unconditionally, and a dispatcher reply is never a pass condition.

## 1. Directories

| Path | Purpose | Writable by |
|---|---|---|
| `~/.cache/hyprvx/<agent>/src/` | Upstream Hyprland clones and per-variant git worktrees (`stock`, `v2`, `v3-dev`) | that agent only |
| `~/.cache/hyprvx/<agent>/build/`, `prefix/<patchset-id>/` | Build trees and install prefixes | that agent only |
| `~/.cache/hyprvx/<agent>/ccache/`, `tools/` | ccache, locally built `hyprwayland-scanner` v0.4.6 | that agent only |
| `~/.cache/hyprvx/locks/heavy.lock` | Heavy-job lock file, created once by Claude | opened read-only by all |
| `~/.cache/hyprvx/<agent>/evidence/<run-id>/` | **Raw evidence** (captures, logs, analyzer JSON, `build-info.json` copy, `summary.json`, `SHA256SUMS`) | that agent only; readable by Claude and the user |
| `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` | **Retained archive**, imported and checksum-verified by Claude (§8); files read-only; never deleted by any tool | Claude only |
| `evidence/<run-id>/` (in each repo checkout) | **Committed index**: `summary.json` and `SHA256SUMS` only | the running agent |

`<agent>` ∈ {`claude`, `codex`} from `HYPRVX_AGENT` (scripts refuse to run when it is unset). Agents never share a
source tree, build tree, prefix or ccache, so neither can alter the other's binaries. `run-id` is
`YYYYMMDD-HHMMSS-<variant>-<git-short-sha>` (local time, e.g. `20261012-141500-patched-1a2b3c4`).

## 2. Resource lock and memory checks

- Heavy jobs are a Hyprland build (`build-hyprland.sh`) or a nested session (`nested.sh start` … `stop`). Only one
  heavy job runs at a time, across both agents:
  `flock -w 3600 ~/.cache/hyprvx/locks/heavy.lock <command>`. A timeout reports BLOCKED, never PASS.
- Before a Hyprland build: `MemAvailable ≥ 6.5 GiB` (`/proc/meminfo`), else BLOCKED. Before a nested session:
  `MemAvailable ≥ 2 GiB`.
- Builds run under `systemd-run --user --scope -p MemoryMax=6G` with `ninja -j4`.
- Abort and report BLOCKED if `/proc/pressure/memory` shows `full avg10 > 10` during a heavy job.
- Light jobs (core CMake `-j2`, GTest, Python analysis, editing) may run concurrently with a heavy job.

## 3. `build-info.json` and reproducibility levels

`~/.cache/hyprvx/<agent>/prefix/<patchset-id>/build-info.json`:

```json
{
  "variant": "stock | v2 | patched",
  "upstream_commit": "efb50993780079460b0cbed1363e2166a2de1d9f",
  "patchset_id": "<sha256(cat patches in git-am order)[:12]> | none",
  "patches": [{"file": "0001-….patch", "sha256": "…"}],
  "git_dirty": "hyprvx-<patchset_id> | clean-upstream",
  "binary_sha256": "…",
  "compiler": "gcc 16.2.1",
  "cmake": "4.4.4",
  "libs": {"aquamarine": "0.15.1", "hyprutils": "0.14.2", "hyprlang": "0.6.8", "hyprgraphics": "0.5.1", "hyprcursor": "0.1.13"},
  "scanner": "hyprwayland-scanner v0.4.6",
  "built_at": "2026-10-12T14:15:00+07:00"
}
```

Reproducibility is reported at three separate levels, never one claim for all three:
1. **Input**: same upstream commit, same ordered patch hashes, same toolchain and library versions.
2. **Test result**: the same L1–L5 results from two clean builds of the same input.
3. **Binary**: identical `binary_sha256`. This is not required (timestamps); report it as observed.

## 4. `hyprvx-testclient` (`tools/testclient/`)

C++ `xdg_toplevel` client using SHM `XRGB8888` (opaque). Deterministic content:
- Background RGB(32,32,32). Grid lines every 50 px, 1 px, RGB(64,64,64), starting at surface (0,0).
- 16×16 px corner markers: top-left RGB(255,0,0), top-right RGB(0,255,0), bottom-left RGB(0,0,255),
  bottom-right RGB(255,0,255). No other pixel uses these colors.
- Scale 1 buffers. With `--scale-aware`, it honours the preferred buffer scale (used for scale 1.25 runs).

CLI: `hyprvx-testclient --size WxH --title T --log PATH [--control PATH] [--layer TOP|OVERLAY] [--scale-aware]`
- `--control PATH` reads newline commands from a FIFO:
  - `fill X Y W H RRGGBB`: paint a surface-local rectangle and commit with damage limited to that rectangle.
  - `redraw`: repaint everything and commit with full damage.
  - `sync`: commit with no content change and request a frame callback.
  - `quit`.
- `--layer` maps a `wlr-layer-shell` surface with the same content instead of a toplevel (V-14).

Log (`--log`), one JSON object per line:
```json
{"t_ns": 123456789, "seq": 7, "ev": "commit", "damage": [0, 0, 32, 32]}
{"t_ns": 123999999, "seq": 7, "ev": "frame_done"}
{"t_ns": 124000000, "ev": "configure", "w": 800, "h": 600}
{"t_ns": 125000000, "ev": "enter", "x": 10.0, "y": 20.0}
{"t_ns": 125100000, "ev": "motion", "x": 11.0, "y": 21.0}
{"t_ns": 125200000, "ev": "button", "button": 272, "state": "pressed"}
```
`seq` increments per commit; `frame_done` carries the `seq` of the commit whose callback fired. Pointer
coordinates are surface-local `wl_fixed` converted to double.

## 5. Deterministic capture rules

- One nested config template, identical for every variant: animations disabled, fixed gaps/borders, no
  wallpaper animation, cursor hidden in captures (`grim` without `-c`), fixed output size and scale.
- Capture only after a ready signal, never after a fixed sleep. After any state change (client commit or
  dev-control camera change), send `sync` and wait for `frame_done` of that `seq`, then a second `sync`/`frame_done`.
  Timeouts (default 5 s) report BLOCKED.
- Each capture is recorded with camera state (`hyprvx-dev:state` output for bookkeeping only), client `seq`,
  variant, `binary_sha256` and run-id.
- **GATE-A**: three stock-vs-stock captures of the reference scene must be pixel-identical before any T-ID
  verdict. If they are not, fix the harness; never loosen T-ID.

## 6. `tools/analyze_frame.py`

Runs as `python -I tools/analyze_frame.py <mode> IMAGE [options] --out JSON`. It reads PNG (Pillow if available)
or PPM (`grim -t ppm`).

| Mode | Options | Checks |
|---|---|---|
| `bbox` | `--expect-box X,Y,W,H --tolerance PX` | Window bounding box from marker colors; all four markers present unless `--allow-clip` is given with an expected visible region |
| `diff` | `--reference IMG [--max-pixels N]` | Pixel-exact comparison (`N` defaults to 0) |

Output JSON:
```json
{"tool_version": "1.0", "mode": "bbox", "image_sha256": "…", "verdict": "PASS",
 "bbox": {"x": 140, "y": 100, "w": 200, "h": 150},
 "markers": {"tl": [140, 100], "tr": [339, 100], "bl": [140, 249], "br": [339, 249]},
 "diff": null, "reasons": []}
```
Exit codes: 0 PASS, 1 FAIL, 2 BLOCKED (unreadable input or bad arguments). The analyzer has its own tests on
synthetic images with known answers. Every FAIL lists machine-readable `reasons` (`marker_missing:<corner>`,
`bbox_mismatch`, `scale_mismatch`, `displacement_mismatch`, `pixels_differ`).

### 6.1 GATE-N decision rules (V-24)

GATE-N proves that the analyzer can detect the defect class Sprint 1 must catch. The V2-as-is run (V-08) records,
in one run-id and with the same scene and nested config:
- **S1 setup validity**: `build-info.json` shows variant `v2` and `patchset_id` = the hash of
  `research/v2-drift-mvp/*.patch`; all ready signals arrived (no BLOCKED timeout).
- **S2 camera applied**: the non-identity V2 capture differs from the V2 identity capture by `diff`, and the window
  border box changed position or size as the V2 camera predicts (research R2: decorations do transform in V2).
  This is judged on captures, never on a dispatcher reply.
- **P positive control**: the analyzer reports PASS on the stock identity capture of the same scene (from GATE-A)
  and on the synthetic known-good fixtures.
- **N detection**: the analyzer reports FAIL on the V2 non-identity captures with at least one reason in the
  expected class (`marker_missing`, `bbox_mismatch`, `scale_mismatch` for RC1 clipping; `pixels_differ` against the
  full-damage frame for RC2 stale regions).
- **H visual confirmation**: Claude inspects the same archived PNGs (and the user may, at V-20) and confirms that a
  clip or stale artifact is visible where the analyzer reports it.

| Observation | GATE-N result | Classification | Action |
|---|---|---|---|
| S1, S2, P, N and H all hold | **PASS** | Negative control detected reliably | Patch judging may proceed (with GATE-A) |
| S1 or S2 fails, or a ready signal timed out | **BLOCKED** | Invalid test setup | Fix build/adapter/harness, rerun; the analyzer is not judged |
| S1, S2 hold; P fails (analyzer also fails good images) | **FAIL** | Defective analyzer (over-reports) | Fix analyzer, add the images as regression fixtures, rerun |
| S1, S2, P hold; analyzer PASS on V2 but H shows a visible artifact | **FAIL** | Defective analyzer (misses defects) | Fix analyzer, add the images as regression fixtures, rerun |
| S1, S2, P hold; N holds but H finds no artifact at the reported place | **FAIL** | Defective analyzer (wrong reason) | Fix analyzer, rerun |
| S1, S2, P hold; analyzer PASS on V2 and H confirms no visible artifact | **BLOCKED** | V2 defect not reproduced in this scene (U1 unconfirmed) | Record as a research finding against RC1/RC2; try the documented V2 failure scenes (zoom 0.25, pan sequence); if still not reproduced, stop and ask the user to approve an alternative negative control |

GATE-N is never marked PASS without S1, S2, P, N and H. An alternative negative control (for example a
deliberately broken build) requires user approval and its own entry in `checklists/verification.md`.

## 7. `scripts/nested.sh`

Wraps the hardened `tools/hyprvx_nested.py`. Every subcommand except `start` and `status` re-verifies the target
before acting: `SO_PEERCRED` PID, process start time, instance signature ≠ host, private `/tmp/hvx-*` runtime dir.
Failed verification ⇒ exit 3 and nothing sent.

| Subcommand | Behaviour |
|---|---|
| `start --variant stock\|v2\|patched --size WxH --scale S` | Starts `Hyprland` from `~/.cache/hyprvx/$HYPRVX_AGENT/prefix/<id>/bin` (never `$PATH`), `HYPRLAND_NO_SD_VARS=1`, `dbus-run-session`, under the heavy lock |
| `run CMD…` | Runs a client with the nested `WAYLAND_DISPLAY` only |
| `dev DISPATCHER ARGS…` | Sends a `hyprvx-dev:*` dispatcher (`dev-control.md`) |
| `capture PATH` | `grim` on the nested output |
| `status` | Prints the recorded session and verification result |
| `stop` | Re-verifies, then terminates the nested compositor |
| `recover` | Cleans up a stale session whose compositor is gone, without signalling any live process it cannot verify |

Exit codes: 0 ok, 1 command failed, 2 usage, 3 identity verification failed. Logs are kept after `stop`.

## 8. Evidence layout, handoff and retention

**Write.** Tests write raw evidence to `~/.cache/hyprvx/$HYPRVX_AGENT/evidence/<run-id>/`: `build-info.json`
(copy), `session.json`, captures, analyzer JSON, client JSONL, logs, `summary.json`, and last `SHA256SUMS`
(`sha256sum` format, relative names, covering every other file in the directory). The same `summary.json` and
`SHA256SUMS` are copied to the repo index `evidence/<run-id>/` and committed. Raw files are never committed and
never deleted by tools.

**Commit trailer.** `Evidence:` names the run-id and the raw directory, for example
`Evidence: 20261012-141500-stock-1a2b3c4 ~/.cache/hyprvx/codex/evidence/20261012-141500-stock-1a2b3c4/`.
A trailer may only reference a run whose raw directory exists on this machine and whose committed `SHA256SUMS`
verifies. Results without such evidence are reported as NOT RUN, never PASS.

**Import (Claude, at every integration).** Before ticking a task:
1. Confirm `evidence/<run-id>/SHA256SUMS` in the reviewed commit is byte-identical to the one in the raw directory.
2. Copy the raw directory to `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` (`cp -a`), make the copy read-only
   (`chmod -R a-w`), and run `sha256sum -c SHA256SUMS` inside the copy.
3. Record run-id, archive path and the `sha256sum -c` result in `../integration-log.md`.
Any missing file or checksum mismatch rejects the handoff: the task is not ticked and its result is NOT RUN until a
new run is handed off. Claude's own runs are archived the same way.

**Inspect (user).** `cd ~/.local/share/hyprvx/evidence/<agent>/<run-id>/ && sha256sum -c SHA256SUMS`, then open
the PNGs and read `summary.json`. `checklists/verification.md` cites the archive path and run-id for every PASS.

**Retain.** The archive is outside both repos and outside the agent caches, so deleting the Codex clone or a cache
does not lose evidence. Nothing deletes it automatically; pruning is the user's decision.

`summary.json`:
```json
{"run_id": "20261012-141500-patched-1a2b3c4", "test": "T-Z025", "result": "PASS",
 "variant": "patched", "patchset_id": "…", "binary_sha256": "…", "artifacts": ["z025.png", "z025.json"],
 "not_run": []}
```
