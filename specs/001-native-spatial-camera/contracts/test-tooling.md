# Contract: Test Tooling, Evidence and Resource Locks (Feature 001)

**Contract version**: 1.0 (2026-10-10; replan P5). Owner: Claude (contract). Implementers: see `tasks.md`.
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
| `evidence/<run-id>/` (in each repo checkout) | Run artifacts | the running agent |

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
synthetic images with known answers. **GATE-N**: on V2-as-is captures (U1) it must report FAIL; otherwise the
analyzer is considered broken.

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

## 8. Evidence layout and Git policy

`evidence/<run-id>/` holds `build-info.json` (copy), `session.json`, captures, analyzer JSON, client JSONL,
`summary.json` and `SHA256SUMS`. Only `summary.json` and `SHA256SUMS` are committed; images and logs stay local
and are never deleted by tools. `checklists/verification.md` references run-ids, and Claude re-runs any check
before ticking it.

`summary.json`:
```json
{"run_id": "20261012-141500-patched-1a2b3c4", "test": "T-Z025", "result": "PASS",
 "variant": "patched", "patchset_id": "…", "binary_sha256": "…", "artifacts": ["z025.png", "z025.json"],
 "not_run": []}
```
