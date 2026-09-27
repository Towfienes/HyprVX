# Recovery and Rollback

## Principle

HyprVX OFF must leave a usable normal Hyprland desktop.

## Existing pre-development protection

Before project implementation:
- configuration archive created
- SHA256 verified
- readonly Btrfs root snapshot created
- readonly Btrfs home snapshot created

## Development protection

Experimental plugin development:
nested Hyprland only

Production integration should be isolated in a removable HyprVX config include.

## Runtime recovery goals

Provide commands equivalent to:
- disable engine
- reset camera
- bring managed windows back into a visible safe region
- ignore persisted state
- start safe mode

## State

Phase 2D persists HyprVX-owned spatial layout in versioned JSON at
`~/.local/state/hyprvx/state.json` by default. The storage API accepts an
explicit path for tests or alternate installations. RuntimeMirror remains
compositor truth for live-window existence; saved windows stay dormant until
the current mirror observes them. The saved placement policy remains active
after restore so new workspace regions and placements continue without overlap.

Restore first uses same-compositor signature plus canonical address, unless a
new `openwindow` was seen in the current connection. Known class/workspace
conflicts reject that match. When the compositor epoch differs, recovery only
uses an exact class/title/workspace fingerprint if it uniquely identifies one
saved and one live window. Ambiguous candidates receive fresh layout. Titles do
not invalidate a strong same-epoch address match. Socket2 has no stable
application-instance ID, so a missed close/open sequence followed by address
reuse with indistinguishable metadata remains unresolved.

Recommended pattern:
write temporary state
-> fsync where appropriate
-> atomic rename

Keep last-known-good state for recovery. State dirty tracking is based on
spatial projector revisions. Callers can use a deterministic debounce clock or
explicitly save on graceful shutdown. Focus and title updates do not trigger
disk writes. No background polling or automatic service startup is used.

`RuntimeBridge` begins, ends, and restarts explicit connection epochs. EOF ends
the current epoch and clears transient mirror/tombstone state. The caller may
begin another epoch; there is no automatic reconnect or retry loop. The
projector separately ends its active epoch by removing live World entries while
retaining dormant geometry and placement counters for observation-driven
recovery.

## Crash behavior

Daemon crash should not kill Hyprland.

Native plugin crashes are higher risk and must be contained through development policy and compatibility checks.
