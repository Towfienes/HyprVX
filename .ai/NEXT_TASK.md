# HyprVX Next Task

## Phase 2C

Design a safe reconciliation boundary between read-only RuntimeMirror state
and HyprVX's spatial world model.

Do not mutate `hyprvx_core::World` until identity, startup partial-state
behavior, ownership changes, and recovery are specified and tested.

## Required behavior

Keep compositor wire formats and socket handling outside `hyprvx_core`. Use
captured events and the existing RuntimeBridge for integration tests.

## Pass criteria

- runtime window identity remains the Hyprland address until a stable mapping is designed
- startup with partial mirror state and window close/reopen are reconciled safely
- core behavior remains compositor-agnostic
- changes have automated coverage
- one-active-output policy remains the target
- monitor hotplug schemas remain deferred until hardware verification
- no production Hyprland or Caelestia configuration changes
- no compositor reload

## Not yet

Do not add monitor hotplug schemas without verified payloads. Do not add a
native plugin or production integration until compatibility, failure
containment and rollback are defined. Preserve read-only behavior until the
reconciliation design is approved.
