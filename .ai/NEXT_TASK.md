# HyprVX Next Task

## Phase 2E

Design the first user-driven camera/navigation integration over the persisted
one-way runtime spatial projection.

Phase 2D final audit hardening updates schema-v1 unsigned 64-bit persistence to
canonical decimal strings, adds collision-safe corrupt snapshots and bounded
dirty-age debounce, and caps persisted geometry at 1e12 world units. Preserve
these constraints in future state-format changes.

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
