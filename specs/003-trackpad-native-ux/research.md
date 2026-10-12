# Research: Trackpad-Native Drift UX (Feature 003)

All source references are Hyprland `efb50993780079460b0cbed1363e2166a2de1d9f` (the pinned tree, read at
`~/.cache/hyprvx/claude/src/hyprland-efb5099`) unless a repository path is given. Checked 2026-10-12.

## R1. Where scrolls and gestures can be intercepted

- **Decision**: The policy plugin listens on Hyprland's event bus: `input.mouse.axis` for scrolls and
  `gesture.swipe.{begin,update,end}` / `gesture.pinch.{begin,update,end}` for gestures. It sets `info.cancelled` for
  every event of a sequence it consumes. No new mechanism hook and no patch are needed.
- **Rationale**:
  - `CInputManager::onSwipeBegin/Update/End` and `onPinchBegin/Update/End` (`src/managers/input/InputManager.cpp:2251-2315`)
    emit the bus event first and return when it is cancelled, *before* `g_pTrackpadGestures->gestureBegin/Update/End`
    (the config `gesture = …` bindings: close, fullscreen, float, special, workspace) and before
    `PROTO::pointerGestures` forwards the gesture to the client. One cancel stops both the compositor binding and
    the client delivery.
  - `CInputManager::onMouseWheel` emits `input.mouse.axis` (`InputManager.cpp:987`) before keybinds
    (`onAxisEvent`, :999) and before the scroll factor is applied (:1043), so the policy sees the raw device delta
    and the source (`IPointer::SAxisEvent`, `src/devices/IPointer.hpp:39-47`: `timeMs`, `source`, `axis`, `delta`,
    `deltaDiscrete`). Feature 002 already uses this listener for Super + wheel (source wheel only,
    `platform/plugin/src/policy/Input.cpp:82`).
  - Both are public plugin API (Feature 002 R4), so forms P and S get identical behavior from the same policy code.
- **Alternatives considered**: a mechanism hook on `CTrackpadGestures::gestureBegin` (would not stop client
  delivery and adds a form S hook); a patch in `onSwipe*` (unnecessary, the bus is already cancellable).

## R2. What a nested session can receive, and what that means for L6

- **Finding** (read-only checks, 2026-10-12):
  - The nested compositor's input comes from aquamarine's Wayland backend. `/usr/lib/libaquamarine.so.14`
    (aquamarine 0.15.1) contains the client interfaces it binds; the list has `wl_pointer` with `axis_source`,
    `axis_stop` and `axis_value120`, and no `zwp_pointer_gestures_v1`. Its only gesture code is the libinput path
    of the DRM backend (`libinput_event_gesture_*`).
  - Result: two-finger scrolls on the real trackpad reach a nested session as finger-source axis events with a
    stop; pinches, swipes and holds never do.
  - **Correction (T045, 2026-10-12)**: wrong for the finger source. The interface list only shows what the binary
    binds; its handler (`aquamarine v0.15.1 src/backend/Wayland.cpp:316-322`) forwards `wl_pointer.axis` with the axis
    and the delta only, so `axis_source` and `axis_stop` are dropped and every nested scroll is source WHEEL (L6 client
    logs: `axis_source wheel`, `value120`). A nested session therefore gets no finger scroll and no stop either: the L6
    finger-scroll items (H1, H2, H4–H8, H10) are BLOCKED and move to the hardware session with B1–B6. The automated
    evidence is unaffected (vpointer `fscroll` sends finger source and stop through the virtual-pointer protocol).
  - Host config (read-only): `~/.config/hypr/config/inputs.lua:14-17` (4 fingers horizontal = workspace;
    3 down = close; 3 up = fullscreen; 3 left = float), `~/.config/hypr/hyprland/gestures.lua:15-27` (3 up = special,
    3 down = toggle special, 4 down = `systemctl suspend-then-hibernate`), `hyprland/input.lua:12` natural scroll on.
    The host runs these before any client sees the gesture.
