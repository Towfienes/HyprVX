# HyprVX Next Task

## Phase 2F

Define runtime input adapter boundaries and visual integration for persisted
Hyprland projections, retaining read-only compositor behavior.

Phase 2D final audit hardening updates schema-v1 unsigned 64-bit persistence to
canonical decimal strings, adds collision-safe corrupt snapshots and bounded
dirty-age debounce, and caps persisted geometry at 1e12 world units. Preserve
these constraints in future state-format changes.

Phase 2E provides `RuntimeCameraController` semantic navigation and routes the
standalone SDL simulator's pan, focus, history, zone, and Home controls through
it. Selection remains distinct from compositor focus; direct input cancels
camera flights; workspace/view jumps affect only the HyprVX camera; one active
output remains the target. Camera view persistence is deferred.

## Required behavior

Keep compositor wire formats and socket handling outside `hyprvx_core`. Use
captured events and the existing RuntimeBridge, SpatialProjector, and durable
state APIs for integration tests.

## Pass criteria

- preserve one-active-output behavior as the primary runtime path
- keep camera commands local to HyprVX; do not add compositor window-control IPC
- keep persistence writes event-driven and skip focus/title churn
- retain durable-state conservative matching and explicit recovery behavior
- one-active-output policy remains the target
- monitor hotplug schemas remain deferred until hardware verification
- no production Hyprland or Caelestia configuration changes
- no compositor reload

## Not yet

Do not add monitor hotplug schemas without verified payloads. Do not add a
native plugin or production integration until compatibility, failure
containment and rollback are defined. Do not add compositor mutation.
