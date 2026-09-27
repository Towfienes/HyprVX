# Input Model

All physical input is translated into semantic canvas actions.

TrackpadAdapter
MouseAdapter
KeyboardAdapter
        |
        v
Semantic Camera Commands
        |
        +-- PanBegin / PanUpdate / PanEnd
        +-- GotoWindow / GotoWorkspace / GotoView
        +-- FocusDirection
        +-- HistoryBack / HistoryForward
        +-- Home
        |
        v
RuntimeCameraController -> Camera / CameraMotion / CameraHistory

The runtime camera controller changes only the HyprVX view. It does not focus
or move compositor windows and does not switch compositor workspaces.

## Trackpad goals

Trackpad interaction should feel continuous and direct.

High-frequency raw events must be accumulated and consumed at frame cadence rather than converted into shell commands one by one.
The SDL simulator maps middle-button drag and keyboard pan to semantic pan
updates. A continuous pan gesture creates at most one camera-history entry.
User input interrupts active camera flights; a new explicit destination
replaces the previous flight.

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

HyprVX spatial selection is separate from Hyprland keyboard focus. Observed
compositor focus initializes selection while navigation is idle; after a user
navigation action, the HyprVX selection owns directional navigation until the
adapter explicitly resets ownership. No compositor command is emitted.

Future three-finger pan, pinch zoom, and four-finger directional gestures must
adapt into the same semantic command layer rather than a separate camera path.
