# HyprVX — Drift Mode for Hyprland

**An infinite, zoomable canvas for Hyprland that is a pure *view*: turn it off and every window is exactly where
Hyprland put it.**

> **Project status: stopped (October 2026). Not maintained.** The code works for its core goal on one machine, but the
> author has stopped working on it. It is published so that anyone can pick it up, fix it and take it further.
> Issues and pull requests are welcome; there is no guarantee of review.
>
> *Tiếng Việt: Dự án đã dừng (10/2026) và không còn được bảo trì. Phần cốt lõi (zoom/pan canvas, trackpad) chạy được trên
> máy của tác giả, nhưng chất lượng hình khi zoom ra và nhiều phần khác chưa hoàn thiện. Mã nguồn được công khai để ai
> quan tâm có thể sửa và đóng góp.*

## What it does

Drift Mode puts a camera (position + zoom) on each workspace of a monitor. Windows keep their real Hyprland geometry,
workspace, tiling and floating state; HyprVX only changes how they are **drawn and hit-tested**:

- **Zoom** 0.1×–2× anchored at the pointer: `Super` + mouse wheel, or a two-finger **pinch** on a touchpad.
- **Pan**: `Super` + left drag on empty canvas, two-finger scroll on empty canvas, `Super` + two-finger scroll anywhere,
  or a three-finger swipe. A flick keeps gliding with damped momentum.
- **Move a window on the canvas**: `Super` + left drag on it. This changes a per-window draw offset; Hyprland's own
  position is never written (no `movewindow` events).
- **Input stays correct** at any zoom: clicks, focus, popups, subsurfaces, XWayland windows and menus, cursor warps.
- **One camera per workspace**; new windows open at the centre of the current view.
- **Off is exact**: `Super` + `Escape` (or `hyprctl hyprvx off`) returns to the untouched Hyprland picture, verified
  pixel-for-pixel in tests.

Planned but **not done**: snapping, window groups, keyboard navigation, bookmarks, pinning (Feature 004, paused at
its first tasks on branch `v3/004-drift-window-behavior`), one unified canvas across workspaces, multi-monitor, HDR,
Caelestia shell integration, packaging.

## Known problems (good places to help)

| Problem | Why | Where to start |
|---|---|---|
| **Zoomed-out windows look rough** (aliasing, shimmering text, jagged borders) | Client buffers are drawn at full resolution and minified without mipmaps | `patches/hyprland/0002-*`, `platform/plugin/src/mechanism/` (texture draw path) |
| Text is blurry above 1× | Buffers are upscaled, not re-rendered at the new scale | — |
| Blur and shadows are not camera-aware | Not verified yet (planned for the "performance & stability" phase) | `docs/v3/HYPRVX_V3_MASTER_PLAN.md` §C, §H Phase 7 |
| More GPU work than needed while zoomed | Render simplification is disabled when the camera is not identity; damage is over-approximated | patches 0002, 0004, 0010 |
| Lua-config Hyprland cannot call the `hyprvx:*` dispatchers | `hl.dispatch` only takes Lua dispatchers; use `hyprctl hyprvx on/off/toggle/panic` | expose them with `HyprlandAPI::addLuaFunction` |
| Reloading the plugin needs a re-login | The `.so` has `STB_GNU_UNIQUE` symbols, so glibc never unloads it; loading the same path again returns the old code | load from a new file name, or log out first |
| Physical trackpad testing is partial | The nested test compositor does not receive finger sources or pinch gestures | `specs/003-trackpad-native-ux/checklists/l6-physical.md` |
| The nested test harness fails on an NVIDIA-primary host | Headless outputs cannot allocate buffers there | `tools/hyprvx_nested.py` |
| Resizing tiled windows in Drift, multi-monitor, HDR, Caelestia | Not implemented / not verified | master plan §H |

## Requirements

HyprVX patches or hooks Hyprland internals, so it is tied to **one exact Hyprland build**:

- Hyprland **0.56.2 at commit `efb50993780079460b0cbed1363e2166a2de1d9f`**, an unmodified build (`GIT_DIRTY` =
  `clean`), with the same plugin ABI: aquamarine 0.15, hyprutils 0.14, hyprgraphics 0.5, hyprcursor 0.1, hyprlang 0.6.
  The plugin refuses to start Drift on anything else.
- Linux, CMake ≥ 3.27, Ninja, a GCC with C++26 support (GCC 16.2 was used), GoogleTest, Python 3 (+ Pillow for the
  test analyzer).

Two delivery forms exist and produce byte-identical pictures in the test suite:

- **Form S** — a plugin (`hyprvx-drift-S.so`) loaded into a **stock** Hyprland; it hooks 26 functions and carries
  copies of the changed Hyprland function bodies. This is the form that ran on real hardware.
