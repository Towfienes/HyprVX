# Contract delta: Test tooling 1.6 (window behavior)

**Contract version**: 1.6 (2026-10-12, Feature 004). Extends 1.5 (`specs/003-trackpad-native-ux/contracts/test-tooling.md`).

- `hyprvx-vkeyboard` key table adds `KEY_HOME`, `KEY_END`, `KEY_F1`…`KEY_F4`, `KEY_LEFTBRACE`, `KEY_RIGHTBRACE`,
  `KEY_LEFT`, `KEY_RIGHT`, `KEY_UP`, `KEY_DOWN`, `KEY_LEFTSHIFT`, `KEY_LEFTCTRL`, `KEY_P` (unknown names still exit 2;
  identity refusal unchanged).
- `lib.sh` helpers: `hvx_combo kb MOD… KEY` (press modifiers, tap KEY, release in reverse order),
  `hvx_world_box NAME` (the window's world box from `hyprctl clients` plus its offset from `hyprctl hyprvx state`).
- Socket2 `movewindow`/`movewindowv2` counting and the `hyprctl clients` snapshot comparison (Feature 002) are used in
  every Feature 004 test (SC-006).
- Frame-by-frame position logging for `t_snap_osc.sh`: after every vpointer motion step the test records the window's
  offset from `hyprctl hyprvx state` (bookkeeping, then judged as a sequence: positions may change only at the
  scripted snap and release steps).
