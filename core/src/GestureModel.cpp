#include "hyprvx/core/GestureModel.hpp"

#include <algorithm>
#include <cmath>

namespace hyprvx::drift {

namespace {

bool finite(cammath::Vec v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

double length(cammath::Vec v) noexcept {
    return std::hypot(v.x, v.y);
}

// Wrapping difference of event times (later - earlier).
std::uint32_t elapsed(std::uint32_t later, std::uint32_t earlier) noexcept {
    return static_cast<std::uint32_t>(later - earlier);
}

bool validGlideConfig(const GlideConfig& cfg) noexcept {
    return glideDamping(cfg.strength).has_value() && std::isfinite(cfg.minSpeed) && std::isfinite(cfg.stopSpeed) &&
        std::isfinite(cfg.maxSpeed) && cfg.minSpeed >= 0.0 && cfg.stopSpeed > 0.0 && cfg.maxSpeed > cfg.stopSpeed;
}

} // namespace

Target decideTarget(InputKind kind, unsigned fingers, const BeginContext& ctx) noexcept {
    const bool gesture = kind != InputKind::Scroll;
    const auto canvasTarget = kind == InputKind::Scroll ? Target::Pan : Target::Zoom;
    if (!ctx.drift)
        return Target::PassThrough;
    if (gesture && (fingers >= 4 || fingers < 2))
        return Target::PassThrough;
    if (gesture && fingers == 3) {
        // Never PassThrough on a Drift monitor (FR-010): the user's 3-finger bindings must not fire.
        if (ctx.layerUnderPointer || ctx.fullscreenScene || ctx.dragActive)
            return Target::Ignore;
        return kind == InputKind::Swipe ? Target::Pan : Target::Zoom;
    }
    if (ctx.layerUnderPointer || ctx.fullscreenScene)
        return Target::PassThrough;
    if (ctx.dragActive)
        return Target::Ignore;
    if (kind == InputKind::Swipe)
        return Target::PassThrough;
    if (ctx.superHeld)
        return canvasTarget;
    if (ctx.windowUnderPointer)
        return Target::PassThrough;
    return canvasTarget;
}

// ---- ScrollTracker -----------------------------------------------------------------------------------------------

ScrollStep ScrollTracker::feed(int axis, double delta, std::uint32_t timeMs) noexcept {
    if ((axis != 0 && axis != 1) || !std::isfinite(delta))
        return ScrollStep::Ignore;
    if (!open_)
        return feedClosed(axis, delta, timeMs);
    if (elapsed(timeMs, last_) > kIdleMs) {
        // The previous sequence never sent its stop; it is over.
        reset();
        return feedClosed(axis, delta, timeMs);
    }
    last_ = timeMs;
    if (delta != 0.0) {
        moving_[axis] = true;
        return ScrollStep::Continue;
    }
    moving_[axis] = false;
    if (!moving_[0] && !moving_[1]) {
        open_ = false;
        return ScrollStep::End;
    }
    return ScrollStep::Continue;
}

ScrollStep ScrollTracker::feedClosed(int axis, double delta, std::uint32_t timeMs) noexcept {
    if (delta == 0.0)
        return ScrollStep::Ignore;
    open_ = true;
    moving_ = {};
    moving_[axis] = true;
    last_ = timeMs;
    return ScrollStep::Begin;
}

void ScrollTracker::reset() noexcept {
    open_ = false;
    moving_ = {};
}

// ---- ReleaseVelocity ---------------------------------------------------------------------------------------------

void ReleaseVelocity::reset() noexcept {
    head_ = 0;
    count_ = 0;
}

void ReleaseVelocity::add(std::uint32_t timeMs, cammath::Vec delta) noexcept {
    if (!finite(delta))
        return;
    if (count_ > 0) {
        auto& newest = ring_[(head_ + kCapacity - 1) % kCapacity];
        if (newest.timeMs == timeMs) {
            newest.delta.x += delta.x;
            newest.delta.y += delta.y;
            return;
        }
    }
    ring_[head_] = {timeMs, delta};
    head_ = (head_ + 1) % kCapacity;
    count_ = std::min(count_ + 1, kCapacity);
}

cammath::Vec ReleaseVelocity::estimate(std::uint32_t endTimeMs) const noexcept {
    if (count_ == 0)
        return {};
    const auto tn = at(count_ - 1).timeMs;
    if (elapsed(endTimeMs, tn) > kMaxRestMs)
        return {};
    // Samples are chronological: the window starts at the oldest sample no more than kWindowMs before tn.
    std::size_t first = count_ - 1;
    while (first > 0 && elapsed(tn, at(first - 1).timeMs) <= kWindowMs)
        --first;
    if (count_ - first < 2)
        return {};
    cammath::Vec sum;
    for (std::size_t i = first + 1; i < count_; ++i) {
        sum.x += at(i).delta.x;
        sum.y += at(i).delta.y;
    }
    const double seconds = elapsed(tn, at(first).timeMs) / 1000.0;
    if (!(seconds > 0.0))
        return {};
    cammath::Vec v{sum.x / seconds, sum.y / seconds};
    const double speed = length(v);
    if (speed > kMaxSpeed)
        v = {v.x * (kMaxSpeed / speed), v.y * (kMaxSpeed / speed)};
    return v;
}

// ---- Glide -------------------------------------------------------------------------------------------------------

std::optional<double> glideDamping(double strength) noexcept {
    if (!std::isfinite(strength) || strength <= 0.0)
        return std::nullopt;
    return 2.0 + 10.0 * (1.0 - std::min(strength, 1.0));
}

cammath::Vec expectedGlide(cammath::Vec v, const GlideConfig& cfg) noexcept {
    if (!validGlideConfig(cfg) || !finite(v))
        return {};
    const double speed = length(v);
    const double start = std::min(speed, cfg.maxSpeed);
    if (speed <= 0.0 || start < cfg.minSpeed)
        return {};
    const double distance = (start - cfg.stopSpeed) / *glideDamping(cfg.strength);
    if (distance <= 0.0)
        return {};
    return {v.x / speed * distance, v.y / speed * distance};
}

bool Glide::start(cammath::Vec velocity, const GlideConfig& cfg) noexcept {
    stop();
    if (!validGlideConfig(cfg) || !finite(velocity))
        return false;
    const double speed = length(velocity);
    if (speed <= 0.0 || std::min(speed, cfg.maxSpeed) < cfg.minSpeed)
        return false;
    CameraMotionConfig motion;
    motion.damping = *glideDamping(cfg.strength);
    motion.maxSpeed = cfg.maxSpeed;
    motion.stopSpeed = cfg.stopSpeed;
    camera_ = hyprvx::Camera{};
    // Every field CameraController validates is finite and in range here, so the constructor cannot throw; the catch
    // only keeps the noexcept promise if that ever changes.
    try {
        controller_.emplace(camera_, motion);
        controller_->setVelocity(Vec2{velocity.x, velocity.y});  // clamps to maxSpeed
    } catch (...) {
        controller_.reset();
        return false;
    }
    active_ = true;
    return true;
}

cammath::Vec Glide::step(double dtSeconds) noexcept {
    if (!active_ || !controller_ || !std::isfinite(dtSeconds) || dtSeconds <= 0.0)
        return {};
    const Vec2 before = camera_.position();
    try {
        controller_->update(dtSeconds);
    } catch (...) {
        stop();
        return {};
    }
    const Vec2 moved = camera_.position() - before;
    // CameraController zeroes the velocity once it is below stopSpeed (no pan input): the glide is over.
    if (controller_->velocity().lengthSquared() == 0.0) {
        active_ = false;
        controller_.reset();
    }
    return {moved.x, moved.y};
}

void Glide::stop() noexcept {
    active_ = false;
    controller_.reset();
}

// ---- pinchCamera -------------------------------------------------------------------------------------------------

cammath::CameraState pinchCamera(const cammath::CameraState& start, cammath::Vec monitorOrigin, cammath::Vec anchor,
                                 double scale, cammath::ZoomLimits limits) noexcept {
    if (!std::isfinite(scale) || scale <= 0.0)
        return start;
    // The target zoom of the pinch (a scalar product, not a camera equation); the camera comes from anchorZoom.
    const double targetZoom = start.zoom * scale;
    return cammath::anchorZoom(start, monitorOrigin, anchor, targetZoom, limits);
}

} // namespace hyprvx::drift
