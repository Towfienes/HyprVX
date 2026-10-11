# Contract: Dev-only camera control (Feature 001)

**Contract version**: 1.1 (2026-10-11; T038: transport, `state` shape and build flag made explicit. 1.0 = the
unversioned table frozen at `966e6b6`, unchanged below)

Compiled only when the build defines `HYPRVX_DEV`. Usable only in nested sessions: the harness refuses to send
these commands to any instance that fails identity re-verification. It is **not** the product interface; the
plugin controller (feature 002) replaces it.

| Dispatcher | Args | Effect | Errors |
|---|---|---|---|
| `hyprvx-dev:camera` | `<Px> <Py> <z>` (monitor = focused) | Set camera exactly | non-finite / out-of-range ⇒ `err:invalid` and no change |
| `hyprvx-dev:camera-mon` | `<monitor> <Px> <Py> <z>` | Same for a named monitor | unknown monitor ⇒ `err:monitor` |
| `hyprvx-dev:panic` | — | All cameras → identity, scene disabled | never fails |
| `hyprvx-dev:state` | — | Returns JSON `{monitor, P, z, revision, passElementsAdded}` | — |
| `hyprvx-dev:full-damage` | — | Damage all outputs (reference frame for T-STALE) | — |

## Build and transport (1.1)
- **Build:** configure Hyprland with `-DHYPRVX_DEV=ON` (the patch adds this CMake option, default `OFF`). Without
  it, none of the commands below exist. Only the `patched` variant is built with it (T013).
- **Transport A, any config parser (preferred for tools):** `hyprctl hyprvx-dev <command> [args]`, where
  `<command>` is the dispatcher name without the `hyprvx-dev:` prefix (`camera`, `camera-mon`, `panic`, `state`,
  `full-damage`). Reply: `ok`, `err:invalid` or `err:monitor`; for `state` the JSON below.
- **Transport B, hyprlang config only:** `hyprctl dispatch hyprvx-dev:<command> [args]`. Reply: `ok` or the error
  string. `state` replies `ok` and writes the JSON to the Hyprland log. Under a Lua config, `hyprctl dispatch` is a
  Lua `hl.dispatch(...)` wrapper and does not reach these dispatchers; use transport A.
- **`state` JSON:** an array with one object per monitor:
  `[{"monitor":"WL-1","P":[x,y],"z":1,"identity":true,"revision":0,"passElementsAdded":0}]`.
  `passElementsAdded` counts camera hint elements added to render passes; it stays `0` while the camera is identity.
- **`panic`:** every monitor's camera is set to exact `identityFor(M)` (Feature 001 has no other scene state).

**Rule (Constitution II):** a dispatcher reply is **never** used as a pass condition. Tests assert on captures
and client logs.
