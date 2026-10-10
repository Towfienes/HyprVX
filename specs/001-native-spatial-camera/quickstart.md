# Quickstart: Feature 001 (nested only)

> Status: the scripts below are specified by the plan and do not exist yet. They are created in Sprint 1 after
> approval. This page is the target workflow.

All commands run from the user's **fish** shell. The scripts are Bash with a shebang; call them via `bash` or
directly.

```fish
cd /home/towfienes/Projects/HyprVX-v3

# 1. Pin upstream and apply the patch series (outside the repo, in ~/.cache/hyprvx)
bash scripts/fetch-hyprland.sh            # clones + verifies HEAD == efb50993…
bash scripts/apply-patches.sh             # git am patches/hyprland/*.patch → prints patchset-id

# 2. Build stock-from-source and patched (memory-capped)
bash scripts/build-hyprland.sh --variant stock
bash scripts/build-hyprland.sh --variant patched
cat ~/.cache/hyprvx/prefix/*/build-info.json

# 3. Unit tests (core + CameraMath)
cmake -S . -B build-v3 -G Ninja -DHYPRVX_WITH_SDL=OFF -DHYPRVX_WITH_HYPRLAND_PLUGIN=OFF
cmake --build build-v3 -j4; and ctest --test-dir build-v3 --output-on-failure

# 4. Nested session (identity re-verified before every command)
bash scripts/nested.sh start --variant patched --size 1280x800 --scale 1
bash scripts/nested.sh run tools/testclient --size 800x600 --log evidence/run/client.jsonl
bash scripts/nested.sh dev camera 0 0 0.25
bash scripts/nested.sh capture evidence/run/z025.png
python -I tools/analyze_frame.py evidence/run/z025.png --expect-scale 0.25 --contract specs/001-native-spatial-camera/contracts/transform-contract.md
bash scripts/nested.sh dev panic
bash scripts/nested.sh stop               # refuses unless PID/start-time/signature still match
```

**Safety notes**
- `scripts/nested.sh` never targets `$HYPRLAND_INSTANCE_SIGNATURE` of the host. It refuses if the runtime dir
  is not a private `/tmp/hvx-*`, or if the `SO_PEERCRED` PID or start time differ from the recorded nested
  compositor.
- Never use `ydotool`/uinput. Pointer tests use `tools/vpointer` connected to the nested `WAYLAND_DISPLAY` only.
- Evidence (images, JSONL, logs, `build-info.json`) is kept under `evidence/<run-id>/` and referenced from
  `checklists/verification.md`.
