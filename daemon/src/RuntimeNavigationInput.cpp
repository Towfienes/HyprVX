#include "hyprvx/runtime/RuntimeNavigationInput.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hyprvx::runtime {
namespace {

bool finite(Vec2 value) noexcept { return std::isfinite(value.x) && std::isfinite(value.y); }

RuntimeNavigationAction action(RuntimeNavigationActionType type) {
    RuntimeNavigationAction result;
    result.type = type;
    return result;
}

} // namespace

RuntimeNavigationActionDispatcher::RuntimeNavigationActionDispatcher(RuntimeCameraController& controller) noexcept
    : controller_(controller) {}

void RuntimeNavigationActionDispatcher::cancelDirectGesture() noexcept {
    pendingPanDelta_ = {};
    if (directGestureActive_) controller_.cancelInputGesture();
    directGestureActive_ = false;
}

bool RuntimeNavigationActionDispatcher::submit(const RuntimeNavigationAction& value) {
    using T = RuntimeNavigationActionType;
    switch (value.type) {
    case T::PanBegin:
        if (directGestureActive_) return false;
        pendingPanDelta_ = {};
        directGestureActive_ = controller_.panBegin();
        return directGestureActive_;
    case T::PanDelta:
        if (!directGestureActive_ || !finite(value.delta)) return false;
        pendingPanDelta_.x += value.delta.x;
        pendingPanDelta_.y += value.delta.y;
        return true;
    case T::PanEnd:
        if (!directGestureActive_) return false;
        static_cast<void>(flushPendingPan());
        directGestureActive_ = false;
        return controller_.panEnd();
    case T::ContinuousPanStart:
    case T::ContinuousPanUpdate:
        if (!finite(value.direction)) return false;
        if (directGestureActive_) cancelDirectGesture();
        controller_.setPanInput(value.direction);
        return true;
    case T::ContinuousPanStop:
        if (directGestureActive_) cancelDirectGesture();
        controller_.setPanInput({});
        return true;
    case T::ZoomBegin:
        if (directGestureActive_) return false;
        pendingPanDelta_ = {};
        directGestureActive_ = controller_.panBegin();
        return directGestureActive_;
    case T::ZoomAtAnchor:
        if (!std::isfinite(value.zoomFactor) || value.zoomFactor <= 0.0 || !finite(value.anchor)) return false;
        if (directGestureActive_) controller_.zoomAtAnchor(value.zoomFactor, value.anchor);
        else {
            static_cast<void>(controller_.panBegin());
            controller_.zoomAtAnchor(value.zoomFactor, value.anchor);
            static_cast<void>(controller_.panEnd());
        }
        return true;
    case T::ZoomEnd:
        if (!directGestureActive_) return false;
        directGestureActive_ = false;
        return controller_.panEnd();
    case T::SelectWindow:
        cancelDirectGesture();
        return controller_.selectWindow(value.windowId);
    case T::GotoWindow:
        cancelDirectGesture();
        return controller_.gotoWindow(value.windowId);
    case T::GotoWorkspace:
        cancelDirectGesture();
        return controller_.gotoWorkspace(value.workspace);
    case T::GotoView:
        cancelDirectGesture();
        return controller_.gotoView(value.view);
    case T::FocusDirection:
        cancelDirectGesture();
        return controller_.focusDirection(value.spatialDirection);
    case T::HistoryBack:
        cancelDirectGesture();
        return controller_.historyBack();
    case T::HistoryForward:
        cancelDirectGesture();
        return controller_.historyForward();
    case T::Home:
        cancelDirectGesture();
        return controller_.home();
    case T::SaveViewSlot:
    case T::GotoViewSlot:
    case T::Quit:
        return false;
    }
    return false;
}

bool RuntimeNavigationActionDispatcher::flushPendingPan() {
    if (!directGestureActive_) return false;
    const Vec2 delta = pendingPanDelta_;
    pendingPanDelta_ = {};
    if (!finite(delta) || (delta.x == 0.0 && delta.y == 0.0)) return false;
    controller_.panByScreen(delta);
    return true;
}

