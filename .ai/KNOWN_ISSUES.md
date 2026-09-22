# HyprVX Known Issues and Open Questions

## Architecture questions

- Final shared-core language and build strategy must be confirmed after Hyprland plugin toolchain audit.
- True compositor-level zoom feasibility must be validated against the target Hyprland version and plugin APIs.
- Native plugin ABI compatibility across Hyprland versions requires a compatibility strategy.

## Input questions

- Exact gesture conflicts with current Caelestia and Hyprland binds must be audited.
- Nested Hyprland cannot perfectly reproduce every physical trackpad behavior.

## Multi-monitor questions

Need explicit tests for:
- mixed refresh rates
- mixed scale factors
- rotation
- monitor hotplug
- monitor removal while camera animation is active
- HDR output
- VRR output

## UX questions

Need user testing for:
- island join dwell time
- detach hysteresis
- camera inertia
- edge-pan dwell
- spatial navigation scoring

## Third-party

License audit is still required before code reuse from reference projects.
