# Data Model: Multi-Window Drift MVP (Option A)

Extends `specs/001-native-spatial-camera/data-model.md`. All coordinates are Hyprland **global logical** units. State
lives in compositor memory for the session (no persistence in this feature). The pure part is `hyprvx_core`
`DriftModel`; the plugin adapts it to Hyprland objects (R3).

## DriftSession (per monitor)

| Field | Type | Rules |
|---|---|---|
| `monitor` | monitor id | key |
| `state` | `OFF` \| `ENTERING` \| `DRIFT` \| `EXITING` | transitions below; starts `OFF` |
| `sceneWorkspace` | workspace id or none | the monitor's active workspace while `DRIFT` (Option A); none otherwise |

```
OFF ──enable──► ENTERING ──valid (R10)──► DRIFT ──disable | Super+Escape | panic | invalid camera | monitor removed──► EXITING ──► OFF
                   └── invalid ──► OFF + notification
```

- `enable` while `ENTERING` or `DRIFT`, and `disable`/`panic` while `OFF` or `EXITING`, are no-ops (idempotent).
- **Entering DRIFT**: load the scene workspace's camera into the monitor, and push the stored offsets of the scene's
  windows to the mechanism.
- **EXITING**: end any drag, set the monitor camera to identity, and push offset 0 for every window. The stored cameras
  and offsets stay in the model. The mechanism then has no non-identity state, which is FR-003 (stock path, no pass
  elements).
- **Panic**: from any state, every monitor goes OFF within one frame. Panic does not clear the stored offsets; the
  dev-control `panic` also resets them, as in Feature 001.

## WorkspaceCamera (per workspace)

| Field | Type | Rules |
|---|---|---|
| `workspace` | workspace id | key; regular and named workspaces; special workspaces never get a camera |
| `camera` | `CameraState{P, z}` | `validate` with limits [0.1, 2.0]; default `identityFor(M)` of the workspace's monitor |

- **Changed by**: pan (`panByScreen`), zoom (`anchorZoom` at the pointer), reset (identity), and the pan drag.
- **Workspace change**: on `workspace.active` while `DRIFT`, the monitor camera is set to the new workspace's camera
  (R2).
- An invalid result is rejected and the camera is unchanged.

## WindowOffset (per window)

| Field | Type | Rules |
|---|---|---|
| `window` | window identity (address) | key; removed on window close/destroy |
| `offset` | `Vec{x, y}` (`O(w)`) | finite; default 0 |

- `W(w) = L(w) + O(w)`. `L(w)` is Hyprland's and is never written.
- **Set by**: the move drag (`O += Δscreen / z`), spawn placement (R6), and override-redirect inheritance (R9).
- **Kept** when the window changes workspace (the offset belongs to the window); shown only while its workspace is a
  Drift scene.
- **Pushed to the mechanism** only for windows of a `DRIFT` scene; every other window has mechanism offset 0.

## Drag (at most one per seat)

| Field | Type | Rules |
|---|---|---|
| `kind` | `PAN` \| `MOVE` \| `RESIZE` | `PAN` on empty canvas, `MOVE` on a window (Super + left button), `RESIZE` = Hyprland border drag on a floating window (mechanism, R5) |
| `window` | window id or none | none for `PAN` |
| `startScreen` | `Vec` | pointer at press |
| `startOffset` / `startCamera` | `Vec` / `CameraState` | value at press, with Δ = S − startScreen. `MOVE`: `O = startOffset + Δ/z`. `PAN`: `P = panByScreen(startCamera, Δ).P`, i.e. `P = startP − Δ/z` |

- **Ends on**: button release, Drift leaving `DRIFT`, panic, the window closing, or the scene workspace changing. The
  end is always clean: no stuck grab, and the last applied value stays.

## Derived predicates

- `isSceneWindow(w, m)` = `session(m).state == DRIFT ∧ w.workspace == session(m).sceneWorkspace ∧ w mapped`.
- `mechanismOffset(w)` = `isSceneWindow(w, monitor(w)) ? O(w) : 0`.
- `spawnOffset(w, m)` = `centre(visibleWorldRect(C(m))) − centre(L(w))`.
- **Fullscreen**: a scene workspace with a fullscreen window is drawn with identity camera and offsets ignored
  (mechanism rule, R7).
