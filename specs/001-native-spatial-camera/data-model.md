# Data Model: Native Spatial Camera

All coordinates are in Hyprland **global logical** units unless stated otherwise. These entities live in
compositor memory (the patch + later the plugin). No persistence in this feature.

## Camera
| Field | Type | Rules |
|---|---|---|
| `origin` (`P`) | `Vector2D` (double) | world point mapped to the monitor's top-left; finite |
| `zoom` (`z`) | double | finite, `z_min ≤ z ≤ z_max` (MVP 0.1 … 2.0) |
| `revision` | uint64 | incremented on every change; drives full-output damage |
- **Identity:** `P = M` (monitor logical origin), `z = 1`.
- Owned per (monitor, scene). In Feature 001 it is per monitor, and the scene is the active workspace.
- Invalid assignment ⇒ rejected (state unchanged) and logged.

## WindowWorldState
| Field | Type | Rules |
|---|---|---|
| `offset` (`O`) | `Vector2D` | finite; **zero in Feature 001** |
| `pinnedToScreen` | bool | false in Feature 001 (feature 004) |
- `W(w) = L(w) + O(w)`, where `L(w)` is Hyprland's logical box. Drift never writes `L`.
- Lifetime: created on window map, destroyed on unmap. Later features persist it keyed by window identity.

## DriftScene
| Field | Type | Rules |
|---|---|---|
| `workspaces` | set of workspace IDs | Feature 001: {active workspace of the monitor}. Unified world (feature 007): all non-special workspaces |
| `enabled` | bool | false ⇒ identity path, no pass elements |
- Predicate `isDriftVisible(window, monitor)` = `enabled ∧ window.workspace ∈ workspaces ∧ visibleWorldRect(C) ∩ W(w) ≠ ∅`.

## Mode state machine
```
OFF ──enable──► ENTERING ──validated──► DRIFT ──disable|panic|invalid|monitor-removed──► EXITING ──► OFF
                    └──validation failed──► OFF (+notification)
```
- All transitions are idempotent. `panic` from any state → OFF within one frame (camera = identity, scene disabled).
- Feature 001 exercises only `camera set` and `panic` through the dev control. The full FSM is feature 002.
