#pragma once

// Feature 003 GestureModel (specs/003-trackpad-native-ux/contracts/gesture-model.md 1.0): the pure part of trackpad
// navigation in Drift Mode. Target decision at the start of a scroll sequence or gesture, scroll sequence tracking,
// release velocity, glide (on the V1 CameraController closed-form damping) and the pinch camera. No Hyprland types, no
// I/O, no clock reads, no heap allocation after construction, and no exception escapes. Geometry is screen logical px;
// times are event milliseconds with wrapping arithmetic.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Camera.hpp"
#include "CameraMath.hpp"
#include "CameraMotion.hpp"

namespace hyprvx::drift {

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

// The contract's ordered table (first match wins). A 3-finger gesture on a Drift monitor is never PassThrough.
[[nodiscard]] Target decideTarget(InputKind kind, unsigned fingers, const BeginContext& ctx) noexcept;

enum class ScrollStep { Begin, Continue, End, Ignore };

// Finger-source scroll sequences: begin on the first non-zero event, end when every axis that moved has sent its stop
// (delta == 0), or implicitly when the next event arrives more than kIdleMs after the previous one.
class ScrollTracker {
public:
    static constexpr std::uint32_t kIdleMs = 200;

    // axis: 0 = vertical, 1 = horizontal.
    [[nodiscard]] ScrollStep feed(int axis, double delta, std::uint32_t timeMs) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool open() const noexcept { return open_; }

private:
    [[nodiscard]] ScrollStep feedClosed(int axis, double delta, std::uint32_t timeMs) noexcept;

    bool                open_ = false;
    std::array<bool, 2> moving_{};
    std::uint32_t       last_ = 0;
};

// Release speed of a consumed pan from its last samples (event timestamps only; deterministic).
class ReleaseVelocity {
public:
    static constexpr std::uint32_t kWindowMs = 100;
    static constexpr std::uint32_t kMaxRestMs = 50;
    static constexpr double        kMaxSpeed = 6000.0; // px/s
    static constexpr std::size_t   kCapacity = 32;

    void reset() noexcept;
    // Screen displacement of the content at event time; non-finite deltas are ignored; a sample with the newest
    // sample's timestamp is merged into it.
    void add(std::uint32_t timeMs, cammath::Vec delta) noexcept;
    // Velocity (px/s) at the end of the sequence (stop or swipe end at endTimeMs).
    [[nodiscard]] cammath::Vec estimate(std::uint32_t endTimeMs) const noexcept;

private:
    struct Sample {
        std::uint32_t timeMs = 0;
        cammath::Vec  delta;
    };
    // Sample i (0 = oldest) of the ring.
    [[nodiscard]] const Sample& at(std::size_t i) const noexcept { return ring_[(head_ + kCapacity - count_ + i) % kCapacity]; }

    std::array<Sample, kCapacity> ring_{};
    std::size_t                   head_ = 0;  // next write position
    std::size_t                   count_ = 0;
};

struct GlideConfig {
    double strength = 0.5;   // [0, 1]; 0 disables
    double minSpeed = 100.0; // px/s
    double stopSpeed = 1.0;  // px/s
    double maxSpeed = 6000.0;
};

// Friction λ (1/s) for a strength: nullopt when strength <= 0 or not finite; else 2 + 10 * (1 - min(strength, 1)).
[[nodiscard]] std::optional<double> glideDamping(double strength) noexcept;

// Total displacement of a glide started at velocity v: direction of v times (min(|v|, maxSpeed) - stopSpeed) / λ, or 0
// if no glide starts.
[[nodiscard]] cammath::Vec expectedGlide(cammath::Vec v, const GlideConfig& cfg) noexcept;

// Exponentially decaying pan after a release, integrated by hyprvx::CameraController (exact closed form, so any split
// of time into steps gives the same path) on a screen-space hyprvx::Camera used only as an integrator.
class Glide {
public:
    Glide() noexcept = default;
    Glide(const Glide&) = delete;  // the controller holds a reference to camera_
    Glide& operator=(const Glide&) = delete;

    // Starts when the config is valid (finite, minSpeed >= 0, stopSpeed > 0, maxSpeed > stopSpeed), a damping exists
    // and min(|v|, maxSpeed) >= minSpeed. The config is validated before the controller is constructed.
    [[nodiscard]] bool start(cammath::Vec velocity, const GlideConfig& cfg) noexcept;
    // Advances by dt seconds (dt <= 0 or non-finite: no movement) and returns this step's content displacement. When
    // the speed falls below stopSpeed the glide ends after this step.
    [[nodiscard]] cammath::Vec step(double dtSeconds) noexcept;
    void stop() noexcept;
    [[nodiscard]] bool active() const noexcept { return active_; }

private:
    hyprvx::Camera                          camera_;
    std::optional<hyprvx::CameraController> controller_;
    bool                                    active_ = false;
};

// Camera during a pinch: anchorZoom(start, monitorOrigin, anchor, start.zoom * scale, limits); a non-finite scale or
// one <= 0 returns start unchanged. Only CameraMath is used (Constitution IV).
[[nodiscard]] cammath::CameraState pinchCamera(const cammath::CameraState& start, cammath::Vec monitorOrigin,
                                               cammath::Vec anchor, double scale,
                                               cammath::ZoomLimits limits = {0.1, 2.0}) noexcept;

} // namespace hyprvx::drift
