# Contract: Camera Transform 1.2 (delta over 1.1)

**Contract version**: 1.2 (2026-10-11, Feature 002). Base: `specs/001-native-spatial-camera/contracts/transform-contract.md`
1.1. Everything in 1.1 stays valid and unchanged; this delta adds the world offset `O(w)` (master plan §C1:
`W(w) = L(w) + O(w)`). `core/include/hyprvx/core/CameraMath.hpp` and its vendored copy in the patch series stay
identical (one equation, Constitution IV).

## Added functions

```cpp
namespace hyprvx::cammath {
// The camera that draws a window with world offset O at its logical position: forward(offsetCamera(c, O), M, L)
// equals forward(c, M, L + O). Origin shift only; the zoom is unchanged.
[[nodiscard]] constexpr CameraState offsetCamera(const CameraState& c, Vec O) noexcept;   // { c.origin − O, c.zoom }
// A screen-space delta expressed in world units: d / z (window move drag, floating resize drag).
[[nodiscard]] constexpr Vec worldDelta(const CameraState& c, Vec d) noexcept;             // { d.x / z, d.y / z }
}
```

## Added invariants (tests in `tests/unit/test_camera_math.cpp`, seeded property tests as in 1.1)

8. `forward(offsetCamera(c, O), M, L)` equals `forward(c, M, L + O)` within 1e-9 (relative to magnitude), for finite
   inputs; `inverse(offsetCamera(c, O), M, S) + O` equals `inverse(c, M, S)` within 1e-9.
9. `offsetCamera(c, {0, 0})` returns `c` exactly, so an identity camera with a zero offset is still identity and adds no
   render work (FR-003).
10. `forward(c, M, W + worldDelta(c, d)) − forward(c, M, W)` equals `d` within 1e-9 (a dragged window follows the
    pointer).

## Seam obligations added in 1.2

| Seam | Must use |
|---|---|
| Per-window render hint (window pass) | `cameraRenderModif(M, offsetCamera(C, O(w)))` while `C` is not identity or `O(w) ≠ 0` |
| Culling, damage, hit-test, surface-local, warp/constraint, decoration hit, popup placement | the 1.1 function on `L + O(w)`, i.e. through `offsetCamera` or by adding `O(w)` to the logical box before the call |
| Window move drag (policy) | `O(w) += worldDelta(C, Δscreen)` |
| Floating resize drag (mechanism) | size delta = `worldDelta(C, Δscreen)` |
| Layer-shell, lock, IME, cursor | never transformed, never offset |

Tolerances are unchanged (± 2 px geometry, ± 1 unit surface-local).
