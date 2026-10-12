# Contract: GestureModel (pure core)

**Contract version**: 1.0 (2026-10-12, Feature 003). Header `core/include/hyprvx/core/GestureModel.hpp`, source
`core/src/GestureModel.cpp`, tests `tests/unit/test_gesture_model.cpp`, namespace `hyprvx::drift`, library
`hyprvx_core` (C++20). Pure: no Hyprland types, no I/O, no clock reads, no heap allocation after construction, no
exceptions escape any function. Geometry is screen logical px (`cammath::Vec`); times are event milliseconds
(`uint32_t`, wrapping arithmetic). Research R3–R5.

## Target decision (R3, R4)

```cpp
enum class InputKind { Scroll, Swipe, Pinch };
enum class Target { PassThrough, Ignore, Pan, Zoom };

struct BeginContext {
    bool drift = false;              // the monitor under the pointer is in DRIFT
    bool layerUnderPointer = false;  // top/overlay layer surface or layer popup under the pointer
    bool fullscreenScene = false;    // the scene workspace has a fullscreen window
    bool dragActive = false;         // a Feature 002 Super + drag (pan or move) is in progress
    bool superHeld = false;
    bool windowUnderPointer = false; // hit test through the camera and offsets
};

[[nodiscard]] Target decideTarget(InputKind kind, unsigned fingers, const BeginContext& ctx) noexcept;
```

Rules, first match wins:

| # | Condition | Target |
|---|---|---|
| 1 | `!drift` | `PassThrough` |
| 2 | `kind != Scroll` and `fingers >= 4` | `PassThrough` |
| 3 | `kind != Scroll` and `fingers < 2` | `PassThrough` |
| 4 | `kind != Scroll` and `fingers == 3` and (`layerUnderPointer` or `fullscreenScene` or `dragActive`) | `Ignore` |
| 5 | `kind == Swipe` and `fingers == 3` | `Pan` |
| 6 | `kind == Pinch` and `fingers == 3` | `Zoom` |
| 7 | `layerUnderPointer` or `fullscreenScene` | `PassThrough` |
| 8 | `dragActive` | `Ignore` |
| 9 | `kind == Swipe` (2 fingers) | `PassThrough` |
| 10 | `superHeld` | `Pan` for `Scroll`, `Zoom` for `Pinch` |
| 11 | `windowUnderPointer` | `PassThrough` |
| 12 | otherwise (empty canvas) | `Pan` for `Scroll`, `Zoom` for `Pinch` |

A 3-finger gesture on a Drift monitor is never `PassThrough` (FR-010).

`Scroll` ignores `fingers` (a finger-source scroll is a two-finger scroll).

## Scroll sequence tracking (R3)

```cpp
enum class ScrollStep { Begin, Continue, End, Ignore };

class ScrollTracker {
public:
    // One finger-source axis event. axis: 0 = vertical, 1 = horizontal. delta == 0 is that axis's stop.
    [[nodiscard]] ScrollStep feed(int axis, double delta, std::uint32_t timeMs) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool open() const noexcept;
    static constexpr std::uint32_t kIdleMs = 200;
};
```

- Closed tracker: `delta != 0` → `Begin` (opens; the axis is marked moving); `delta == 0` → `Ignore`; a non-finite
  delta or an axis other than 0/1 → `Ignore`.
- Open tracker: if `timeMs − last > kIdleMs` (wrapping difference), the old sequence is over: the event is handled as
  on a closed tracker (`Begin` or `Ignore`). Otherwise `delta != 0` → `Continue` (marks the axis moving);
  `delta == 0` → marks that axis stopped; when no axis is moving any more → `End` (closes), else `Continue`.
- `last` is updated by every accepted event. A sequence whose stop never comes is closed by the next event after the
  idle time; no timer is involved, and such a sequence never glides.

## Release velocity (R5)