void RuntimeNavigationActionDispatcher::resetForEpoch() noexcept {
    pendingPanDelta_ = {};
    directGestureActive_ = false;
    controller_.endEpoch();
}

void RuntimeNavigationActionDispatcher::cancelInput() noexcept {
    pendingPanDelta_ = {};
    directGestureActive_ = false;
    controller_.cancelInputGesture();
}

std::optional<std::size_t> KeyboardNavigationAdapter::directionSlot(KeyboardKey key) noexcept {
    switch (key) {
    case KeyboardKey::H: return 0;
    case KeyboardKey::J: return 1;
    case KeyboardKey::K: return 2;
    case KeyboardKey::L: return 3;
    case KeyboardKey::Up: return 4;
    case KeyboardKey::Down: return 5;
    case KeyboardKey::Left: return 6;
    case KeyboardKey::Right: return 7;
    default: return std::nullopt;
    }
}

Vec2 KeyboardNavigationAdapter::panDirection() const noexcept {
    Vec2 result{};
    if (held_[0] || held_[6]) result.x -= 1.0;
    if (held_[3] || held_[7]) result.x += 1.0;
    if (held_[2] || held_[4]) result.y -= 1.0;
    if (held_[1] || held_[5]) result.y += 1.0;
    if (result.lengthSquared() > 1.0) {
        const double inverseLength = 1.0 / std::sqrt(result.lengthSquared());
        result.x *= inverseLength;
        result.y *= inverseLength;
    }
    return result;
}

bool KeyboardNavigationAdapter::panActive() const noexcept {
    return std::any_of(held_.begin(), held_.end(), [](bool pressed) { return pressed; });
}

bool KeyboardNavigationAdapter::key(
    KeyboardKey keyValue, bool pressed, KeyboardModifiers modifiers, bool repeat,
    RuntimeNavigationAction& output) {
    if (pressed && !repeat && modifiers.alt && (keyValue == KeyboardKey::Left || keyValue == KeyboardKey::Right)) {
        output = action(keyValue == KeyboardKey::Left ? RuntimeNavigationActionType::HistoryBack : RuntimeNavigationActionType::HistoryForward);
        return true;
    }
    const auto slot = directionSlot(keyValue);
    if (slot) {
        if (!pressed && !held_[*slot]) return false;
        if (!pressed && held_[*slot]) {
            const bool wasActive = panActive();
            held_[*slot] = false;
            const bool nowActive = panActive();
            output = action(wasActive && !nowActive ? RuntimeNavigationActionType::ContinuousPanStop : RuntimeNavigationActionType::ContinuousPanUpdate);
            output.direction = panDirection();
            return true;
        }
        if (modifiers.shift) {
            if (!pressed || repeat) return false;
            output = action(RuntimeNavigationActionType::FocusDirection);
            switch (keyValue) {
            case KeyboardKey::H: case KeyboardKey::Left: output.spatialDirection = SpatialDirection::Left; break;
            case KeyboardKey::J: case KeyboardKey::Down: output.spatialDirection = SpatialDirection::Down; break;
            case KeyboardKey::K: case KeyboardKey::Up: output.spatialDirection = SpatialDirection::Up; break;
            case KeyboardKey::L: case KeyboardKey::Right: output.spatialDirection = SpatialDirection::Right; break;
            default: break;
            }
            return true;
        }
        if (modifiers.alt || modifiers.control) return false;
        const bool wasActive = panActive();
        held_[*slot] = pressed;
        const bool nowActive = panActive();
        output = action(!wasActive && nowActive ? RuntimeNavigationActionType::ContinuousPanStart :
            wasActive && !nowActive ? RuntimeNavigationActionType::ContinuousPanStop : RuntimeNavigationActionType::ContinuousPanUpdate);
        output.direction = panDirection();
        return true;
    }
    if (!pressed || repeat) return false;
    switch (keyValue) {
    case KeyboardKey::Home: case KeyboardKey::R: output = action(RuntimeNavigationActionType::Home); return true;
    case KeyboardKey::Digit1: case KeyboardKey::Digit2: case KeyboardKey::Digit3:
        output = action(modifiers.control ? RuntimeNavigationActionType::SaveViewSlot : RuntimeNavigationActionType::GotoWorkspace);
        output.slot = keyValue == KeyboardKey::Digit1 ? 1 : keyValue == KeyboardKey::Digit2 ? 2 : 3;
        output.workspace = std::to_string(output.slot);
        return true;
    case KeyboardKey::F: output = action(RuntimeNavigationActionType::GotoViewSlot); output.slot = 2; return true;
    case KeyboardKey::Escape: output = action(RuntimeNavigationActionType::Quit); return true;
    default: return false;
    }
}

