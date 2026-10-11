# Contract: Drift Mode control (product interface)

**Contract version**: 1.0 (2026-10-11, Feature 002). Implemented by the policy plugin `hyprvx-drift` in both delivery
forms (research R3). It replaces the dev-only control of Feature 001 as the user-facing interface; the dev control
(`specs/001-native-spatial-camera/contracts/dev-control.md`, extended in `contracts/test-tooling.md` §dev) stays in
dev builds for tests.

## Dispatchers (bindable)

Monitor = the focused monitor unless stated otherwise. Replies through `hyprctl dispatch` are `ok` or an error string.

| Dispatcher | Args | Effect | Errors |
|---|---|---|---|
| `hyprvx:toggle` | — | OFF → ENTERING → DRIFT, or DRIFT → EXITING → OFF | `err:incompatible` (validation failed; Drift stays OFF and a notification is shown) |
| `hyprvx:on` | — | Enter Drift; no-op if already entering or in DRIFT | `err:incompatible`, `err:state` (active workspace is special) |
| `hyprvx:off` | — | Leave Drift; no-op if OFF | never fails |
| `hyprvx:panic` | — | Every monitor OFF within one frame (camera identity, mechanism offsets 0). Stored offsets and cameras are kept; only the dev-control `panic` also clears them | never fails |
| `hyprvx:pan` | `<dx> <dy>` (screen logical px) | DRIFT only: `C = panByScreen(C, (dx, dy))`, i.e. content moves by (dx, dy) | `err:state` (not in DRIFT), `err:invalid` (non-finite) |
| `hyprvx:zoom` | `<factor>` (> 0) | DRIFT only: `C = anchorZoom(C, M, pointer, z·factor, [0.1, 2.0])`, clamped to the range | `err:state`, `err:invalid` |
| `hyprvx:reset` | — | DRIFT only: camera of the scene workspace → identity | `err:state` |

## Built-in inputs (no user config needed; research R4)

Active only while the monitor under the pointer is in DRIFT. Each consumed event is cancelled before keybinds and
clients.

| Input | Effect |
|---|---|
| Super + Escape (key press) | `hyprvx:off` on the focused monitor |
| Super + wheel (vertical) | zoom by `zoom_step` per detent at the pointer, wheel up = zoom in (steps = −value120 / 120, so high-resolution wheels give fractional steps) |
| Super + left button on empty canvas, drag | pan: content follows the pointer |
| Super + left button on a window, drag | move: `O(w) += Δscreen / z`; the window follows the pointer |

Other input (clicks without Super, keys, scrolling without Super) is never consumed: it reaches windows through the
camera (FR-010).

## Config values

| Key | Type | Default | Rule |
|---|---|---|---|
| `plugin:hyprvx:zoom_step` | float | 1.1 | > 1; factor per wheel detent |

## State query (bookkeeping only)

`hyprctl hyprvx state` returns JSON:

```json
{"monitors":[{"monitor":"HVX-1","state":"DRIFT","workspace":1,"P":[0,0],"z":0.5}],
 "windows":[{"address":"0x…","workspace":1,"offset":[300,0]}],
 "build":{"form":"P","patchset":"…","hyprland":"efb5099…"}}
```

**Rule (Constitution II)**: replies and the state query are never pass conditions. Tests assert on captures, client
logs and `hyprctl clients`.
