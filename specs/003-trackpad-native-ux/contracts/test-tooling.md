# Contract: Test Tooling 1.5 (delta over 1.4)

**Contract version**: 1.5 (2026-10-12, Feature 003). Base: `specs/002-multi-window-drift-mvp/contracts/test-tooling.md`
1.4 (and 1.3 before it). Every earlier rule stays: directories, heavy lock and memory checks, `build-info.json`,
deterministic captures, analyzer, identity re-verification before every mutating command, evidence layout and
archive, `sha256sum -c`. Changes require a version bump and a log entry. Research R9.

## §4 `hyprvx-testclient` additions

- Binds `zwp_pointer_gestures_v1` (version 1, XML from the system `wayland-protocols`, already generated) for its
  pointer and logs, with `"surface":<name>`:
  - `{"ev":"swipe_begin","fingers":N}`, `{"ev":"swipe_update","dx":…,"dy":…}`, `{"ev":"swipe_end","cancelled":0|1}`;
  - `{"ev":"pinch_begin","fingers":N}`, `{"ev":"pinch_update","dx":…,"dy":…,"scale":…,"rotation":…}`,
    `{"ev":"pinch_end","cancelled":0|1}`.
- Logs `{"ev":"axis_source","source":"wheel"|"finger"|"continuous"|"tilt"}`, `{"ev":"axis_stop","axis":"v"|"h"}` and
  `{"ev":"axis_value120","axis":…,"value120":…}`; the existing `axis` line is unchanged.

## §4a `hyprvx-vpointer` addition

New control command:

```
fscroll T0 DT N DX DY [stop]
```

- Sends N finger-source axis frames at event times `T0, T0+DT, …` (milliseconds, the request `time` field), each with
  value `(DX, DY)` per frame. For each non-zero axis it sends `axis`, then `axis_source(finger)` (Hyprland rebuilds
  the pending event on `axis`, research R9), then `frame`.
