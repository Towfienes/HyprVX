# Quickstart: Feature 001 (nested only)

> Status: validated against the delivered scripts on 2026-10-11 (T050). Steps 1–2 ran in T014 and the T044/T046
> clean rebuilds, step 4 in the Sprint 1 suites, steps 3 and 5 literally in fish in run
> `20261011-095719-patched-f1c2f47-t050` (archived under `~/.local/share/hyprvx/evidence/claude/`).

All commands run from the user's **fish** shell. The scripts are Bash with a shebang; call them via `bash` or
directly.

```fish
cd /home/towfienes/Projects/HyprVX-v3

# Cache root (contracts/test-tooling.md): ~/.cache/hyprvx/<agent>/{src,build,prefix,ccache,tools,evidence}
set -x HYPRVX_AGENT claude

# 1. Pin upstream (Hyprland efb5099 + submodules, worktrees stock/v2/v3-dev; glaze b518eec) and the local scanner
bash scripts/fetch-hyprland.sh
bash scripts/build-scanner.sh                       # hyprwayland-scanner v0.4.6 into ~/.cache/hyprvx/$HYPRVX_AGENT/tools

# 2. Apply the series and build (heavy lock, MemAvailable >= 6.5 GiB, PSI watchdog, MemoryMax=6G, -j4)
bash scripts/apply-patches.sh --variant patched     # git am patches/hyprland/*.patch -> prints patchset_id
bash scripts/build-hyprland.sh --variant stock
bash scripts/build-hyprland.sh --variant patched    # configures with -DHYPRVX_DEV=ON
cat ~/.cache/hyprvx/$HYPRVX_AGENT/prefix/*/build-info.json

# 3. Unit tests (core + CameraMath), headless configure, and the tool tests
cmake -S . -B build-v3 -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_WAYLAND_PLATFORM=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-v3 -j4; and ctest --test-dir build-v3 --output-on-failure
python3 -I tools/tests/test_analyze_frame.py
bash scripts/check-single-transform.sh --self-test; and bash scripts/check-single-transform.sh

# 4. Gates and native tests (each writes and archives its own evidence run)
bash tests/native/harness/test_identity_refusal.sh  # V-07
bash tests/native/gate_a_stock_determinism.sh       # GATE-A, V-23
bash tests/native/t_id.sh                           # T-ID, V-09
bash tests/native/run_sprint1.sh                    # T-ID, US2, US4 at scale 1 and 1.25 (after GATE-N)

# 5. Manual nested session (identity re-verified before every command except start/status)
bash scripts/nested.sh start --variant patched --size 1280x800 --scale 1
set E ~/.cache/hyprvx/$HYPRVX_AGENT/evidence/manual-(date +%Y%m%d-%H%M%S); mkdir -p $E; mkfifo $E/c.fifo
bash scripts/nested.sh run --background -- ~/.cache/hyprvx/$HYPRVX_AGENT/build/testclient/hyprvx-testclient \
    --size 800x600 --log $E/client.jsonl --control $E/c.fifo
bash scripts/nested.sh dev camera 0 0 0.25          # transport A: hyprctl hyprvx-dev camera ...
echo sync > $E/c.fifo                               # ready rule: wait for frame_done of that seq (lib.sh does it twice)
bash scripts/nested.sh capture $E/z025.png          # grim -o HVX-1
python3 -I tools/analyze_frame.py bbox $E/z025.png --expect-box 35,25,200,150 --tolerance 2 --out $E/z025.json
bash scripts/nested.sh dev panic
bash scripts/nested.sh stop                         # refuses (exit 3) unless PID/start time/exe/signature still match
```

**Safety notes**
- `scripts/nested.sh` never targets the host instance: it refuses (exit 3, nothing sent) if the runtime dir is not
  a private `/tmp/hvx-*`, if the `SO_PEERCRED` PID, start time or executable differ from the recorded nested
  compositor, or if the nested signature equals the host's.
- Tests render on the headless output `HVX-1` with the configured mode; the nested window on the host
  (`WAYLAND-1`) is disabled because its size follows the host layout. The host configuration is never changed.
- Never use `ydotool`/uinput. Pointer tests (Phase 2) use `tools/vpointer` connected to the nested `WAYLAND_DISPLAY` only.
- Raw evidence (images, JSONL, logs, `build-info.json`) is written under `~/.cache/hyprvx/<agent>/evidence/<run-id>/`;
  only `summary.json` and `SHA256SUMS` are committed under `evidence/<run-id>/`. Each run is archived and verified
  into `~/.local/share/hyprvx/evidence/<agent>/<run-id>/` (inspect with `sha256sum -c SHA256SUMS`). Runs are referenced from
  `checklists/verification.md`.
