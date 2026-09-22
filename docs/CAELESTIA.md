# Caelestia Integration

Caelestia is an optional presentation and control layer.

It must not own the spatial world engine.

## Planned UI

- current zone indicator
- minimap
- spatial search
- overview
- island controls
- settings
- diagnostics

## Screen-space

The following remain screen-space:
- bar
- launcher
- notifications
- OSD
- lock UI
- minimap
- overview UI

Normal managed applications normally exist in world-space.

## Minimap

Default minimap renders metadata:
- rectangles
- app identity
- island boundaries
- camera viewport

Live application thumbnails are optional and disabled by default.

## Search

Search may index:
- app identity
- title
- island
- zone
- project metadata

Search results trigger semantic camera navigation rather than manual window movement.
