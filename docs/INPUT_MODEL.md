# Input Model

All physical input is translated into semantic canvas actions.

TrackpadAdapter
MouseAdapter
KeyboardAdapter
        |
        v
Canvas Actions
        |
        +-- Pan
        +-- Zoom
        +-- Navigate
        +-- MoveWindow
        +-- MoveIsland
        +-- GoHome
        +-- GoZone

## Trackpad goals

Trackpad interaction should feel continuous and direct.

High-frequency raw events must be accumulated and consumed at frame cadence rather than converted into shell commands one by one.

## Mouse goals

Primary ideas:
- MMB drag pans canvas
- optional outer-edge pan
- internal monitor boundaries do not trigger edge pan
- modifier plus wheel controls zoom when available

## Keyboard goals

Keyboard must allow complete navigation without a pointer.

Directional navigation scoring should consider:
- direction angle
- distance
- overlap
- island relationship

Nearest Euclidean distance alone is insufficient.

## Profiles

Laptop profile:
trackpad first

Desktop profile:
mouse and keyboard first

Both control the same underlying world.
