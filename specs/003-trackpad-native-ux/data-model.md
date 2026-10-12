# Data Model: Trackpad-Native Drift UX

Extends `specs/002-multi-window-drift-mvp/data-model.md`. Screen geometry is global logical px; times are event
milliseconds. All state lives in compositor memory for the session. The pure part is `hyprvx_core` `GestureModel`
(`contracts/gesture-model.md`); the policy adapter owns one instance of each entity below.

## GestureSession (at most one open at a time)

| Field | Type | Rules |
|---|---|---|
| `kind` | `Scroll` \| `Swipe` \| `Pinch` | from the first event |
| `fingers` | unsigned | 2 for finger scrolls; from `begin` for gestures |
| `target` | `PassThrough` \| `Ignore` \| `Pan` \| `Zoom` | `decideTarget` at begin; never changes |
| `monitor` | monitor id | the monitor under the pointer at begin |
| `workspace` | workspace id | the monitor's scene workspace at begin |
| `startCamera` | `CameraState` | scene camera at begin (cancel restores it) |
| `anchor` | screen point | pointer at begin (pinch anchor) |
| `velocity` | `ReleaseVelocity` | pan samples (Pan target only) |
| `live` | bool | true while the monitor is still in DRIFT on the same workspace; false stops all camera effects |

```
idle ──begin (finger scroll Begin | swipe/pinch begin)──► open(target)
open ──update──► open                  (Pan/Zoom: camera change only if live)
open ──end (scroll End | gesture end)──► idle   (Pan + live + not cancelled → maybe Glide)
open ──cancelled end──► idle           (Pan/Zoom + live → camera = startCamera)
open ──Drift off | panic | workspace change | monitor removed──► open with live = false (consumption continues)
```

- `PassThrough` sessions are only tracked so that the sequence is not re-decided mid-way; their events are never
  cancelled.
- A Feature 002 drag beginning while a session is open closes the session's camera effect (`live = false`).

## Glide (at most one; belongs to a monitor)

| Field | Type | Rules |
|---|---|---|
| `monitor`, `workspace` | ids | from the session that started it |
| `glide` | `Glide` | started with the release velocity and the current `GlideConfig` |
| `lastStep` | steady-clock time | set at start; each `render.preChecks` of the monitor steps by now − lastStep |

- Stops on: any axis event, gesture begin, button, key, Drift off/toggle/panic, workspace change on its monitor,
  monitor removal, plugin unload, or `Glide::active() == false`.
- Each step applies `panByScreen(C, displacement)` to the monitor's scene camera through `DriftModel::pan`.

## TrackpadSettings (read from config values on use)

| Field | Key | Default | Validation |
|---|---|---|---|
| `panFactor` | `plugin:hyprvx:pan_factor` | 1.0 | finite > 0, else default + one log line |
| `glide.strength` | `plugin:hyprvx:glide` | 0.5 | finite in [0, 1], else default + one log line |
| `glide.minSpeed` | `plugin:hyprvx:glide_min_speed` | 100 | finite ≥ 0, else default + one log line |
| `touchpadFactor` | `input:touchpad:scroll_factor` | Hyprland's | read as is; ≤ 0 or non-finite → 1.0 |

## Relationships

- `GestureSession.startCamera` and every camera change go through `DriftModel` (`setCamera` for pinch and cancel,
  `pan` for scroll/swipe/glide), so per-workspace cameras (Feature 002) stay the single store.
- Window offsets `O(w)` are never touched by trackpad input.
