# R1 source verification: Feature 003 (T043)

Written on 2026-10-12 against `v3/003-trackpad-native-ux@749c5be` (plugin sources unchanged since `789c75c`),
**before** the clean-room rebuild and its suite run, as T043 requires. The results section is appended after the runs
and does not change the findings above it.

## FR-001: nothing is consumed while Drift is OFF

- `platform/plugin/src/policy/Gesture.cpp:300`: a scroll event is ignored (returned without `info.cancelled`) unless
  it is a finger-source event and either a scroll sequence is already open or some monitor is in Drift.
- `Gesture.cpp:310-312`: an event is cancelled only when the open sequence's target is not `PassThrough`. The target
  is decided once at the sequence begin (`openSession`, `Gesture.cpp:163-176`); `beginContext`
  (`Gesture.cpp:148-161`) leaves `ctx.drift = false` when the pointer is not on a Drift monitor
  (`driftMonitorUnderPointer`, `Session.cpp:246`), and `decideTarget` then gives `PassThrough` (contract
  gesture-model 1.0; unit tests V-01).
- A sequence that began in Drift keeps being consumed after Drift is left until its end. This is FR-013 ("the rest
  of the sequence stays consumed"), not an FR-001 breach; T-TP-MODES checks it.
- `Gesture.cpp:217-234`: a gesture begin with no Drift monitor returns before opening a session; update/end then see
  a closed gesture (`consumed`, `Gesture.cpp:237-242`) and are not cancelled.
- `Gesture.cpp:218` and `:298-299` stop a glide on every gesture begin and axis event. This only stops camera
  motion; it cancels no event. A glide can exist only after a consumed pan in Drift.
- `Input.cpp:50` and `:77` (key, button) only call `stopGlide()`; the Feature 002 OFF early returns in those
  listeners are unchanged.

Finding: none.

## Constitution IV: no inline camera arithmetic in the new policy and core files

- Camera changes in `Gesture.cpp` go only through `model().pan` (`:125`, `:200`, `:262`) and `model().setCamera` with
  a camera from `drift::pinchCamera` (`:245`, `:286`). `pinchCamera` (`core/src/GestureModel.cpp:218-225`) delegates
  the camera to `cammath::anchorZoom`.
- `Gesture.cpp:196` (`-delta * touchpad * panFactor`) and `:259` (`delta * F`) scale an input delta into a screen
  pan. They do not use a camera field.
- `core/src/GestureModel.cpp:223` `start.zoom * scale` computes the target zoom level (a scalar product). The lint
  reports it, because it matches `zoom *`. It is the only finding, recorded in `static_evidence.sh` as the reviewed
  list. CameraMath is vendored by patch 0001, which this feature may not change.
- Lint run `20261012-094235-static-749c5be-f002_static`: policy and form S mechanism (20 files) PASS.

Finding: one reviewed lint match, not a second camera equation.

## Constitution V: no write to offsets, L(w), workspace or layout from trackpad input

- `grep` of `Gesture.cpp` for offset, `moveWindow`, `m_realPosition`, `m_workspace`, `changeWorkspace`, `dispatch`
  and layout: no match. Its only state changes are `model().pan`, `model().setCamera` and `sync(true)` (lines above).
- `Session.cpp:108`, `:152`, `:178` and `:188-189` add only `stopGlide()` calls. `Session.cpp:207` adds `"glide"`
  to the `hyprctl hyprvx state` JSON.
- T-TP-DRAG (V-16) checks that trackpad input during a move drag does not change the dragged window. T-ARBITRATE
  (V-12) checks that 3-finger gestures in Drift change no window (`hyprctl clients` before and after) and fire no
  binding.

Finding: none.

## FR-020: no `hvxtest` in delivery builds

- The test plugin is a separate CMake project (`tools/testinput/CMakeLists.txt:8`, `:24`; one source
  `tools/testinput/src/main.cpp`). `scripts/build-plugin.sh` builds it only with `--testinput`, which excludes
  `--dev` (`:7-8`, `:20`), into `plugin/testinput-<form>/`.
- `platform/plugin` sources contain no `hvxtest` (`grep -rn hvxtest platform/` gives nothing).
- T-FR020 run `20261012-094227-patched-749c5be-t_fr020` found no `hvxtest` string in `plugin/P` or `plugin/S`, and
  `unknown request` from stock + S and from patched-nodev + P. Both positive controls passed.

Finding: none.

## Tests changed after the code they test existed

| Commit | Test change | Reason |
|---|---|---|
| `5d254f9` (after `8468a63`, scroll sessions) | `f003_lib.sh` `tp_scene_ok`: "fully on the output" uses the logical size `SIZE / SCALE` | At scale 1.25 it compared logical boxes with the physical size, so windows partly off the logical output were judged. This made the check stricter. |
| `7a72be4` | `tp_scene_ok` fails when no window is judged | Closes a vacuous PASS. Stricter. |
| `9aedd30` (after `7a4b694`, pinch) | `t_cancel.sh` gains its 3-finger part | Planned extension (T029), not a change to the pinch part |
| `09dfe85` | `t_arbitrate.sh` control judged on socket2 events instead of a `clients` snapshot. `t_tp_off.sh` captures with `b` after the close binding removed `a`, and compares the 4-finger case by active workspace | The `clients` snapshot raced the binding, and `a` was gone, so its ready rule had no reader. The workspace after a 4-finger swipe has no client to capture. The stock comparison is kept, with a vacuity check. |
| `5d34da2` | `t_arbitrate.sh` waits up to 3 s for each control event | The socket2 logger writes asynchronously. |
| `789c75c` | `t_arbitrate.sh` patterns accept the timestamp prefix | The negative Drift-phase check was vacuous. Every earlier archive was re-checked: action events appear only in the Drift-off control phase. Stricter. |
| `789c75c` | `t_glide_stop.sh` takes the workspace-case captures after returning to workspace 1 | Clients on workspace 1 are hidden on workspace 2, so they get no frame callbacks. The frame-count check stays on workspace 2. |
| `789c75c` | `t_tp_drag.sh` case 3 uses the canvas point | (480,300) lies in `a`'s border grab area after case 2 moved `a`. This was a setup error. Expectations are unchanged. |
| `789c75c` | `t_tp_modes.sh` output case judged 0 px against Hyprland's `clients` boxes | Hyprland's FALLBACK round trip moves floating windows by (-1,-1), as in Feature 002 T-MONITOR. The pixel diff against the pre-change reference could not hold for the output case. The off and panic cases keep the 0 px diff. |
| `789c75c` | `t_tp_off.sh` resets `TP_CLIENTS` after `a` closes | `pointer_driver` synced on a FIFO with no reader, and the test hung. |
| `749c5be` | `compare_forms.py` excludes 24 glide captures, with reasons | The glide end and interrupt positions depend on frame and interrupt timing. Two form P runs of the same source differ. Each run still judges the captures itself (±2 px, still picture). |
| (after `749c5be`) | `pointer_driver.py` `read_lines` leaves an unparsable last record for the next poll | T041: one of 88 Feature 002 runs (`t_input_mw` form P scale 1.25) BLOCKED. The driver read `vp.jsonl` while vpointer was appending its `done` record and got `JSONDecodeError` at char 0. The archive shows the record complete afterwards, and 5 re-runs passed. An unparsable record before the last still raises. Tool robustness only; no expectation changed. |

No expectation was loosened to make product code pass. The one product defect found during these runs (the glide
stalled on `render.pre`) was fixed in the product (`789c75c`), and the test expectations stayed as they were.

## Results (appended after the runs)

- Clean-room build A from a fresh worktree at `cf97aee`, without ccache: 5 plugins, 0 warnings, patched prefix
  `d291f743b864`, `sources_dirty` false. Then `run_feature003.sh`: **PASS 56/56**
  (`20261012-111722-suite-cf97aee-feature003`; `sha256sum -c` OK for the suite and all 56 runs).
- Build B (worktree removed and re-added at the same path): all 5 plugins **byte-identical** to A (V-21).
- Captures: form P vs S 140/140 identical; suite A vs the earlier `37aa805` suite 140/140 identical per form
  (glide end/interrupt captures excluded with the reasons in `compare_forms.py`).
- Form S built at a different path differs only in the `__FILE__` strings of the copied bodies. Proof: S rebuilt
  from the main checkout has the earlier hash, and the `strings` diff against B is the two source paths only.
- The findings above (written before these runs) stand. No result changed them.
