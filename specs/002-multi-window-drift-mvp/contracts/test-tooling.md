# Contract: Test Tooling 1.4 (delta over 1.3)

**Contract version**: 1.4 (2026-10-11, Feature 002). Base: `specs/001-native-spatial-camera/contracts/test-tooling.md` 1.3.
Every rule of 1.3 stays: directories, heavy lock and memory checks, `build-info.json`, deterministic captures,
analyzer, identity re-verification, evidence layout and archive. Changes require a version bump and a log entry.

## §4 `hyprvx-testclient` additions

- Binds `wl_keyboard` when the seat has one, and logs:
  - `{"ev":"kb_enter","surface":"main"}` and `{"ev":"kb_leave"}`;
  - `{"ev":"key","key":<evdev code>,"state":"pressed"|"released"}`;
  - `{"ev":"modifiers","depressed":<mask>}`.
- Logs pointer axis events: `{"ev":"axis","axis":"v"|"h","value":<float>,"surface":<name>}`.
- Existing lines and fields are unchanged.

## §4a `hyprvx-vpointer` addition

New control command `scroll DX DY`: a `wl_pointer` axis event (wheel source, value120 = 120 per unit), followed by
`frame`. It logs `done` like the other commands.

## §4b `hyprvx-vkeyboard` (`tools/vkeyboard/`, new)

```
hyprvx-vkeyboard --log PATH --control FIFO
```

- **Display**: connects to the verified nested display only. It refuses under the same identity rules as vpointer
  (§4a): the harness runs it through `nested.sh run`, and it exits 3 without `HYPRVX_NESTED=1` and a
  `WAYLAND_DISPLAY` that matches the session file.
- **Keyboard**: creates a `zwp_virtual_keyboard_v1` with a US keymap compiled by xkbcommon. The protocol XML is copied
  from Hyprland `efb5099` `protocols/virtual-keyboard-unstable-v1.xml`, and its license is recorded in
  `docs/THIRD_PARTY.md`.
- **Commands**: `down KEY`, `up KEY` and `tap KEY`, where KEY is a Linux evdev name such as `KEY_LEFTMETA`, `KEY_ESC`
  or `KEY_A`. Modifier state is sent with each change. `quit` ends it.
- **Log**: one JSON line per command (`{"ev":"done","cmd":…}`) and `{"ev":"ready"}` at start.
- **Never uinput**: `ydotool` stays forbidden.

## §4c `hyprvx-x11client` (`tools/x11client/`, new)

```
hyprvx-x11client --size WxH --title T --log PATH --control FIFO [--menu X,Y,W,H] [--class C]
```

- **Drawing**: an xcb window drawing the §4 content and markers. `--menu` maps an override-redirect child window,
  positioned at window-relative `X,Y` when the control command `menu` arrives. The menu draws the popup palette
  (test-tooling 1.3 `--marker-set popup`) and sets WM_TRANSIENT_FOR to the main window.
- **Log**: pointer motion, button and key events per window (`"surface":"main"|"menu"`, X window-local coordinates),
  and `{"ev":"drawn","seq":N}` after each redraw has been flushed with `xcb_flush` and a round trip.
- **Control**: `redraw`, `menu`, `unmenu`, `quit`.
- **Ready rule for X11**: `drawn` for the latest seq, then the §5 ready rule on a Wayland test client twice.

## §5 Config additions (`tests/native/config/hyprland.conf.in`)

- XWayland stays enabled (the stock default).
- Window rules place `hyprvx-x11client` floating at a fixed position, as for the other clients.
- Nothing else changes. The policy plugin is loaded by the harness after start (§7), not by the config, so that the
  stock baseline config stays identical for every variant.

## §7 `scripts/nested.sh` additions

- `nested.sh plugin load PATH`: loads the policy plugin into the verified session through the prefix's `hyprctl`. Its
  sha256 is recorded in the evidence. Form P loads `hyprvx-drift-P.so` into the patched build; form S loads
  `hyprvx-drift-S.so` into the stock build.
