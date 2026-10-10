# Contract: Camera Transform (single source of truth)

**Contract version**: 1.1 (2026-10-10; replan P3: exact identity, V1 compatibility, frozen C++ signatures)

Implemented once in `core/include/hyprvx/core/CameraMath.hpp` (header-only, no dependencies beyond the C++20
standard library, `SPDX-License-Identifier: BSD-3-Clause`, decision DV3-10). It is vendored unchanged into the
Hyprland patch series. Every subsystem calls these functions and none re-derives them (Constitution IV).

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

**Identity short-circuit (normative).** `forward`, `inverse`, `forwardBox`, `inverseBox` and `surfaceLocal`
MUST test `isIdentity(C, M)` first and, when true, return their input unchanged (for `surfaceLocal`:
`S − W_surface_origin`). Floating-point `M + (W − M)` is not exact: `1920 + (0.1 − 1920)·1 =
0.09999999999990905`. In addition, the compositor takes the stock path whenever the camera is identity (FR-002).

## C++ interface (frozen for Sprint 1; changes require a contract version bump)
```cpp
// core/include/hyprvx/core/CameraMath.hpp
// SPDX-License-Identifier: BSD-3-Clause
namespace hyprvx::cammath {

struct Vec  { double x = 0.0, y = 0.0; };
struct Box  { Vec pos; Vec size; };
struct CameraState { Vec origin; double zoom = 1.0; };    // P, z
struct ZoomLimits  { double min = 0.1; double max = 2.0; }; // MVP range; callers may pass others

enum class Error { None, NonFinite, NonPositiveZoom, ZoomOutOfRange };

[[nodiscard]] constexpr bool  isIdentity(const CameraState& c, Vec M) noexcept;
[[nodiscard]] constexpr Vec   forward(const CameraState& c, Vec M, Vec W) noexcept;
[[nodiscard]] constexpr Vec   inverse(const CameraState& c, Vec M, Vec S) noexcept;
[[nodiscard]] constexpr Box   forwardBox(const CameraState& c, Vec M, Box B) noexcept;
[[nodiscard]] constexpr Box   inverseBox(const CameraState& c, Vec M, Box R) noexcept;
[[nodiscard]] constexpr Box   visibleWorldRect(const CameraState& c, Vec monitorSize) noexcept;
[[nodiscard]] constexpr Vec   surfaceLocal(const CameraState& c, Vec M, Vec S, Vec surfaceOrigin) noexcept;
[[nodiscard]] Box             damageBox(const CameraState& c, Vec M, Box worldBox) noexcept; // roundOut + 1 px
[[nodiscard]] Error           validate(const CameraState& c, ZoomLimits limits) noexcept;
// zNew is clamped to limits; non-finite a or zNew ⇒ c returned unchanged.
[[nodiscard]] CameraState     anchorZoom(const CameraState& c, Vec M, Vec a, double zNew, ZoomLimits limits) noexcept;
[[nodiscard]] constexpr CameraState panByScreen(const CameraState& c, Vec d) noexcept;
[[nodiscard]] constexpr CameraState identityFor(Vec M) noexcept;                     // { M, 1.0 }, exact

} // namespace hyprvx::cammath
```
The header defines its own POD types so that it can be vendored into Hyprland without `hyprvx_core`. Adapters to
`hyprvx::Vec2`/`Rect` (core) and `Vector2D`/`CBox` (Hyprland) live at the call sites and contain no arithmetic
beyond field copies.

## V1 compatibility
- `hyprvx::Camera` (`core/src/Camera.cpp`) keeps its public API and viewport-local semantics. It delegates to
  `CameraMath` with `M = {0, 0}`.
- `Camera::setZoom` keeps its own validation (throws `std::invalid_argument`, no clamping).
  `setZoomAroundScreenPoint` delegates the anchor math with unbounded limits
  (`{std::numeric_limits<double>::min(), std::numeric_limits<double>::max()}`), so V1 behaviour is unchanged.
- The existing camera GTests and simulators must pass unchanged. A change of their expected values is a defect.

## Invariants (tests: `tests/unit/test_camera_math.cpp`, property-based with ≥ 10⁴ seeded random cases;
expected values come from an independent oracle such as `long double` or hand-computed dyadic values, not only
from forward/inverse agreement; the oracle lives in tests only and is never a second runtime implementation)
1. `inverse(forward(W)) = W`, |error| ≤ 1e-9·max(1,|W|).
2. After `anchorZoom` at `a`, `forward(W_a)` = `a` within 1e-9 (when `z'` is not clamped).
3. `forwardBox` / `inverseBox` round-trip within 1e-9.
4. Identity ⇒ `forward(W) = W` and `inverse(S) = S` **exactly** (short-circuit above), including non-zero `M`.
5. Rejection: non-finite `P` or `z`, `z ≤ 0`, or `z` out of `[z_min, z_max]` ⇒ `validate` returns an error;
   setters built on it leave the camera unchanged.
6. Damage mapping: `screenDamage = roundOut(forwardBox(box)) expanded by 1 px`. This must contain every pixel
   the renderer touches for that box (native tests T-STALE and T-STALE-STATIC).
7. V1 equivalence: for random `P`, `z`, `W`, `Camera{P,z}.worldToScreen(W) == forward({P,z}, {0,0}, W)` and the same
   for `screenToWorld`/`inverse`.

## Seam obligations (patch series)
| Seam | Must use |
|---|---|
| Window-pass render hint (PRE_WINDOWS…POST_WINDOWS) | TRANSLATE/SCALE equivalent to `forward` in monitor pixels; composed with any existing geometry modifier |
| `drawSurface` clip / `visibleRegion` / `clipBox` | `forwardBox` |
| `shouldRenderWindow` / `visibleOnMonitor` | `visibleWorldRect` ∩ `W(w)` |
| `damageSurface` / `damageWindow` / `damageBox` (window-owned) | `damageBox` (forwardBox + roundOut + 1 px) |
| `windowAt` / `windowSurfaceAt` / `surfaceLocalAt` | monitor = monitorAt(S); then `inverse` |
| Pointer focus / motion local coordinates | `surfaceLocal` |
| Decoration / corner hit selection (`checkInputOnDecos`) | `inverse` |
| Compositor move/resize drag while camera ≠ identity | blocked and logged in Feature 001 (FR-012); `Δ_screen / z` on `O(w)` in Feature 002 |
| Warp / constraint regions | `forward` of the world target / region |
| Layer-shell, lock, IME, cursor | **never** transformed |

## Tolerances for native tests
- Geometry: ± 2 px (logical, scale 1) / ± 2 physical px (scale 1.25).
- Surface-local pointer: ± 1 unit.
