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
