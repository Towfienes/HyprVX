# External infinite-canvas projects: review (2026-10-12)

Read-only review against the HyprVX V3 requirements (master plan §C5, Milestone U U-1..U-6, Constitution V).
Nothing was built, run or installed. Clones lived in the session scratchpad. Labels: **[code]** = confirmed by
reading the source (not by running it), **[README]** = claimed only, **[none]** = no evidence.

Decision (user, 2026-10-12, DV3-18): continue HyprVX on the master plan; use phantomat as a reference only.

| | phantomat (kaolti) | hypr-canvas (aaronsb) | infinity-land (otaviosenne) |
|---|---|---|---|
| Form | Hyprland plugin, ~19.4k lines | plugin, ~470 lines | full Hyprland fork |
| Hyprland | 0.56+, Lua config (host is 0.56.2 Lua) | headers of the installed build | 0.51.1 |
| Activity | 597 commits (history of hyprland-scroll-overview), last push 2026-10-08 | 14 commits, 2026-03-21..29, "super alpha" | one day of commits (2026-03-27) |
| Tests | 17 nested test scripts (~211 checks); no CI, no published results | none | none for the canvas |
| License | BSD-3-Clause | MIT | BSD-3-Clause |

## Against the HyprVX criteria

- **NORMAL ↔ DRIFT state preservation**: phantomat **[code]** saves workspace, floating, box and fullscreen, floats
  the windows and moves them between workspaces, then restores on exit, clamping boxes to the output when the
  topology changed (`scrollOverview.cpp:212-238`). This is the §C5 fallback HyprVX rejected. It would fail U-3
  (movewindow events) and risks U-4. hypr-canvas has no mode: transformed while zoom ≠ 1, no window writes
  **[code]**. infinity-land's canvas is a layout that writes `m_realPosition` **[code]**.
- **Native windows**: all three use real surfaces. phantomat has X11 and popup tests **[code]**; hypr-canvas hooks
  `position()` and 11 more functions **[code]**.
- **Trackpad**: phantomat has pinch zoom, overview gestures and finger scroll **[code]**, but no hardware evidence
  **[none]**. hypr-canvas and infinity-land: wheel and drag only.
- **Caelestia Shell**: no evidence for any of them **[none]**. phantomat is used on Omarchy (waybar). hypr-canvas
  remaps `position()` globally, so clicks on a bar while zoomed are likely wrong (inference, not run).
- **Workspace ownership**: phantomat changes it: a window's workspace follows the screen that shows most of it, and
  workspace keys are disabled on the canvas **[code]**. infinity-land replaces workspaces with frames **[README]**.
- **Unified World**: phantomat has one plane, but gets it by changing ownership (fails U-3/U-4 by design).
  hypr-canvas draws every window when zoomed, with no ownership logic **[code]**.
- **HDR**: none of them handles HDR. phantomat only propagates the image description to its blur buffers **[code]**.
- **Multi-monitor**: phantomat has linked screens or independent cameras, with a 2-monitor nested test **[code]**;
  the author uses it daily on 2 monitors (NVIDIA) **[README]**. hypr-canvas has one global camera on the focused
  monitor **[code]**.
- **Hyprland compatibility**: phantomat and hypr-canvas rely on internal hooks and need a rebuild on every Hyprland
  update. infinity-land would replace the host compositor and lags 5 releases.

## What HyprVX takes from phantomat (reference only)

Ideas and test cases only. Any copied code needs the user's approval first, and must keep phantomat's BSD-3
copyright and license notice (THIRD_PARTY record).

- Phase 5 (DriftWM window behavior): type-to-find navigator, nearest-window-in-direction rules
  (`docs/window-rules.md`), frame/fit, recent windows, minimap, undo of moves.
- Phase 8 (multi-monitor): linked screens as one desk vs independent cameras; a window across the seam;
  fullscreen per screen while other screens keep the canvas.
- Tests: its nested cases for fullscreen (`tests/fullscreen-cases-nested.py`), X11 (`tests/x11-nested.py`) and popups
  (`tests/popup-nested.py`) as a checklist of edge cases for HyprVX tests (rewritten in HyprVX's harness and
  evidence rules).