```cpp
class ReleaseVelocity {
public:
    static constexpr std::uint32_t kWindowMs = 100;
    static constexpr std::uint32_t kMaxRestMs = 50;
    static constexpr double kMaxSpeed = 6000.0; // px/s
    static constexpr std::size_t kCapacity = 32;

    void reset() noexcept;
    // A consumed pan sample: screen displacement of the content at event time. Non-finite deltas are ignored.
    // A sample with the same timeMs as the newest one is merged into it (deltas added).
    void add(std::uint32_t timeMs, cammath::Vec delta) noexcept;
    // Velocity in px/s at the end of the sequence (stop or swipe end at endTimeMs).
    [[nodiscard]] cammath::Vec estimate(std::uint32_t endTimeMs) const noexcept;
};
```

- Fixed-capacity ring (`kCapacity`); when full the oldest sample is dropped.
- `estimate`: no samples → 0. `tn` = newest sample time. If `endTimeMs − tn > kMaxRestMs` → 0. Window = samples with
  `tn − t ≤ kWindowMs`. Fewer than 2 in the window → 0. Else `v = (Σ deltas of the window except the oldest) /
  ((tn − t_oldest) / 1000)`. If `|v| > kMaxSpeed`, scale `v` to length `kMaxSpeed`.

## Glide (R5)

```cpp
struct GlideConfig {
    double strength = 0.5;   // [0, 1]; 0 disables
    double minSpeed = 100.0; // px/s
    double stopSpeed = 1.0;  // px/s
    double maxSpeed = 6000.0;
};

// λ for a strength: nullopt when strength <= 0 or not finite; else 2 + 10 * (1 - min(strength, 1)).
[[nodiscard]] std::optional<double> glideDamping(double strength) noexcept;

// Total displacement of a glide started at velocity v (the formula tests compare against):
// direction of v times (min(|v|, maxSpeed) - stopSpeed) / λ, or 0 if no glide starts.
[[nodiscard]] cammath::Vec expectedGlide(cammath::Vec v, const GlideConfig& cfg) noexcept;

class Glide {
public:
    // Starts when the config is valid (finite, minSpeed >= 0, stopSpeed > 0, maxSpeed > stopSpeed), damping exists
    // and |v| >= minSpeed; returns whether it started. Never throws: the config is validated before
    // hyprvx::CameraController is constructed or configured.
    [[nodiscard]] bool start(cammath::Vec velocity, const GlideConfig& cfg) noexcept;
    // Advances by dt seconds (dt <= 0 or non-finite → no movement) and returns the content displacement of this step.
    // When the speed falls below stopSpeed the glide ends (active() becomes false) after this step.
    [[nodiscard]] cammath::Vec step(double dtSeconds) noexcept;
    void stop() noexcept;
    [[nodiscard]] bool active() const noexcept;
};
```

- Implemented on V1 `hyprvx::CameraController` + `hyprvx::Camera` (Constitution VIII): damping = λ, pan input zero,
  `maxSpeed`, `stopSpeed`; the integrator camera starts at the origin with zoom 1 and each step returns the change of
  its position.
- Invariants (unit-tested): the sum of all steps until the glide ends equals `expectedGlide` within 0.5 px for any
  split of time into steps (single step of 10 s, 60 Hz, 144 Hz, random steps); the per-step speed never increases;
  `stop()` makes the next `step` return 0; a glide that did not start returns 0 from `step`.

## Pinch camera (R4)

```cpp
// Camera during a pinch: anchorZoom(start, monitorOrigin, anchor, start.zoom * scale, limits).
// scale non-finite or <= 0 → start unchanged.
[[nodiscard]] cammath::CameraState pinchCamera(const cammath::CameraState& start, cammath::Vec monitorOrigin,
                                               cammath::Vec anchor, double scale,
                                               cammath::ZoomLimits limits = {0.1, 2.0}) noexcept;
```

Uses only `CameraMath.hpp`; no second camera equation (Constitution IV). The world point under `anchor` in `start`
stays under `anchor` (transform contract invariant 3), also when the zoom is clamped.
