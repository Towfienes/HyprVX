# Multi-monitor Model

## V1 semantics

One shared world.

Each active output has its own camera.

Output A -> Camera A
Output B -> Camera B

Windows exist once in world space.

## Edge pan

A boundary between two adjacent physical outputs is not an edge-pan boundary.

Only an outer desktop boundary may trigger mouse edge-pan.

## Required tests

- hotplug
- unplug
- output disable and re-enable
- different refresh rates
- different scales
- rotation
- HDR
- VRR
- output removal during animation

## Camera persistence

When an output disappears, active render state should be destroyed.

A lightweight persisted camera state may remain for later restoration.

## Later

Optional linked cameras may allow multiple outputs to behave as one connected viewport.
