# HyprVX Current State

## Phase

Phase 2A.1 complete - event probe and typed Hyprland event parser

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

## Next

Phase 2A.2 - define the next Hyprland discovery/integration slice after reviewing
the event probe output; do not modify production compositor configuration.