void KeyboardNavigationAdapter::reset() noexcept { held_.fill(false); }

MouseNavigationAdapter::MouseNavigationAdapter(MouseNavigationConfig config) : config_(config) {
    if (!std::isfinite(config_.dragSensitivity) || config_.dragSensitivity <= 0.0 ||
        !std::isfinite(config_.wheelZoomSensitivity) || config_.wheelZoomSensitivity <= 0.0)
        throw std::invalid_argument("mouse navigation sensitivities must be finite and positive");
}

bool MouseNavigationAdapter::button(MouseButton buttonValue, bool pressed, RuntimeNavigationAction& output) {
    if (buttonValue != MouseButton::Middle || middleDown_ == pressed) return false;
    middleDown_ = pressed;
    output = action(pressed ? RuntimeNavigationActionType::PanBegin : RuntimeNavigationActionType::PanEnd);
    return true;
}

bool MouseNavigationAdapter::motion(Vec2 pixelDelta, RuntimeNavigationAction& output) const {
    if (!middleDown_ || !finite(pixelDelta)) return false;
    output = action(RuntimeNavigationActionType::PanDelta);
    output.delta = {pixelDelta.x * config_.dragSensitivity, pixelDelta.y * config_.dragSensitivity};
    return true;
}

bool MouseNavigationAdapter::click(std::optional<WindowId> projectedWindow, RuntimeNavigationAction& output) const {
    if (!projectedWindow) return false;
    output = action(RuntimeNavigationActionType::SelectWindow);
    output.windowId = *projectedWindow;
    return true;
}

bool MouseNavigationAdapter::wheelBegin(RuntimeNavigationAction& output) {
    if (wheelActive_) return false;
    wheelActive_ = true;
    output = action(RuntimeNavigationActionType::ZoomBegin);
    return true;
}

bool MouseNavigationAdapter::wheel(double verticalDelta, Vec2 screenAnchor, RuntimeNavigationAction& output) const {
    if (!wheelActive_ || !std::isfinite(verticalDelta) || !finite(screenAnchor)) return false;
    const double factor = std::exp(verticalDelta * config_.wheelZoomSensitivity);
    if (!std::isfinite(factor) || factor <= 0.0) return false;
    output = action(RuntimeNavigationActionType::ZoomAtAnchor);
    output.zoomFactor = factor;
    output.anchor = screenAnchor;
    return true;
}

bool MouseNavigationAdapter::wheelEnd(RuntimeNavigationAction& output) {
    if (!wheelActive_) return false;
    wheelActive_ = false;
    output = action(RuntimeNavigationActionType::ZoomEnd);
    return true;
}

void MouseNavigationAdapter::reset() noexcept { middleDown_ = false; wheelActive_ = false; }

GestureNavigationAdapter::GestureNavigationAdapter(GestureNavigationConfig config) : config_(config) {
    if (config_.panFingerCount == 0 || config_.swipeFingerCount == 0 || config_.panFingerCount == config_.swipeFingerCount ||
        !std::isfinite(config_.panSensitivityPixels) || config_.panSensitivityPixels <= 0.0 ||
        !std::isfinite(config_.swipeThreshold) || config_.swipeThreshold <= 0.0 ||
        !std::isfinite(config_.pinchSensitivity) || config_.pinchSensitivity <= 0.0)
        throw std::invalid_argument("gesture navigation configuration is invalid");
}