- `nested.sh xdisplay`: prints the nested XWayland display (`:N`) only if `/tmp/.X11-unix/X<N>`'s lock file
  (`/tmp/.X<N>-lock`) holds the PID of the verified compositor or of a direct child of it. Otherwise it exits 3.
- `nested.sh run --x11 -- CMD…`: like `run`, with `DISPLAY` set from `xdisplay`.

## §dev Dev control 1.2 (dev builds; base `specs/001-native-spatial-camera/contracts/dev-control.md` 1.1)

- `offset <address> <x> <y>` → `setWindowOffset`.
- `state` adds `"offsets":[{"address":…,"offset":[x,y]}]` (non-zero only).
- `panic` also resets every window offset.
- Replies remain non-evidence (Constitution II).

## §9 Feature 002 native tests (`tests/native/`)

Each test runs in form P and in form S (`HVX_FORM=P|S`), at scale 1 and 1.25 unless noted.

| ID | Criterion |
|---|---|
| T-DRIFT-OFF | enter Drift, set offsets and camera, leave: capture byte-identical to the stock capture of the same scene; `hyprctl clients` (workspace, at, size, floating, fullscreen) unchanged; no `movewindow*` event on socket2; ×3 |
| T-MULTI | three windows with distinct offsets at z ∈ {0.25, 0.5, 1, 2}: each bbox within ± 2 px of `forwardBox(L + O)` |
| T-INPUT-MW | pointer local ± 1 on offset windows, their popup and subsurface; click focus; no event on empty canvas |
| T-KEYS | after a click focuses an offset window, `tap KEY_A` reaches only that client |
| T-SCROLL | `scroll 0 1` over an offset window reaches it (axis event); with Super held it zooms instead and the client gets nothing |
| T-X11 | X11 window with offset: bbox ± 2 px, pointer local ± 1; its override-redirect menu is drawn at parent offset and gets input |
| T-MOVE | Super + left drag by d at z: window bbox moves by d (± 1 px on screen), `O` changes by d/z; `L` unchanged; no `movewindow*` |
| T-RESIZE | floating border drag in Drift: client configure size changes by d/z (± 1); tiled border drag: no size change, one log line |
| T-DRAG-ABORT | panic / `hyprvx:off` / client quit during a drag: compositor alive, no stuck grab (next click reaches a client) |
| T-ANCHOR | 20 zoom steps (dispatcher and Super + wheel) at a fixed pointer: the world point under the pointer drifts ≤ 1 px |
| T-PAN | dispatcher pan and Super + canvas drag: content displacement equals the request ± 1 px |
| T-WS-CAMERA | two workspaces with different cameras: switching shows each workspace's own camera; values never leak |
| T-WS-ANIM | with workspace animations enabled, a switch in Drift shows no intermediate frame (captures right after the switch equal the settled image) |
| T-SPAWN | a window mapped in Drift under a panned, zoomed camera is centred in the viewport ± 2 px; mapped when OFF: as stock |
| T-FULLSCREEN | fullscreen window in Drift drawn as stock (byte-identical to the stock fullscreen capture); leaving fullscreen restores the Drift view |
| T-PANIC-DRIFT | panic from Drift: next capture equals stock, 10/10 |
| T-IDEMP | `hyprvx:on` twice, `hyprvx:off` twice, panic while OFF: captures and state unchanged after the first call |
| T-STALE-MW | 50 random camera/offset states with three windows: partial-damage capture equals forced full-damage capture |
| T-MONITOR | removing the scene output while in Drift: compositor alive, Drift OFF |
| T-SCENARIO | the 12-step scenario script (steps 3/5/7 by keyboard/pointer controls; snap and pin excluded): captures per step for human review (SC-008) |

Feature 001's Sprint 1 and US3 suites are re-run as regression in both forms with the policy plugin loaded and Drift
OFF; identity captures must stay byte-identical to the Feature 001 baseline.
