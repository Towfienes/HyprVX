# Data Model: DriftWM Window Behavior (Feature 004)

All state is owned by HyprVX (master plan §C5) and lives for the compositor session. Hyprland's window model is read
only.

| Entity | Fields | Owner | Rules |
|---|---|---|---|
| World box | window id, `L(w)` (Hyprland, read), `O(w)` (Feature 002 offset), size | derived each use | box = `L + O`; pinned windows excluded from snap, groups and navigation |
| Drag (extended from Feature 002) | kind (Pan/Move), monitor, start screen point, members `[{window, startOffset}]`, `SnapState` | `DriftModel` | Move with one member (Super + drag) or a group (Shift + Super + drag, members = `groupOf`); cancel restores every start offset; ends on button up, Drift off, panic, monitor loss, workspace change |
| Snap state | per axis: attached, target window, snapped coordinate | inside the drag | `window-behavior.md` `snapMove`; cleared when the drag ends |
| Group | ordered window ids | derived (`groupOf`) | facing edges at gap ± 1 world px and overlapping on the other axis, transitive |
| Pin | window → screen top-left (monitor-local logical) | policy (mirror) + mechanism | set by pin, cleared by unpin or window close; kept across Drift off/on and panic; mechanism draws identity outside Drift or under fullscreen |
| Bookmarks | per workspace: slots 1–4 → optional camera | policy | store overwrites; recall of empty slot does nothing |
| View history | per workspace: V1 `CameraHistory` (capacity 32) | policy | jumps record the previous camera; back/forward move through it; a new jump clears forward |
| Settings | snap, snap_threshold, snap_gap, snap_release, frame_margin | config values | invalid → default with one log line per key |

## State transitions

- **Drag**: Idle → (Super / Shift + Super + left press on a non-pinned scene window in DRIFT) Moving → (motion)
  Moving with `snapMove` applied to all members → (release) Idle with offsets kept, or (cancel) Idle with offsets
  restored.
- **Snap (per axis, inside Moving)**: Free → (facing edge within threshold/zoom) Attached(target) → (unsnapped
  candidate more than `release` from the snapped coordinate, or target gone) Free.
- **Pin**: Unpinned → (pin) Pinned(screen) → (unpin) Unpinned with `O(w)` set to keep the drawn top-left; window close
  drops it.
- **Camera jump**: any → (focus-reveal, center, frame, home, bookmark recall) new camera, previous recorded;
  history back/forward moves along the record without recording.
