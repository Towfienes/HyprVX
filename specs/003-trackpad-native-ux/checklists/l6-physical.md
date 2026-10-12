# L6 Physical Checklist: Trackpad-Native Drift UX (Feature 003)

**Purpose**: V-24 / SC-007 / FR-021. A human performs these items with the real trackpad in a **visible nested session**
started by the harness, and signs. Partial by design (spec Clarifications, research R2/R11): a nested session receives
two-finger scrolling but no pinch, swipe or hold, and the host acts on 3- and 4-finger gestures first.
**Status values**: PASS / FAIL / BLOCKED / NOT RUN, each with a short note. Nothing is PASS unless it was performed.

## Safety first (read before touching the trackpad)

- [x] S1 **Never make a 3- or 4-finger gesture over the nested window.** Your host compositor acts on them before the
  nested session sees them: 3 fingers down closes the focused window (the nested session window itself), 3 up goes
  fullscreen / special workspace, 3 left floats, 4 horizontal switches host workspaces, **4 down suspends the machine**.
- [x] S2 Use two fingers only. Stop the session with `HYPRVX_AGENT=claude bash scripts/nested.sh stop` (Claude runs it),
  never by closing the window with a gesture.
- [x] S3 The host Hyprland, Caelestia and GPU settings are not changed by this session; nothing is installed into the
  host compositor. The agent does not record the host screen; a video is optional and only yours.

## Session (Claude prepares it)

`nested.sh start --variant patched --interactive`, form P policy plugin loaded, three test windows (a, b, c), Drift on
(`hyprvx:on`), glide at its default 0.5. The nested window shows the Drift canvas.

## Items performed by hand

| # | Action | Expected | Status | Note |
|---|---|---|---|---|
| H1 | Two-finger scroll on empty canvas, slowly, in all four directions | The canvas follows your fingers (natural scrolling on the host), smoothly, without jumps | BLOCKED (environment) | Performed by the user: the canvas did not pan. Cause: aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`, so every nested scroll is source WHEEL. Automated: V-04 T-TP-PAN |
| H2 | Two-finger scroll with the pointer over a window, no Super | The window's content scrolls; the canvas does not move | BLOCKED (environment) | Observed: the scroll reached the window under the pointer, but as WHEEL (Feature 002 path), not the Feature 003 finger path. Automated: V-05 |
| H3 | Hold Super, two-finger scroll over a window | The canvas pans; the window does not scroll | BLOCKED (host) | The host takes every Super combination over the nested window (`SUPER + mouse_down/up` switches host workspaces; Super + drag moves the whole nested window). Automated: V-06 |
| H4 | Start a two-finger scroll on canvas and move onto a window while scrolling | The canvas keeps panning until you lift your fingers | BLOCKED (environment) | aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`: every nested scroll is source WHEEL (client logs `axis_source wheel`, `value120`), so the finger-scroll path never runs. Automated: V-07 T-TP-LOCK |
| H5 | Quick two-finger flick on canvas and lift | The canvas keeps gliding and slows down to a stop (no abrupt end, no endless drift) | BLOCKED (environment) | aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`: every nested scroll is source WHEEL (client logs `axis_source wheel`, `value120`), so the finger-scroll path never runs. Automated: V-13 T-GLIDE |
| H6 | Flick, then touch the trackpad again during the glide | The glide stops at once | BLOCKED (environment) | aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`: every nested scroll is source WHEEL (client logs `axis_source wheel`, `value120`), so the finger-scroll path never runs. Automated: V-14 T-GLIDE-STOP |
| H7 | Flick, then click or press a key during the glide | The glide stops at once | BLOCKED (environment) | aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`: every nested scroll is source WHEEL (client logs `axis_source wheel`, `value120`), so the finger-scroll path never runs. Automated: V-14 |
| H8 | Claude sets `plugin:hyprvx:glide 0`; flick again | No glide: the canvas stops when you lift | BLOCKED (environment) | aquamarine 0.15.1 `src/backend/Wayland.cpp:316-322` forwards `wl_pointer.axis` without `axis_source`/`axis_stop`: every nested scroll is source WHEEL (client logs `axis_source wheel`, `value120`), so the finger-scroll path never runs. Automated: V-13 |
| H9 | Super + Escape (Drift off), then two-finger scroll over a window and over empty space | Scrolling behaves exactly as without HyprVX | PASS | Drift turned off by `hyprvx:off` (Claude), not Super + Escape, which the host would take. The user scrolled over a window and over empty space: nothing moved on screen. Log: the window under the pointer got 814 scroll events, camera identity, every client at its Hyprland position |
| H10 | Feel: is two-finger panning in Drift comfortable at the default speed? | Your judgement (comment welcome) | BLOCKED (environment) | No finger pan in nested (see H1), so the feel cannot be judged. In a free try the user found Feature 002 Super + wheel zoom and Super + drag pan usable ("rồi ok rồi đó") |

## Items not possible in a nested session (BLOCKED, environment)

| # | Item | Why BLOCKED | Automated evidence (nested, synthetic gestures) |
|---|---|---|---|
| B1 | Pinch on canvas zooms at the pointer | The nested backend does not receive pinches (aquamarine 0.15.1 binds no pointer-gestures interface) | V-10 T-PINCH run-ids (verification.md) |
| B2 | Super + pinch over a window zooms; pinch over a window without Super reaches the app | same | V-10 |
| B3 | 3-finger swipe pans and 3-finger pinch zooms; the close/fullscreen/float bindings never fire in Drift | Pinch/swipe not received; the host acts on 3-finger gestures first | V-12 T-ARBITRATE, V-08 |
| B4 | 4-finger horizontal swipe switches workspaces in Drift and each keeps its camera | Host acts on 4-finger gestures first | V-12 |
| B5 | Cancelled gesture restores the camera | Pinch/swipe not received | V-11 T-CANCEL |
| B6 | U4: 3-finger swipes still arrive with three-finger drag enabled | Needs the trackpad driven by a compositor that owns it | — (open, master plan U4) |

These items are carried forward to the first approved hardware session with HyprVX in a compositor that owns the
trackpad (Phase 9 production candidate, or an earlier one the user approves separately). They are not counted as PASS.

## Signature

Performed by: **Towfienes**  Date: **2026-10-12**  Result: **PARTIAL: H9 PASS; H1–H8 and H10 BLOCKED (environment
or host); B1–B6 BLOCKED (environment)**

Recorded from the user's words in the session chat: "rồi ok rồi đó" and "chuột vẫn chưa hiện mà thôi cái này chắc do
chạy nested nên thôi khoogn sao" (the pointer stays invisible in the nested window; accepted as a nested limitation).

## Session notes (Claude)

- Runs: `20261012-125342-patched-039c972-l6_session` (BLOCKED: pointer invisible), `20261012-131548-patched-6abeb09-l6_session`
  (BLOCKED: software cursor froze the window), `20261012-131952-patched-c28f640-l6_session` (PARTIAL: H1 and H9 performed),
  `20261012-133415-patched-883b697-l6_session` (PARTIAL: free try).
- The pointer is invisible in the nested window through the wayland backend. A software cursor froze the nested window
  (every motion renders a frame; the host did not release buffers in time). The interactive config keeps the hardware
  cursor with `use_cpu_buffer = 1` (pointer still invisible, input works).
- research R2's assumption (nested receives two-finger scrolling) holds only for scroll events, not for their finger
  source. Every finger-scroll item moves to the hardware session together with B1–B6.
