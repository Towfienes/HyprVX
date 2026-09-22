# HyprVX Engineering Rules

## Safety

- Never modify production Hyprland configuration without a reversible integration file.
- Never require a logout or compositor restart unless explicitly necessary.
- Experimental native plugin work belongs in nested Hyprland.
- Before risky host testing, confirm current work is saved.
- Every destructive command must have a documented rollback.
- State files must be written atomically.

## Architecture

- Caelestia must not implement the hot camera loop.
- Do not move every window through shell commands every frame.
- Do not use hyprctl as a rendering or animation transport.
- Separate world model from compositor integration.
- Separate input adapters from semantic canvas actions.
- Keep persistence outside the render hot path.
- Avoid hard dependencies between UI and engine.

## Performance

- No busy loops.
- No cursor polling when pointer events exist.
- No window polling when compositor events exist.
- Coalesce high-frequency input before visual updates.
- Use frame timing and delta time for animations.
- Hidden UI must not continuously animate.
- Live thumbnails are opt-in, never the default.
- Battery mode may reduce cosmetic effects but must not make interaction sluggish.
- A measurable idle CPU regression must be investigated before adding features.

## UX

- New windows default to free windows.
- Joining an island must be intentional.
- Detaching from an island must be reversible and predictable.
- Do not auto-merge islands in V1.
- Do not implement nested islands in V1.
- Avoid modal management modes.
- Spatial navigation should feel directional, not merely distance-based.
- Always provide Home and recovery navigation.

## Third-party code

- Inspiration is allowed.
- Source copying requires a recorded license audit.
- Track every reused algorithm or code fragment in docs/THIRD_PARTY.md.
- Do not assume compatible licenses.
