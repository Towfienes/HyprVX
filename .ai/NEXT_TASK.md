# HyprVX Next Task

# HyprVX Next Task

## Phase 3 completion state

The nested native prototype is implemented and validated. The system has a
nested-only harness, Wayland overlay, socket2 runtime projection, optional
read-only Hyprland event plugin, and semantic control bridge. Exact tested
build/test and nested results are recorded in `.ai/CURRENT_STATE.md` and
`docs/NATIVE_PHASE3.md`.

## Next engineering boundary

Investigate a Hyprland render/input API that can transform application content,
popups, and pointer hit testing together. Until such an API is available,
plugin transform activation remains refused and compositor zoom remains
deferred. Separately, validate live native pointer and keyboard input in the
nested session using a safe nested-only input source, and measure active frame
latency. Do not target the host compositor.

## Constraints

- keep `hyprvx_core` compositor-agnostic
- maintain one active output as the runtime target
- keep the RuntimeMirror -> SpatialProjection -> World direction one-way
- do not poll Hyprland or use `hyprctl` as a frame loop
- do not modify production Hyprland or Caelestia configuration
- never load the plugin into the host compositor
- preserve event-driven rendering and near-zero idle CPU
- defer monitor event schemas until payloads are verified
- keep persistence and recovery conservative and atomic
