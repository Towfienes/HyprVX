# HyprVX Next Task

## Phase 2D

Design persistence and reconnect recovery for the one-way runtime spatial
projection before adding user-driven camera or grouping controls.

## Required behavior

Keep compositor wire formats and socket handling outside `hyprvx_core`. Use
captured events and the existing RuntimeBridge and SpatialProjector for
integration tests.

## Pass criteria

- define whether projected layout state survives process restart or is rebuilt each epoch
- specify reset/reconnect behavior for projector IDs, workspace regions, and placement slots
- preserve read-only behavior and one-way projection
- one-active-output policy remains the target
- monitor hotplug schemas remain deferred until hardware verification
- no production Hyprland or Caelestia configuration changes
- no compositor reload

## Not yet

Do not add monitor hotplug schemas without verified payloads. Do not add a
native plugin or production integration until compatibility, failure
containment and rollback are defined. Do not add camera control or compositor
mutation until persistence and reconnect policy are specified.
