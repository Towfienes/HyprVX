# HyprVX Architecture Decisions

## D001 - Hyprland remains the compositor

Status: accepted

HyprVX extends Hyprland rather than replacing it.

## D002 - Hybrid architecture

Status: accepted

Use separate layers:

- shared spatial/world logic
- daemon or service for state, persistence and control
- native Hyprland plugin for compositor hot-path capabilities when required
- Caelestia integration for optional UI

## D003 - Simulator before compositor integration

Status: accepted

World coordinates, camera transforms, island behavior and navigation must be testable without a real Hyprland session.

## D004 - Free windows are first-class

Status: accepted

Applications are not automatically forced into islands.

## D005 - Island and Zone are different concepts

Status: accepted

Island:
a group of windows.

Zone:
a named location or spatial region.

## D006 - One-level islands for V1

Status: accepted

No nested islands.

## D007 - Independent per-output cameras first

Status: accepted

All monitors observe the same world but may have different camera positions.

Linked camera behavior may be added later.

## D008 - Screen-space and world-space are separate

Status: accepted

Caelestia bar, launcher, notifications, lockscreen and other layer-shell UI remain screen-space.

Normal managed applications normally live in world-space.

## D009 - True zoom is not an MVP requirement

Status: accepted

Pan and world semantics must become stable before compositor-level true zoom.

True zoom is experimental until Hyprland render hooks and compatibility are proven stable.

## D010 - License is not frozen yet

Status: accepted

Project license will be selected after a third-party license audit.

## D011 - Read-only event-driven Hyprland runtime bridge

Status: accepted

Keep Hyprland wire parsing and socket access outside `hyprvx_core`. A semantic
adapter translates verified socket2 events into structured runtime events, and
an eventually consistent `RuntimeMirror` observes those events without mutating
`World`. The socket2 reader blocks for input; it does not poll Hyprland or issue
`hyprctl` requests.

The initial runtime policy targets one active output: the laptop output is
active alone, an external DP output takes over exclusively while connected,
and the laptop output resumes when it disconnects. Simultaneous multi-monitor
behavior is deferred. Typed monitor hotplug events remain deferred until their
payloads are verified on hardware.

## D012 - One-way runtime spatial projection

Status: accepted

`RuntimeMirror` is compositor observation state. The separate
`hyprvx_runtime_projection` integration layer reconciles it one-way into
`hyprvx_core::World`; there is no compositor control path in Phase 2C.
Projected geometry is owned by HyprVX and is never represented as geometry
reported by Hyprland. Workspace regions are spatial zones, not Islands, and
observed windows stay free windows by default. Window IDs are allocated
monotonically per projector epoch from canonical addresses and are removed on
close; workspace and placement slot assignments remain reserved for the epoch
to keep unrelated window lifecycle events from shifting existing geometry.
The primary runtime target remains one active output.
