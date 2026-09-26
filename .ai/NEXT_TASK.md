# HyprVX Next Task

## Phase 2A.4

Continue Hyprland 0.56.2 socket2 event discovery using captured probe output.
Only add typed schemas for event formats that have been observed and recorded.

Keep the work isolated from the production Hyprland session.

## Required behavior

Review observed event payloads and compatibility before selecting the next
bounded parser extension. Keep event handling event-driven and avoid polling.

## Pass criteria

- every new typed schema is backed by captured payload examples
- unknown event names and payloads remain preserved
- changes have automated coverage
- no production Hyprland or Caelestia configuration changes
- no compositor reload

## Not yet

Do not add typed schemas for uncaptured event formats. Do not add a native
plugin or production integration until compatibility, failure containment and
rollback are defined.
