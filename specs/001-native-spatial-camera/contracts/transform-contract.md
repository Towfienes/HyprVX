# Contract: Camera Transform (single source of truth)

Implemented once in `core/include/hyprvx/core/CameraMath.hpp` (header-only, no dependencies, permissive
license). It is vendored unchanged into the Hyprland patch series. Every subsystem calls these functions and
none re-derives them (Constitution IV).

## Symbols
- `M` — monitor logical origin
- `s` — monitor scale
- `T` — output transform
- `C = (P, z)` — camera
- `L(w)` — Hyprland logical box
- `O(w)` — world offset
- `W(w) = L(w) + O(w)` — world box

## Functions
```
forward(C, M, W)      = M + (W − P)·z                  // world → screen-logical
inverse(C, M, S)      = P + (S − M)/z                  // screen-logical → world
forwardBox(C, M, B)   = { forward(B.pos), B.size·z }
inverseBox(C, M, R)   = { inverse(R.pos), R.size/z }
visibleWorldRect(C,M,size_m) = { P, size_m / z }
anchorZoom(C, M, a, z') : W_a = inverse(C,M,a);  P' = W_a − (a − M)/z';  z' clamped
panByScreen(C, d)     : P' = P − d/z
surfaceLocal(C, M, S, W_surface_origin) = inverse(C,M,S) − W_surface_origin   // unscaled surface units
physical(S)           = T((S − M)·s)                   // existing Hyprland path; not reimplemented
isIdentity(C, M)      = (P == M) ∧ (z == 1)            // exact compare; set exactly by reset/panic
```

## Invariants (tests: `tests/unit/test_camera_math.cpp`, property-based with ≥ 10⁴ random cases)
1. `inverse(forward(W)) = W`, |error| ≤ 1e-9·max(1,|W|).
2. After `anchorZoom` at `a`, `forward(W_a)` = `a` within 1e-9.
3. `forwardBox` / `inverseBox` round-trip within 1e-9.
4. Identity ⇒ `forward(W) = W` exactly.
5. Rejection: non-finite `P` or `z`, `z ≤ 0`, or `z` out of `[z_min, z_max]` ⇒ the setter returns an error;
   the camera is unchanged.
6. Damage mapping: `screenDamage = roundOut(forwardBox(box)) expanded by 1 px`. This must contain every pixel
   the renderer touches for that box (native test T-STALE).

## Seam obligations (patch series)
| Seam | Must use |
|---|---|
| Window-pass render hint (PRE_WINDOWS…POST_WINDOWS) | TRANSLATE/SCALE equivalent to `forward` in monitor pixels; composed with any existing geometry modifier |
| `drawSurface` clip / `visibleRegion` / `clipBox` | `forwardBox` |
| `shouldRenderWindow` / `visibleOnMonitor` | `visibleWorldRect` ∩ `W(w)` |
| `damageSurface` / `damageWindow` / `damageBox` (window-owned) | `forwardBox` + roundOut + 1 px |
| `windowAt` / `windowSurfaceAt` / `surfaceLocalAt` | monitor = monitorAt(S); then `inverse` |
| Pointer focus / motion local coordinates | `surfaceLocal` |
| Drag delta | `Δ_screen / z` (from `inverse` differences) |
| Warp / constraint regions | `forward` of the world target / region |
| Layer-shell, lock, IME, cursor | **never** transformed |

## Tolerances for native tests
- Geometry: ± 2 px (logical, scale 1) / ± 2 physical px (scale 1.25).
- Surface-local pointer: ± 1 unit.
