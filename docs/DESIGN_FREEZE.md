# HyprVX Design Freeze v0.1

HyprVX provides a continuous spatial desktop for Hyprland.

## V1 world

World
- FreeWindow
- Island
  - Window
- Zone
- Camera per output

## V1 interaction goals

Trackpad:
- continuous spatial pan
- later pinch zoom
- directional navigation gestures

Mouse:
- middle-button canvas pan
- optional intelligent edge pan
- modifier plus wheel zoom when zoom is available

Keyboard:
- camera movement
- directional spatial focus
- named-zone jump
- camera history back and forward

## Island V1

An island:
- contains windows
- moves as one spatial unit
- supports local member coordinates
- starts with free layout
- may later support tiled layout

Join:
- drag window into magnetic region
- show preview
- require dwell or explicit modifier
- release to join

Detach:
- drag through island boundary
- use hysteresis to avoid accidental toggling
- preview detach state
- release to detach

Not in V1:
- nested islands
- automatic island merging
- AI arrangement
- permanent live thumbnails
- 3D space

## Performance requirement

A beautiful implementation that causes persistent CPU use, poor battery life, or input lag is considered a failed implementation.
