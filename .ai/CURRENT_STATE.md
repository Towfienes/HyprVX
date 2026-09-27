# HyprVX Current State

## Phase

Phase 2B complete - read-only event-driven Hyprland runtime bridge

## Completed

- project concept and architecture defined
- production Hyprland/Caelestia configuration backed up
- SHA256 backup verification completed
- readonly Btrfs root snapshot created
- readonly Btrfs home snapshot created
- initial real-workload performance baseline captured
- project brain v1 created
- toolchain audit completed
- implementation stack selected: C++20 core + GTest
- Vec2 implemented
- Rect implemented
- Camera implemented
- free window model implemented
- Island implemented
- World implemented
- headless simulator implemented
- 12 deterministic unit tests passing

## Phase 1A validation

Build:
PASS

Unit tests:
12 / 12 PASS

Simulator:
PASS

Validated:
- negative world coordinates
- large world coordinates
- world/screen round trip
- cursor-anchor-preserving zoom math
- island local-to-world transform
- island movement preserving local member geometry
- island world bounds
- free window storage
- island storage

## Current host

- CachyOS
- Hyprland 0.56.2
- Wayland
- Fish shell
- Btrfs

## Production safety

No HyprVX compositor plugin is loaded.

No production Hyprland or Caelestia configuration has been modified.

## Phase 1B.1 validation

Build:
PASS

Unit tests:
20 / 20 PASS

Camera motion:
PASS

Validated:
- frame-rate-independent manual motion at 60 / 120 / 210 Hz
- frame-rate-independent inertia
- frame-rate-independent target flight
- no target overshoot
- normalized pan input
- velocity limiting
- zoom clamping
- cursor-anchor-preserving zoom
- invalid delta rejection

Observed simulator frame-rate delta 60 -> 210 Hz:
0.000000

## Phase 2A.1 validation

Build:
PASS

Full test suite:
104 / 104 PASS

Validated event probe behavior:
- explicit runtime socket discovery and `/tmp/hypr` fallback
- candidate ordering, duplicate removal and actionable errors
- fragmented and batched event lines
- CRLF, invalid UTF-8 and trailing partial event handling
- typed C++ parsing for the eight observed Hyprland 0.56.2 event formats
- unknown event preservation and malformed event rejection
- Hyprland IPC parser remains in a separate adapter library, outside hyprvx_core

Production Hyprland and Caelestia configuration remain unmodified.

## Phase 2A.3 validation

Build:
PASS

Full test suite:
112 / 112 PASS

Validated lifecycle events:
- openwindow fields split at the first three commas, preserving commas in titles
- closewindow address parsing
- timestamp-prefixed lifecycle events
- malformed openwindow payload remains an UnknownEvent with original payload
- urgent remains UnknownEvent because its schema has not been captured

Hyprland-specific parsing remains isolated in the IPC adapter library.

## Phase 2A.5 validation

Build:
PASS

Full test suite:
124 / 124 PASS

Validated captured events:
- movewindow and movewindowv2
- createworkspace and createworkspacev2
- destroyworkspace and destroyworkspacev2
- v2 workspace names preserve commas after the required leading fields
- malformed known workspace/move events remain UnknownEvent
- uncaptured monitoraddedv2 remains UnknownEvent

Timestamp-prefixed probe lines continue to parse.

## Phase 2B validation

Build:
PASS

Full test suite:
179 / 179 PASS

Validated:
- SemanticEventAdapter maps structured events and suppresses captured legacy/v2 duplicates
- RuntimeMirror safely tracks partial windows, workspace state and focus
- blocking socket2 reader frames split, combined, CRLF and trailing partial input
- event lines are capped at 64 KiB; oversize unterminated frames fail safely
- runtime socket path is preferred, with `/tmp/hypr` fallback when needed
- `hyprvx-ipc-watch --file` replay uses the same parser, adapter and mirror path
- read-only live socket smoke test connected and waited for events
- address-first `changefloatingmode` format and empty activewindowv2 focus-clear event
- malformed recognized payloads are preserved as UnknownEvent
- canonical hex window identity, merge-preserved out-of-order metadata, and bounded close tombstones
- tombstoned late metadata is suppressed from mirror state and semantic watch output
- mirror reset clears state for a new connection epoch
- runtime state contains metadata only; socket2 geometry is not inferred

Runtime integration stays outside `hyprvx_core`; it does not mutate `World`.
Runtime policy targets one active output, with external DP taking exclusive
priority while connected. Monitor hotplug typing remains deferred pending
hardware verification. Production Hyprland configuration and Caelestia remain
unmodified.

## Next

Phase 2C - define a safe, explicit reconciliation design before connecting
read-only runtime observations to the spatial world model.
