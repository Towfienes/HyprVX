# HyprVX Next Task

## Phase 2F status

Runtime input boundaries and simulator integration are implemented. Keyboard,
mouse, and semantic gesture adapters share `RuntimeNavigationAction` and
`RuntimeNavigationActionDispatcher`; SDL decoding stays in the simulator.
Pan input is coalesced per event batch, stale gesture endings are ignored, and
click selection/Home respect HyprVX selection ownership. The simulator remains
read-only toward Hyprland.

Final validation is recorded in `.ai/CURRENT_STATE.md`. Physical trackpad
event acquisition and production bindings remain deferred.

The final audit fixed unsupported modifier key-up handling, ignored key-up for
unheld directions, and added SDL focus-loss cancellation of transient input
without resetting the compositor epoch. The complete suite now passes 285 /
285; see current state for the full validation record.

## Next

Review Phase 2F behavior and define the next milestone before adding physical
trackpad acquisition or compositor-facing integration. Preserve the project
constraints below.

## Required behavior

- keep Hyprland parsing and socket handling outside `hyprvx_core`
- maintain one active output as the runtime target
- keep camera actions local to HyprVX; add no compositor window-control IPC
- keep persistence writes event-driven and skip focus/title churn
- retain conservative restore matching and explicit recovery behavior
- defer monitor event schemas until payloads are verified
- do not modify production Hyprland or Caelestia configuration
- do not add a native plugin, auto-start service, or polling loop

Phase 2D persistence constraints remain: canonical decimal strings for
unsigned 64-bit state, collision-safe corrupt snapshots, bounded dirty-age
debounce, and persisted geometry limited to 1e12 world units.
