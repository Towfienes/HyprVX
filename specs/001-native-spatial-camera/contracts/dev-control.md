# Contract: Dev-only camera control (Feature 001)

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

**Rule (Constitution II):** a dispatcher reply is **never** used as a pass condition. Tests assert on captures
and client logs.
