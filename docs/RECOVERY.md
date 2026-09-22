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

Persist state atomically.

Recommended pattern:
write temporary state
-> fsync where appropriate
-> atomic rename

Keep last-known-good state for recovery.

## Crash behavior

Daemon crash should not kill Hyprland.

Native plugin crashes are higher risk and must be contained through development policy and compatibility checks.