- **Form P** — a patch series (`patches/hyprland/0001-0013`) applied to Hyprland plus a smaller policy plugin.

## Build

The build scripts were written for the author's automated workflow: they keep everything under
`~/.cache/hyprvx/<agent>/` and require `HYPRVX_AGENT=claude` or `codex` (it only names that cache lane). They also
refuse to run with less than 6.5 GiB of free memory.

```sh
export HYPRVX_AGENT=claude
scripts/fetch-hyprland.sh                       # Hyprland v0.56.2 (efb5099) source and the pinned glaze
scripts/build-hyprland.sh --variant stock       # a stock Hyprland prefix to build the plugin against
scripts/build-plugin.sh --form S                # -> ~/.cache/hyprvx/claude/plugin/S/hyprvx-drift-S.so

# unit tests (no compositor)
cmake -S . -B build -G Ninja && ninja -C build && ctest --test-dir build
```

For form P: `scripts/build-hyprland.sh --variant patched` then `scripts/build-plugin.sh --form P`.

## Use (form S, at your own risk)

A plugin bug can crash your Hyprland session. Save your work first, and **copy the `.so` to its own file name** (never
load a file that a later build will overwrite):

```sh
cp ~/.cache/hyprvx/claude/plugin/S/hyprvx-drift-S.so ~/hyprvx-drift-S-test1.so
hyprctl plugin load ~/hyprvx-drift-S-test1.so
hyprctl hyprvx on          # Drift on the focused monitor
hyprctl hyprvx off         # back to the exact Hyprland picture (also: Super + Escape)
hyprctl hyprvx panic       # everything off at once
hyprctl hyprvx state       # JSON state
hyprctl plugin unload ~/hyprvx-drift-S-test1.so
```

With a legacy (hyprlang) config the dispatchers `hyprvx:on|off|toggle|panic|pan|zoom|reset` and the settings
`plugin:hyprvx:zoom_step`, `pan_factor`, `glide`, `glide_min_speed` are also available. While Drift is on,
three-finger gestures on that monitor never trigger your Hyprland gesture bindings; four-finger gestures pass through.

## How it works

- `core/include/hyprvx/core/CameraMath.hpp` — the **single** world↔screen transform (`screen = M + (world − P)·z`).
  Every render, hit-test, damage and input path goes through it; a lint (`scripts/check-single-transform.sh`) rejects
  camera arithmetic anywhere else.
- `patches/hyprland/` — the mechanism: render seam, clipping, culling, damage, input/hit-test, per-window offsets,
  drag and the dev control.
- `platform/plugin/src/policy/` — Drift Mode itself: per-monitor state machine, cameras, input and gesture handling,
  glide. `platform/plugin/src/mechanism/` — the form S hooks and copied bodies.
- `core/src/DriftModel.cpp`, `GestureModel.cpp`, `WindowBehavior.cpp` — compositor-free models with unit tests.
- `tests/native/` — tests that run a **separate nested Hyprland** (never the user's session), drive a virtual
  pointer/keyboard and test clients, and judge **captured pixels**; each run is archived with SHA-256 sums.
  They need a host session whose primary GPU can allocate headless buffers (the author's AMD iGPU worked).

## Repository map and history

| Path | Content |
|---|---|
| `docs/v3/HYPRVX_V3_MASTER_PLAN.md` | Architecture, decisions (DV3-*), roadmap (Vietnamese with English terms) |
| `.specify/memory/constitution.md` | Project principles (view-only Drift, single transform, evidence-based testing) |
| `specs/001-…` → `specs/004-…` | Spec Kit features: spec, plan, research, contracts, tasks, verification checklists, integration logs |
| `evidence/` | Index (summary + SHA-256) of every archived test run; the raw archives are not in the repository |
| `research/` | Earlier V1/V2 attempts kept for reference |
| `.ai/`, `CLAUDE.md`, `AGENTS.md`, `.claude/` | Hand-off notes and agent instructions from the development workflow |

Branches: `v3/spec-foundation` (default; Features 001–003 merged plus the first real-hardware fixes),
`v3/004-drift-window-behavior` (paused Feature 004), `v3/u0-spike` (unified-world feasibility spike, not for merge).

Most of the code, tests and documents were written with **Claude Code** (Anthropic) under the author's direction,
following the Spec Kit workflow recorded in `specs/`.

## Contributing

Open an issue or a pull request. Useful first steps: run the unit tests, read the master plan §C (architecture) and
the constitution, then pick a row from **Known problems**. Please keep the single-transform rule and add a test with
any change to rendering or input.

## License

BSD 3-Clause (see `LICENSE`). Parts derived from Hyprland keep Hyprland's BSD 3-Clause notice; other third-party
sources are listed in `docs/THIRD_PARTY.md`. Not affiliated with the Hyprland project.