- **Decision** (user's answer to the spec clarification, 2026-10-12): L6 is done in a nested session for what
  physically reaches it (two-finger pan, Super + two-finger pan, glide, Drift-off scrolling). Pinch, 3-finger and U4
  hardware items are BLOCKED (environment), carry the automated evidence, and move to a later approved hardware
  session. The L6 checklist warns the human never to make 3- or 4-finger gestures over the nested window.

## R3. Scroll sequences: begin, end, target and pan distance

- **Decision**:
  - Only finger-source axis events (`WL_POINTER_AXIS_SOURCE_FINGER`) start a trackpad sequence. Wheel, continuous
    and tilt sources keep Feature 002 behavior (Super + wheel zooms; otherwise delivered).
  - A sequence begins with the first finger-source event with `delta ≠ 0` while no sequence is open. It ends when
    both axes have sent a stop (`delta == 0` from a finger source; that is how `axis_stop` arrives, see R9), or after
    200 ms without a finger-source event (devices that never send a stop).
  - The target is decided at begin (FR-002) and kept until the end:
    1. not in Drift, a layer surface or layer popup under the pointer, or a fullscreen window on the scene
       workspace → **pass through** (never cancelled);
    2. a Feature 002 drag is in progress → **consume and ignore**;
    3. Super held → **pan**;
    4. a window under the pointer → **pass through**;
    5. otherwise (empty canvas) → **pan**.
  - Pan distance: `screenDelta = −(dx, dy) · f`, with `f = input:touchpad:scroll_factor × plugin:hyprvx:pan_factor`
    (pan factor default 1.0). A positive scroll value means "scroll down", which moves a document's content up;
    the canvas follows the same rule, so it moves exactly like a scrolled window. Natural scrolling is applied by
    the input stack before the event reaches Hyprland, so with natural scrolling on (the user's setting) content
    follows the fingers. The touchpad factor makes canvas speed equal to application scroll speed.
- **Rationale**: the stop event is the "fingers lifted" signal on wl_pointer; the timeout only covers missing
  stops. Deciding at begin avoids a sequence that pans and then scrolls a window under the moving pointer.
- **Alternatives considered**: start on any source (would turn mouse wheels into pans, breaking Feature 002);
  invert by reading `natural_scroll` (wrong: the value already arrives inverted).

## R4. Gestures: target, pinch zoom, swipe pan, cancel

- **Decision**:
  - At `begin`, the target is decided once:
    - not in Drift → pass through;
    - 4 or more fingers (swipe or pinch) → pass through (workspace swipe stays stock, FR-011);
    - 3 fingers → always consumed (FR-010): over a layer, with a fullscreen window or during a Feature 002 drag it is
      ignored; otherwise swipe = pan, pinch = zoom (checklist CHK002: a pass-through 3-finger gesture over the bar
      would run the user's "close" binding in Drift);
    - 2-finger pinch: layer or fullscreen → pass through; drag in progress → ignore; Super held or empty canvas →
      zoom; over a window without Super → pass through.
  - The full ordered table is `contracts/gesture-model.md` `decideTarget`.
  - Pinch: `scale` in `SPinchUpdateEvent` is cumulative from the gesture start (libinput semantics,
    `IPointer.hpp:70-75`). Each update sets the scene camera to
    `anchorZoom(C0, M, a0, z0 · scale, [0.1, 2.0])`, where `C0`, `z0` are the camera at begin and `a0` the pointer
    at begin. Absolute from the start camera means no drift from rounding and makes cancel exact. Rotation and the
    pinch's translation delta are ignored.
  - Swipe (3 fingers): `screenDelta = +delta · pan_factor` (libinput reports finger motion; content follows the
    fingers, independent of natural scrolling; the touchpad scroll factor does not apply to swipes).
  - `end` with `cancelled = true` → the camera is set back to `C0`; no glide.
  - A consumed gesture keeps being consumed until its end even if Drift is turned off meanwhile (otherwise Hyprland
    would get an `update`/`end` without a `begin`); after the monitor leaves Drift no camera change is applied.
- **Alternatives considered**: incremental zoom per update (the V1 adapter, R12; accumulates error and cannot
  restore on cancel); using the pinch's own anchor (the pointer does not move during a pinch; the begin pointer is
  the documented anchor).

## R5. Momentum (glide)

- **Decision**:
  - **Release velocity**: from the consumed pan samples `(t, dx, dy)` in screen px (after R3/R4 mapping; samples
    with the same timestamp are merged). Let `tn` be the last motion sample and `W = 100 ms`. Take the samples with
    `t ≥ tn − W`; if fewer than 2, `v = 0`. Otherwise `v = (Σ deltas except the oldest) / (tn − t_oldest)`. If the
    end (stop or swipe end) comes more than 50 ms after `tn`, `v = 0` (the fingers rested before lifting). `|v|` is
    clamped to 6000 px/s. Only event timestamps are used, so the estimate is deterministic for given events.
  - **Glide**: reuse V1 `hyprvx::CameraController` (`core/include/hyprvx/core/CameraMotion.hpp`,
    `core/src/CameraMotion.cpp:282-348`). Its damped update is the exact closed form of `v' = −λv`
    (`v(t) = v0·e^{−λt}`, displacement `v0(1 − e^{−λt})/λ`), so splitting time into frames of any length gives the
    same path. It drives a screen-space `hyprvx::Camera` used only as an integrator; each frame the change of that
    camera's position is applied with `panByScreen` to the scene camera. Pan input stays zero; `maxSpeed = 6000`,
    `stopSpeed = 1 px/s`.
  - **Strength mapping**: `plugin:hyprvx:glide` = s ∈ [0, 1], default 0.5. s = 0 → no glide. Otherwise
    `λ = 2 + 10·(1 − s)` per second (s = 0.5 → λ = 7, close to the V1 default damping 7.5; s = 1 → λ = 2).
    Glide starts only if `|v| ≥ plugin:hyprvx:glide_min_speed` (default 100 px/s).
  - **Expected total distance** (used by tests): `(|v| − stopSpeed)/λ` along `v`; the tail after the stop speed is
    below `1/λ` px.
  - **Interruption**: any axis event, gesture begin, button, key, Drift off, panic, workspace change or monitor
    removal stops the glide at once (velocity 0, no further pan).
- **Rationale**: Constitution VIII (reuse the verified closed-form integrator); determinism lets tests compare the
  final camera against a formula within 2 px.
- **Alternatives considered**: a new decay implementation (duplicate of `CameraController`); an explicit-Euler
  per-frame step (frame-rate dependent).

## R6. Driving glide frames without polling

- **Decision**: while a glide is active, the policy listens to `render.preChecks` (`src/event/EventBus.hpp`,
  emitted per monitor at the start of every frame attempt, before the damage check) for the glide's monitor,
  advances the controller by the time since the previous step (steady clock), applies the pan and schedules the
  next frame. The pan changes the camera, which damages the monitor through the mechanism, so that frame renders;
  when the glide stops nothing changes and no further frames are produced (Constitution VI). The first step is
  triggered by the release itself.
- **Correction (T036, 2026-10-12)**: this decision first named `render.pre`. Hyprland emits `render.pre` only after
  the damage check (`src/render/Renderer.cpp` renderMonitor), so a step there cannot supply the damage that keeps the
  next frame alive: in the first T-GLIDE run a 3.5 s glide got two frames and stalled. `render.preChecks` is the same
  point where Hyprland's animation manager ticks. The behavior requirement is unchanged.
- **Verification**: a frame counter in the test-only plugin (R9) counts `render.pre` per monitor; the test checks
  that the count stops increasing after the glide ends.
- **Alternatives considered**: an event-loop timer (produces wakeups with no damage); `hyprctl` from the test
  (polling, forbidden).

## R7. Coexistence with Feature 002 controls and other surfaces

- Super + wheel keeps zooming for wheel sources only (unchanged); Super + two-finger scroll pans (master plan §D).
- Super + left drag (pan or move) keeps priority: trackpad sequences that begin during a drag are consumed and
  ignored; a drag that begins during a trackpad sequence ends that sequence first (FR-014).
- Layer surfaces (top/overlay layers and layer popups, same test as `pressTarget`, `Input.cpp:51-63`) and a
  fullscreen window on the scene workspace (`m_hasFullscreenWindow` of the workspace) get stock delivery.
- Special workspace: Feature 002 refuses Drift on a special workspace; nothing changes.

## R8. Mode changes during a gesture or glide

- Turning Drift off, panic, a workspace change and monitor removal stop the glide and close the gesture session's
  camera effect; a consumed gesture or scroll sequence stays consumed until its natural end (R4), and passes nothing
  to clients mid-sequence. `shutdownInput()` (plugin unload) clears the sessions and the glide.

## R9. Test input and measurement

- **Finger scrolls**: `hyprvx-vpointer` gains a `fscroll` command that sends finger-source axis events with
  explicit timestamps and an optional stop, through `zwlr_virtual_pointer_v1` on the verified nested display.
  - Quirk of Hyprland's virtual pointer (`src/protocols/VirtualPointer.cpp:44-80`): each `axis` request rebuilds
    the pending event with the default source (wheel), and `axis_source` applies to the most recent axis. The tool
    therefore sends `axis` then `axis_source(finger)` for each axis, then `frame`. `axis_stop` keeps the source of
    that axis's previous event, so a stop after a finger event arrives as a finger-source `delta == 0` event.
  - The request's `time` becomes `SAxisEvent::timeMs`, so tests control the release velocity exactly.
- **Gestures**: a test-only plugin `hyprvx-testinput` (built for form P against the patched prefix and for form S
  against the stock prefix, loaded only by the harness into nested sessions) registers the hyprctl command
  `hvxtest`. `hvxtest gesture …` calls the public `CInputManager::onSwipeBegin/Update/End` and
  `onPinchBegin/Update/End` (`InputManager.hpp:146-152`) with the given fields, so a synthetic gesture takes the
  real path: event bus → `CTrackpadGestures` → pointer-gestures protocol. `hvxtest frames <monitor>` reports the
  `render.pre` count. The command word does not start with `hyprvx`, so Hyprland's prefix matching of hyprctl
  commands cannot route it to the policy's `hyprvx` command (Feature 002 fix `2deb797`).
  - FR-020: the test plugin is a test tool. It is never part of form P or form S, never built into a delivery
    artifact, and the harness loads it only into verified nested sessions; T-FR020 checks that delivery builds
    without it answer `unknown request` to `hvxtest`.
  - Alternatives considered: patch 0014 in the dev control plus a copy in the form S dev control (two
    implementations and a patch series change for a test-only function).
- **Nested config**: the shared test config adds the user's window-action gesture bindings in Hyprlang syntax
  (`gesture = 3, down, close`, `3, up, fullscreen`, `3, left, float`, `4, horizontal, workspace`; legacy handler
  `src/config/legacy/ConfigManager.cpp:618`). Bindings that run commands (the host's suspend) are never copied.
  The config change requires a GATE-A re-run (stock determinism, as in Feature 002 T014).
- **Test client**: binds `zwp_pointer_gestures_v1` (XML from the system `wayland-protocols`, already generated by
  `CMakeLists.txt:194`; nothing vendored) and logs swipe/pinch begin, update and end, plus `axis_source`,
  `axis_stop` and `axis_value120`, so "delivered to the window" is checked on the client side.

## R10. Delivery forms

- The feature is policy-only (event bus, `render.pre`, existing camera operations). Form P and form S run the same
  policy code; the form S mechanism and the patch series 0001–0013 are unchanged, so the patchset id stays
  `d291f743b864` and no copied function changes.
- Both forms run the whole suite; captures are compared byte for byte (P/S parity, as Feature 002 V-23).

## R11. L6 checklist (physical hardware, nested, human)

- One nested session (patched build, form P plugin, dev build not required) on a host output window, started by the
  harness; Drift is on; three test windows.
- Items the human performs with the real trackpad: two-finger pan on canvas (content follows fingers), two-finger
  scroll over a window (window scrolls, camera still), Super + two-finger pan over a window, fast release glides and
  stops, touching again stops the glide, glide setting 0 has no glide, Drift off: scrolling is stock.
- Items recorded BLOCKED (environment) with the automated evidence: pinch zoom, Super + pinch, pinch over a window,
  3-finger swipe/pinch arbitration, 4-finger workspace swipe in Drift, U4.
- Safety lines at the top: never perform 3- or 4-finger gestures over the nested window; the session is stopped by
  the harness, not by closing the window with a gesture.
- Video: optional and recorded by the user if wanted; the agent never records the host screen.

## R12. Reuse (Constitution VIII)

- Reused: `CameraMath` (`panByScreen`, `anchorZoom`), `DriftModel` (cameras, modes, drags), V1 `CameraController`
  and `Camera` (glide integration), the Feature 002 policy listeners, harness, vpointer, test client and analyzer.
- Not reused, with source evidence: V1 `GestureNavigationAdapter` (`daemon/include/hyprvx/runtime/RuntimeNavigationInput.hpp:104-130`,
  `daemon/src/RuntimeNavigationInput.cpp:276-344`). It scales normalized deltas by 1000 px (`:286`), turns each
  pinch update into an incremental zoom factor (`:304-312`, error accumulates and cancel cannot restore), accepts
  only 2-finger pinches (`:298`), maps swipes to a directional focus action (`:333-342`) and has no target-at-begin
  or consumption decision. It also lives in the V1 runtime library (`hyprvx_runtime_navigation`, linked to the V1
  projection runtime), not in `hyprvx_core`.
- New: `GestureModel` in `hyprvx_core` (pure: sequence targeting, release velocity, glide state on top of
  `CameraController`), unit-tested in L2.

## R13. U4 (3-finger swipe with three-finger drag on)

- libinput's three-finger drag uses 3-finger motion as a pointer drag; the expectation is that 3-finger swipes are
  then not generated. This cannot be checked in a nested session or without host input access, so U4 stays open
  and BLOCKED for L6. 3-finger swipe pan is implemented and tested with synthetic gestures only; two-finger pan is
  the supported physical path. If a 3-finger drag ever arrives as button + motion, it is ordinary pointer input and
  never a window action.
