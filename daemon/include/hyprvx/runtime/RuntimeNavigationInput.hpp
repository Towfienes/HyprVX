#pragma once

#include "hyprvx/runtime/RuntimeCameraController.hpp"

#include <array>
#include <optional>
#include <string>

namespace hyprvx::runtime {

enum class RuntimeNavigationActionType {
    PanBegin, PanDelta, PanEnd,
    ContinuousPanStart, ContinuousPanUpdate, ContinuousPanStop,
    SelectWindow, GotoWindow, GotoWorkspace, GotoView,
    FocusDirection, HistoryBack, HistoryForward, Home,
    ZoomBegin, ZoomAtAnchor, ZoomEnd,
    SaveViewSlot, GotoViewSlot, Quit
};

// Device-independent intent. PanDelta is logical screen-pixel displacement;
// adapters normalize their own sensitivity before creating this action.
struct RuntimeNavigationAction {
    RuntimeNavigationActionType type{};
    Vec2 delta{};
    Vec2 direction{};
    Vec2 anchor{};
    double zoomFactor{1.0};
    WindowId windowId{};
    SpatialDirection spatialDirection{SpatialDirection::Right};
    CameraView view{};
    std::string workspace;
    int slot{};
};

class RuntimeNavigationActionDispatcher {
public:
    explicit RuntimeNavigationActionDispatcher(RuntimeCameraController& controller) noexcept;
    [[nodiscard]] bool submit(const RuntimeNavigationAction& action);
    [[nodiscard]] bool flushPendingPan();
    // Cancel device state after window focus loss without resetting projector,
    // selection, camera position, or connection epoch.
    void cancelInput() noexcept;
    void resetForEpoch() noexcept;
    [[nodiscard]] bool directGestureActive() const noexcept { return directGestureActive_; }
    [[nodiscard]] Vec2 pendingPanDelta() const noexcept { return pendingPanDelta_; }

private:
    void cancelDirectGesture() noexcept;
    RuntimeCameraController& controller_;
    Vec2 pendingPanDelta_{};
    bool directGestureActive_{};
};

enum class KeyboardKey {
    H, J, K, L, Up, Down, Left, Right, Home, R,
    Digit1, Digit2, Digit3, F, Escape
};

struct KeyboardModifiers { bool shift{}; bool alt{}; bool control{}; };

class KeyboardNavigationAdapter {
public:
    [[nodiscard]] bool key(
        KeyboardKey key,
        bool pressed,
        KeyboardModifiers modifiers,
        bool repeat,
        RuntimeNavigationAction& output);
    void reset() noexcept;
    [[nodiscard]] Vec2 panDirection() const noexcept;
    [[nodiscard]] bool panActive() const noexcept;

private:
    [[nodiscard]] static std::optional<std::size_t> directionSlot(KeyboardKey key) noexcept;
    std::array<bool, 8> held_{};
};

enum class MouseButton { Left, Middle, Other };

struct MouseNavigationConfig {
    double dragSensitivity{1.0};
    double wheelZoomSensitivity{0.14};
};

class MouseNavigationAdapter {
public:
    explicit MouseNavigationAdapter(MouseNavigationConfig config = {});
    [[nodiscard]] bool button(MouseButton button, bool pressed, RuntimeNavigationAction& output);
    [[nodiscard]] bool motion(Vec2 pixelDelta, RuntimeNavigationAction& output) const;
    [[nodiscard]] bool click(std::optional<WindowId> projectedWindow, RuntimeNavigationAction& output) const;
    [[nodiscard]] bool wheelBegin(RuntimeNavigationAction& output);
    [[nodiscard]] bool wheel(double verticalDelta, Vec2 screenAnchor, RuntimeNavigationAction& output) const;
    [[nodiscard]] bool wheelEnd(RuntimeNavigationAction& output);
    void reset() noexcept;
    [[nodiscard]] bool dragging() const noexcept { return middleDown_; }
    [[nodiscard]] bool wheelActive() const noexcept { return wheelActive_; }

private:
    MouseNavigationConfig config_;
    bool middleDown_{};
    bool wheelActive_{};
};

struct GestureNavigationConfig {
    unsigned panFingerCount{3};
    unsigned swipeFingerCount{4};
    double panSensitivityPixels{1000.0};
    double swipeThreshold{0.35};
    double pinchSensitivity{1.0};
};

class GestureNavigationAdapter {
public:
    explicit GestureNavigationAdapter(GestureNavigationConfig config = {});
    [[nodiscard]] bool panBegin(unsigned fingers, RuntimeNavigationAction& output);
    [[nodiscard]] bool panUpdate(Vec2 normalizedDelta, RuntimeNavigationAction& output);
    [[nodiscard]] bool panEnd(RuntimeNavigationAction& output);
    [[nodiscard]] bool pinchBegin(unsigned fingers, RuntimeNavigationAction& output);
    [[nodiscard]] bool pinchUpdate(double scaleFactor, Vec2 normalizedAnchor, Vec2 viewport, RuntimeNavigationAction& output);
    [[nodiscard]] bool pinchEnd(RuntimeNavigationAction& output);
    [[nodiscard]] bool swipeBegin(unsigned fingers);
    void swipeUpdate(Vec2 normalizedDelta) noexcept;
    [[nodiscard]] bool swipeEnd(RuntimeNavigationAction& output);
    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    enum class State { Idle, Pan, Pinch, Swipe };
    GestureNavigationConfig config_;
    State state_{State::Idle};
    Vec2 swipeTotal_{};
};

} // namespace hyprvx::runtime
