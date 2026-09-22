# Islands

## Definition

An island is a spatial group of windows.

It is not:
- a workspace
- a monitor
- a zone
- a nested compositor

## Coordinates

Island:
world position

Member window:
local position relative to island

Moving an island changes its world transform while preserving member local geometry.

## Free windows

Free windows remain valid at all times.

Opening a new application does not automatically create or join an island.

## Join

Default interaction:

drag near island
-> magnetic preview
-> short intentional dwell
-> release
-> join

Fast pass-through must not join.

## Detach

drag member beyond island boundary
-> resistance or hysteresis
-> detach preview
-> release
-> free window

## Layouts

V1:
- free layout

Later:
- tiled
- stack

## Merge

Island-to-island merge is a later feature and must require explicit intent.

No automatic collision-based merge.