bool GestureNavigationAdapter::panBegin(unsigned fingers, RuntimeNavigationAction& output) {
    if (state_ != State::Idle || fingers != config_.panFingerCount) return false;
    state_ = State::Pan;
    output = action(RuntimeNavigationActionType::PanBegin);
    return true;
}

bool GestureNavigationAdapter::panUpdate(Vec2 delta, RuntimeNavigationAction& output) {
    if (state_ != State::Pan || !finite(delta)) return false;
    output = action(RuntimeNavigationActionType::PanDelta);
    output.delta = {delta.x * config_.panSensitivityPixels, delta.y * config_.panSensitivityPixels};
    return true;
}

bool GestureNavigationAdapter::panEnd(RuntimeNavigationAction& output) {
    if (state_ != State::Pan) return false;
    state_ = State::Idle;
    output = action(RuntimeNavigationActionType::PanEnd);
    return true;
}

bool GestureNavigationAdapter::pinchBegin(unsigned fingers, RuntimeNavigationAction& output) {
    if (state_ != State::Idle || fingers != 2) return false;
    state_ = State::Pinch;
    output = action(RuntimeNavigationActionType::ZoomBegin);
    return true;
}

bool GestureNavigationAdapter::pinchUpdate(double scaleFactor, Vec2 anchor, Vec2 viewport, RuntimeNavigationAction& output) {
    if (state_ != State::Pinch || !std::isfinite(scaleFactor) || scaleFactor <= 0.0 ||
        !finite(anchor) || !finite(viewport) || viewport.x <= 0.0 || viewport.y <= 0.0) return false;
    const double factor = std::pow(scaleFactor, config_.pinchSensitivity);
    if (!std::isfinite(factor) || factor <= 0.0) return false;
    output = action(RuntimeNavigationActionType::ZoomAtAnchor);
    output.zoomFactor = factor;
    output.anchor = {anchor.x * viewport.x, anchor.y * viewport.y};
    return true;
}

bool GestureNavigationAdapter::pinchEnd(RuntimeNavigationAction& output) {
    if (state_ != State::Pinch) return false;
    state_ = State::Idle;
    output = action(RuntimeNavigationActionType::ZoomEnd);
    return true;
}

bool GestureNavigationAdapter::swipeBegin(unsigned fingers) {
    if (state_ != State::Idle || fingers != config_.swipeFingerCount) return false;
    state_ = State::Swipe;
    swipeTotal_ = {};
    return true;
}

void GestureNavigationAdapter::swipeUpdate(Vec2 delta) noexcept {
    if (state_ == State::Swipe && finite(delta)) { swipeTotal_.x += delta.x; swipeTotal_.y += delta.y; }
}

bool GestureNavigationAdapter::swipeEnd(RuntimeNavigationAction& output) {
    if (state_ != State::Swipe) return false;
    state_ = State::Idle;
    if (std::max(std::abs(swipeTotal_.x), std::abs(swipeTotal_.y)) < config_.swipeThreshold) return false;
    output = action(RuntimeNavigationActionType::FocusDirection);
    output.spatialDirection = std::abs(swipeTotal_.x) > std::abs(swipeTotal_.y)
        ? (swipeTotal_.x < 0.0 ? SpatialDirection::Right : SpatialDirection::Left)
        : (swipeTotal_.y < 0.0 ? SpatialDirection::Down : SpatialDirection::Up);
    return true;
}

void GestureNavigationAdapter::reset() noexcept { state_ = State::Idle; swipeTotal_ = {}; }
bool GestureNavigationAdapter::active() const noexcept { return state_ != State::Idle; }

} // namespace hyprvx::runtime
