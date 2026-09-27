# Input Model

All physical input is translated into semantic canvas actions.

GestureNavigationAdapter
MouseNavigationAdapter
KeyboardNavigationAdapter
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

`RuntimeNavigationActionDispatcher` validates action ordering and coalesces
high-frequency direct-pan deltas until the end of each input event batch. A
pan gesture contributes at most one history transaction. If navigation
interrupts a direct gesture, its pending delta and transaction are canceled;
a late pan-end is ignored. Keyboard pan uses held-key state and the existing
delta-time-aware camera motion model.

The runtime camera controller changes only the HyprVX view. It does not focus
or move compositor windows and does not switch compositor workspaces.

## Trackpad goals

Trackpad interaction should feel continuous and direct.

High-frequency raw events must be accumulated and consumed at frame cadence rather than converted into shell commands one by one.
The SDL simulator keeps SDL key/button decoding in
`SdlNavigationInputAdapter`; keyboard and mouse policy lives in device-neutral
adapters. Middle-button drag, keyboard movement, wheel zoom, and window click
selection all pass through semantic actions and the shared dispatcher. Three-
finger pan, pinch, and four-finger directional swipe have testable adapters
that need no physical trackpad. A continuous pan gesture creates at most one
camera-history entry. User input interrupts active camera flights; a new
explicit destination replaces the previous flight.

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

Physical trackpad event acquisition must adapt into the same semantic command
layer rather than a separate camera path.

The current gesture adapter uses normalized deltas and configurable
sensitivity/threshold values; device-specific libinput/libgesture acquisition
is deferred. The simulator remains the only active physical-input integration.

The generic action currently does not carry a focused-workspace hint into
Home dispatch. Callers needing that fallback can call
`RuntimeCameraController::home` directly with the workspace context; the
simulator dispatcher uses the controller's focused projected window, known
content, and origin fallbacks.

## Phase 3 nested native input

The nested Wayland overlay maps pointer, XKB keyboard, and optional pointer
gesture protocol events into the same semantic adapters. Passive mode passes
input to ordinary applications; interactive mode accepts navigation input.
Seat capability loss and pointer/keyboard leave cancel transient input.
Touchpad gesture delivery depends on the nested compositor advertising the
client gesture protocol. No host input device is opened.
