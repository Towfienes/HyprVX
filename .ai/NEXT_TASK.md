# HyprVX Next Task

## Phase 2A.2

Continue Hyprland discovery using the Phase 2A.0 event probe, informed by
captured event output and compatibility review.

Keep the work isolated from the production Hyprland session.

## Required behavior

Review observed events and Hyprland compatibility before selecting the next
bounded implementation step. Keep event handling event-driven and avoid polling.

## Pass criteria

- the next step is defined from actual probe observations
- changes have automated coverage
- no production Hyprland or Caelestia configuration changes
- no compositor reload

## Not yet

Do not add a native plugin or production integration until compatibility,
failure containment and rollback are defined.
