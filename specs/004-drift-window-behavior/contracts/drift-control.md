# Contract delta: Drift Mode control 1.2 (window behavior)

**Contract version**: 1.2 (2026-10-12, Feature 004). Extends 1.1 (`specs/003-trackpad-native-ux/contracts/drift-control.md`)
and 1.0; everything there stays valid. Implemented by the policy plugin in both delivery forms.

## Moving windows

- Super + left drag on a window (1.0) now snaps (`window-behavior.md` `snapMove`) while snapping is enabled.
- Shift + Super + left drag on a window moves its whole group (`groupOf`) and snaps as one. Shift is read at drag
  begin. Cancellation restores every member's offset (1.0 rules).
- Pinned windows are never moved by a drag, never snap targets and never group members. Shift + Super + drag on a
  window in no group moves it alone. A member closed during a drag leaves it; a closed snap target releases that axis.

## Built-in keys (consumed only on a monitor in DRIFT; with Drift off they reach Hyprland unchanged)

| Key | Action |
|---|---|
| Super + Left/Right/Up/Down | focus the neighbour in that direction (non-pinned windows of the scene) and reveal it; no neighbour: nothing changes, key consumed |
| Super + Home | home: camera of the scene workspace → identity |
| Super + End | center the focused window |
| Super + Shift + End | frame the focused window |
| Ctrl + Super + End | frame the focused window's group |
| Super + F1…F4 | recall bookmark 1–4 (empty: nothing) |
| Super + Shift + F1…F4 | store the current camera in bookmark 1–4 |
| Super + bracketleft / bracketright | history back / forward |
| Super + Shift + P | pin the focused window / unpin it |

Center, frame, home, bookmark recall, focus-reveal and history jumps set the camera at once and record the previous
camera in the workspace's history when the camera changes (history moves do not record). A running glide stops first.
Focus-reveal uses the frame margin. Each key press acts once; holding a key does not repeat the action (the plugin's key listener receives physical press and release events only, efb5099 `InputManager.cpp:1630`; the compositor's key-repeat timer serves repeating binds only, `KeybindManager.cpp:135`); releases pass through. Several pinned
windows: the most recently pinned is drawn on top; a pin follows its window to another workspace.

## Dispatchers

`hyprvx:focus l|r|u|d`, `hyprvx:center`, `hyprvx:frame window|group`, `hyprvx:home`, `hyprvx:bookmark store|recall N`,
`hyprvx:history back|forward`, `hyprvx:pin`, `hyprvx:unpin`. On a monitor not in DRIFT, or without a focused scene
window where one is needed, they return `err:state` and change nothing; bad arguments return `err:args`.

## Settings (config values)

`plugin:hyprvx:snap` (0/1, default 1), `plugin:hyprvx:snap_threshold` (screen px, 24), `plugin:hyprvx:snap_gap`
(world px, 12), `plugin:hyprvx:snap_release` (world px, 32), `plugin:hyprvx:frame_margin` (fraction of the smaller
screen side, 0.05). Invalid values fall back to the default with one log line per key.

## State

`hyprctl hyprvx state` adds `"pinned": [{"address":…, "screen":[x,y]}]` and per monitor `"history":[back, forward]`
sizes and `"bookmarks":[1..4 set flags]` (bookkeeping only; never a pass condition, Constitution II).

## Pin

Pin stores the focused window's current drawn top-left (screen, monitor-local) as its fixed screen position through
the mechanism (`mechanism-api.md` 1.1); unpin sets the window's offset so that its drawn top-left at the current camera
stays where it is, then clears the pin. While pinned the window is drawn at its logical size (identity camera); after
unpin its drawn size is its size times the zoom, so only at zoom 1 is the whole drawn box unchanged. With Drift off, panic or fullscreen the mechanism draws identity; the pin is
kept and applies again in Drift.
