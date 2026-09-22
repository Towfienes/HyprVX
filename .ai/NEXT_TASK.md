# HyprVX Next Task

## Immediate task

Freeze the initial project brain and make the first Git commit.

Then begin Phase 1A: core world model.

## Phase 1A requirements

Implement and test:

- Vec2 world coordinates
- Rect geometry
- Camera state
- viewport transform
- free window model
- island model
- local-to-world transforms

Do not connect to Hyprland yet.

## Pass criteria

- deterministic unit tests
- no compositor dependency
- camera math works with negative coordinates
- island movement preserves member local positions
- world coordinates support large distances without visible instability

## Not yet

Do not implement:
- Hyprland plugin
- real trackpad gestures
- true zoom
- Caelestia UI
- live thumbnails
