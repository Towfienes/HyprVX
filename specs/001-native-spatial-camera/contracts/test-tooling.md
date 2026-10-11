# Contract: Test Tooling, Evidence and Resource Locks (Feature 001)

**Contract version**: 1.3 (2026-10-11, T055: testclient `--popup`/`--subsurface` and per-surface pointer events, the
`hyprvx-vpointer` CLI (§4a), analyzer `--marker-set`. 1.2 editorial (DV3-15); 1.1 added `deps.glaze`; 1.0 frozen at `966e6b6`)
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

`<agent>` comes from `HYPRVX_AGENT` (scripts refuse to run when it is unset). Since DV3-15 all new work uses
`claude`; `codex` caches and archives from the retired Codex lane are kept as history and never reused. `run-id` is
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
  "deps": {"glaze": {"tag": "v7.2.0", "commit": "b518eec7a22e56ffa238b072c07f47efa7cea97f"}},
  "built_at": "2026-10-12T14:15:00+07:00"
}
```

Reproducibility is reported at three separate levels, never one claim for all three:
1. **Input**: same upstream commit, same ordered patch hashes, same toolchain and library versions, same pinned
   FetchContent sources (glaze is cloned at its commit by `scripts/fetch-hyprland.sh` and configured offline with
   `FETCHCONTENT_SOURCE_DIR_GLAZE` and `FETCHCONTENT_FULLY_DISCONNECTED=ON`; Git discovery stays disabled so that
   `GIT_DIRTY` is the truthful `hyprvx-<patchset_id>` / `clean-upstream`).
2. **Test result**: the same L1–L5 results from two clean builds of the same input.
3. **Binary**: identical `binary_sha256`. This is not required (timestamps); report it as observed.

## 4. `hyprvx-testclient` (`tools/testclient/`)

C++ `xdg_toplevel` client using SHM `XRGB8888` (opaque). Deterministic content:
- Background RGB(32,32,32). Grid lines every 50 px, 1 px, RGB(64,64,64), starting at surface (0,0).
- 16×16 px corner markers: top-left RGB(255,0,0), top-right RGB(0,255,0), bottom-left RGB(0,0,255),
  bottom-right RGB(255,0,255). No other pixel uses these colors.
- Scale 1 buffers. With `--scale-aware`, it honours the preferred buffer scale (used for scale 1.25 runs).

CLI: `hyprvx-testclient --size WxH --title T --log PATH [--control PATH] [--layer TOP|OVERLAY] [--scale-aware]
[--popup X,Y,W,H] [--subsurface X,Y,W,H] [--position X,Y] [--app-id ID] [--resizable]`
- `--control PATH` reads newline commands from a FIFO:
  - `fill X Y W H RRGGBB`: paint a surface-local rectangle and commit with damage limited to that rectangle.
  - `redraw`: repaint everything and commit with full damage.
  - `sync`: commit with no content change and request a frame callback.
  - `quit`.
- `--layer` maps a `wlr-layer-shell` surface with the same content instead of a toplevel (V-14).
- `--popup X,Y,W,H` (1.3, toplevel only): after the first configure, maps one `xdg_popup` of size W×H whose top-left is
  at parent-surface-local (X, Y) (positioner anchor rect (X, Y, 1, 1) with anchor `top_left`, so the anchor point is
  (X, Y), and gravity `bottom_right`; `constraint_adjustment` none; no grab). Content uses the popup palette below. The popup is never
  repositioned by the client; an offset outside every monitor is allowed (V-19).
- `--subsurface X,Y,W,H` (1.3, toplevel only): one desynchronized `wl_subsurface` of size W×H at parent-local (X, Y),
  placed above the parent, with the subsurface palette below.
- `--resizable` (1.3): the toplevel sets no min/max size (by default min = max = `--size`), so a compositor resize
  can change its size; the content stays `--size` (used only as the identity control of `t_drag_scope.sh`).
- Palettes (background, grid, then markers TL, TR, BL, BR; grid and markers as for the main surface):
  main RGB(32,32,32) / (64,64,64) / red (255,0,0), green (0,255,0), blue (0,0,255), magenta (255,0,255);
  popup RGB(40,40,80) / (64,64,64) / yellow (255,255,0), cyan (0,255,255), orange (255,128,0), violet (128,0,255);
  subsurface RGB(48,40,32) / (64,64,64) / pink (255,0,128), lime (128,255,0), azure (0,128,255), spring (0,255,128).
  No other client pixel uses a marker color. Popup and subsurface buffers follow `--scale-aware` like the main one.

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
coordinates are surface-local `wl_fixed` converted to double. Since 1.3, `enter`, `leave`, `motion` and `button`
carry `"surface": "main" | "popup" | "sub"` (the surface the event is for; `motion` and `button` name the surface of
the last `enter`), e.g. `{"t_ns": 125000000, "ev": "enter", "surface": "popup", "x": 10.0, "y": 20.0}`. Popup
lifecycle lines: `{"ev": "popup_configure", "x": X, "y": Y, "w": W, "h": H}` (the position the compositor chose,
parent-relative) and `{"ev": "popup_done"}` when the compositor dismisses it.

## 4a. `hyprvx-vpointer` (`tools/vpointer/`, 1.3)

C++ client of `zwlr_virtual_pointer_v1` (vendored XML, `docs/THIRD_PARTY.md`). Test-only; it moves the pointer of the
nested compositor and nothing else.

CLI: `hyprvx-vpointer --layout WxH --log PATH (--control FIFO | --script FILE)`
- `--layout WxH`: the logical size of the nested output layout (one output at (0, 0)). Absolute motion is sent with
  extents (W·256, H·256), so positions resolve to 1/256 logical px.
- Commands, one per line, each followed by `frame` and a `wl_display_roundtrip`, then a log line:
  - `move X Y`: absolute motion to logical (X, Y) (doubles, 0 ≤ X ≤ W, 0 ≤ Y ≤ H).
  - `down [BUTTON]`, `up [BUTTON]`, `click [BUTTON]`: Linux button code, default 272 (`BTN_LEFT`).
  - `quit`.
- Log, one JSON object per line: `{"t_ns": …, "seq": N, "ev": "done", "cmd": "move", "x": X, "y": Y}` after the
  roundtrip, i.e. after the compositor has processed the request and sent any resulting pointer events;
  `{"ev": "error", "message": …}` for a rejected command.
- Safety (defense in depth, in addition to `scripts/nested.sh run`, the only supported launcher): it refuses to
  connect (exit 3, nothing sent) unless `XDG_RUNTIME_DIR` is a private `/tmp/hvx-*` directory owned by the user with
  mode 0700 and `WAYLAND_DISPLAY` is a bare socket name; after connecting it reads the server PID with `SO_PEERCRED`
  and disconnects (exit 3) unless `/proc/<pid>/exe` lies under `~/.cache/hyprvx/<agent>/prefix/`. No uinput,
  `ydotool` or host display is ever used (master plan §F).

Attribution rule for tests: after each `done` line, the test sends `sync` to the testclient and waits for that
commit's `frame_done`. Pointer events for the command are logged before that `frame_done`, because the compositor
sent them before it received the commit. Events between two such barriers belong to the command between them.

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
| `bbox` | `--expect-box X,Y,W,H --tolerance PX [--marker-set main\|popup\|sub] [--region X,Y,W,H]` | Bounding box from the marker colors of the chosen palette (default `main`, 1.3); all four markers present unless `--allow-clip` is given with an expected visible region |
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
`Evidence: 20261012-141500-stock-1a2b3c4 ~/.cache/hyprvx/claude/evidence/20261012-141500-stock-1a2b3c4/`.
A trailer may only reference a run whose raw directory exists on this machine and whose committed `SHA256SUMS`
verifies. Results without such evidence are reported as NOT RUN, never PASS.

**Archive (before ticking a task).**
1. Confirm `evidence/<run-id>/SHA256SUMS` in the commit is byte-identical to the one in the raw directory.
2. Copy the raw directory to `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` (`cp -a`), make the copy read-only
   (`chmod -R a-w`), and run `sha256sum -c SHA256SUMS` inside the copy.
3. Record run-id, archive path and the `sha256sum -c` result in `../integration-log.md`.
Any missing file or checksum mismatch means the task is not ticked and its result is NOT RUN until a new run is
archived.

**Inspect (user).** `cd ~/.local/share/hyprvx/evidence/<agent>/<run-id>/ && sha256sum -c SHA256SUMS`, then open
the PNGs and read `summary.json`. `checklists/verification.md` cites the archive path and run-id for every PASS.

**Retain.** The archive is outside both repos and outside the agent caches, so deleting a clone or a cache
does not lose evidence. Nothing deletes it automatically; pruning is the user's decision.

`summary.json`:
```json
{"run_id": "20261012-141500-patched-1a2b3c4", "test": "T-Z025", "result": "PASS",
 "variant": "patched", "patchset_id": "…", "binary_sha256": "…", "artifacts": ["z025.png", "z025.json"],
 "not_run": []}
```