- With `stop`, it then sends `axis_stop` at time `T0 + N·DT` for each axis that moved (non-zero value) and a `frame`
  (a stop keeps that axis's previous source in Hyprland, so an axis that never moved would arrive with the wheel source).
- Limits: `1 ≤ N ≤ 200`, `0 ≤ DT ≤ 1000`, `|DX|, |DY| ≤ 500`, finite; otherwise an `error` line and nothing is sent.
- Logs `done` like the other commands, with the parameters.
- Same identity refusal as the other commands (nested display only).

## §8 Test-only plugin `hyprvx-testinput` (`tools/testinput/`, new)

- Built by `scripts/build-plugin.sh --testinput --form P|S` into `~/.cache/hyprvx/<agent>/plugin/testinput-<form>/`
  (fresh build directory, `build-info.json`), against the patched prefix for P and the stock prefix for S. It is a
  test tool: never part of a delivery form, never installed, loaded only with `nested.sh plugin load` into a
  verified nested session after the policy plugin.
- Registers the hyprctl command `hvxtest` (exact word; it does not begin with `hyprvx`):
  - `hvxtest gesture swipe begin <fingers> <timeMs>` · `swipe update <fingers> <dx> <dy> <timeMs>` ·
    `swipe end <cancelled 0|1> <timeMs>`
  - `hvxtest gesture pinch begin <fingers> <timeMs>` · `pinch update <fingers> <dx> <dy> <scale> <rotation> <timeMs>`
    · `pinch end <cancelled 0|1> <timeMs>`
  - Each calls the public `g_pInputManager->onSwipe*/onPinch*` with exactly these fields, so the event takes the
    compositor's own path (event bus, `gesture =` bindings, pointer-gestures protocol). Replies `ok` or `err:<why>`
    (non-finite or out-of-range values are refused: fingers 1–5, |dx|,|dy| ≤ 2000, 0 < scale ≤ 100).
  - `hvxtest frames <monitor>`: the number of `render.pre` events seen for that monitor since load.
- Replies are never pass conditions except as a measurement (frame counts); every behavioral check uses captures,
  client logs, `hyprctl clients` or socket2 events.

## §5 Config additions (`tests/native/config/hyprland.conf.in`)

- The user's window-action gesture bindings, Hyprlang syntax: `gesture = 3, down, close`, `gesture = 3, up,
  fullscreen`, `gesture = 3, left, float`, `gesture = 4, horizontal, workspace`. Bindings that run commands (the
  host's `systemctl suspend-then-hibernate`, special-workspace toggles bound to exec) are never copied.
- The config change needs a GATE-A re-run (stock determinism, baseline image hash unchanged).

## §9 Feature 003 native tests (`tests/native/`)

Each test runs in form P and form S, at scale 1 and 1.25 unless noted. Finger scrolls come from `fscroll`; gestures
from `hvxtest gesture`; expected camera values from the formulas in `contracts/gesture-model.md` computed by the test
(Python), checked through captures and the analyzer (window bboxes ± 1 px unless noted).

| ID | Criterion |
|---|---|
| T-TP-PAN | finger scrolls on empty canvas at z 1 and 0.5: every window bbox moves by `−(ΣDX, ΣDY)·f` ± 1 px; no client logs an axis event |
| T-TP-WINDOW | finger scroll over a window without Super: the client logs the same axis values, source `finger` and the stops; capture unchanged |
| T-TP-SUPER | Super + finger scroll over a window: pans as T-TP-PAN; the client logs no axis event |
| T-TP-LOCK | a sequence begun on canvas keeps panning when the pointer moves over a window; one begun over a window stays delivered when the pointer moves to canvas |
| T-PINCH | 2-finger pinches on canvas at three anchors with scales 0.5, 2, 0.01 and 50: the world point under the anchor stays under it ± 1 px; zoom = clamp(z0·s); Super + pinch over a window zooms; pinch over a window without Super: client logs the pinch, capture unchanged |
| T-ARBITRATE | config with the §5 bindings; ≥ 20 3-finger swipes (down, up, left, right) and 3-finger pinches in Drift: window count, fullscreen and floating state unchanged (`hyprctl clients`), no special-workspace or fullscreen events (socket2), camera pans/zooms as specified; control with Drift off: each bound action happens; 4-finger horizontal swipe switches the workspace in both modes and each workspace keeps its camera |
| T-CANCEL | pinch and 3-finger swipe ended with `cancelled 1`: capture byte-identical to the capture before the gesture; no glide (frame count stops) |
| T-GLIDE | releases at 1000 and 3000 px/s with glide 0.5 and 1.0: final displacement after the glide = `expectedGlide` ± 2 px; `hvxtest frames` constant over 500 ms after the end; glide 0, a release below `glide_min_speed` and a 60 ms rest before the stop give no movement after the stop |
| T-GLIDE-STOP | during a glide: a finger scroll, a click, a key, `hyprvx:off`, panic and a workspace switch each stop it: two captures one frame apart after the interrupt are identical, and the frame count stops |
| T-TP-MODES | Drift off or panic in the middle of a consumed gesture: the remaining events stay consumed (no client logs them), the capture after OFF equals stock, the next gesture behaves as configured |
| T-TP-DRAG | a gesture or finger scroll during a Super + drag changes neither the camera nor the dragged window |
| T-TP-SURFACES | sequences over a top-layer surface and with a fullscreen window on the scene are delivered as stock (client logs) and move nothing |
| T-TP-OFF | Drift off: finger scrolls, pinches and 3/4-finger gestures: captures byte-identical to the stock session; client logs identical to the stock session; the §5 bindings act |
| T-FR020 | delivery builds without the test plugin (form S plugin alone, `patched-nodev` with form P plugin) answer `unknown request` to `hvxtest`; no `hvxtest` string in the delivery plugins |

Regression: the Feature 002 suite (`run_feature002.sh`) and Feature 001 suites (`run_sprint1.sh`, `run_phase2.sh`)
in both forms with the Feature 003 policy plugin; Feature 002 captures stay byte-identical (no trackpad input in
those tests) and P/S parity is checked over all Feature 003 captures.

## §L6 Physical checklist (`checklists/l6-physical.md`, created with tasks)

Human, real trackpad, one nested session started by the harness (research R11). Items PASS / FAIL / BLOCKED / NOT RUN
with a reason; the pinch, 3-finger, 4-finger-in-Drift and U4 items are BLOCKED (environment) with the automated
evidence cited. Safety lines first: no 3- or 4-finger gesture over the nested window; stop the session with the
harness. The agent never records the host screen.
