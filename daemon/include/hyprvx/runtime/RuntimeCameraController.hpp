#pragma once

#include "hyprvx/core/CameraMotion.hpp"
#include "hyprvx/core/NavigationState.hpp"
#include "hyprvx/core/SpatialNavigation.hpp"
#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/World.hpp"

#include <optional>
#include <memory>
#include <string_view>

namespace hyprvx::runtime {

class SpatialProjector;

// Semantic runtime navigation. Physical input adapters translate to these
// methods; this class never sends focus, window, or workspace commands to Hyprland.
class RuntimeCameraController {
public:
    RuntimeCameraController(
        Camera& camera,
        World& world,
        Vec2 viewportPixels,
        SpatialProjector* projector = nullptr,
        CameraMotionConfig motion = {},
        std::size_t historyCapacity = 64);
    RuntimeCameraController(
        Camera& camera,
        CameraController& motion,
        World& world,
        Vec2 viewportPixels,
        SpatialProjector* projector = nullptr,
        std::size_t historyCapacity = 64);

    void setViewport(Vec2 viewportPixels);
    [[nodiscard]] bool panBegin();
    void panByWorld(Vec2 delta);
    void panByScreen(Vec2 deltaPixels);
    [[nodiscard]] bool panEnd();
    void setPanInput(Vec2 direction);

    [[nodiscard]] bool gotoWindow(WindowId id, ViewportFramingPolicy policy = {});
    [[nodiscard]] bool gotoExternalAddress(std::string_view address, ViewportFramingPolicy policy = {});
    [[nodiscard]] bool gotoWorkspace(std::string_view workspace, ViewportFramingPolicy policy = {});
    [[nodiscard]] bool gotoView(CameraView view);
    [[nodiscard]] bool focusDirection(SpatialDirection direction, ViewportFramingPolicy policy = {});
    [[nodiscard]] bool historyBack();
    [[nodiscard]] bool historyForward();
    [[nodiscard]] bool home(
        ViewportFramingPolicy policy = {},
        std::optional<std::string_view> focusedWorkspace = std::nullopt);

    // Observed compositor focus seeds selection only until explicit user
    // navigation takes ownership. No compositor command is emitted.
    void observeCompositorSelection(std::optional<WindowId> id);
    void observeCompositorFocus(std::string_view externalAddress);
    void resetSelectionOwnership() noexcept;
    void reconcileProjector();
    void endEpoch() noexcept;

    // Call only while needsFrame() is true. Returns whether view changed.
    [[nodiscard]] bool tick(double deltaSeconds);
    [[nodiscard]] bool needsFrame() const noexcept;
    [[nodiscard]] const Camera& camera() const noexcept { return camera_; }
    [[nodiscard]] Camera& camera() noexcept { return camera_; }
    [[nodiscard]] std::optional<WindowId> selectedWindow() const noexcept { return selectedWindow_; }
    [[nodiscard]] bool userOwnsSelection() const noexcept { return userOwnsSelection_; }
    [[nodiscard]] const CameraHistory& history() const noexcept { return history_; }
    [[nodiscard]] CameraController& motion() noexcept { return motion_; }
    [[nodiscard]] const CameraController& motion() const noexcept { return motion_; }

private:
    [[nodiscard]] CameraView currentView() const noexcept;
    [[nodiscard]] bool selectAndFrame(WindowId id, ViewportFramingPolicy policy);
    [[nodiscard]] bool transitionTo(CameraView target, bool recordHistory = true);
    [[nodiscard]] bool hasProjectedWindow(WindowId id) const;
    void finishPanIfSettled();

    Camera& camera_;
    World& world_;
    Vec2 viewportPixels_;
    SpatialProjector* projector_{};
    std::unique_ptr<CameraController> ownedMotion_;
    CameraController& motion_;
    CameraHistory history_;
    CameraHistoryTransaction panHistory_;
    SpatialNavigator navigator_;
    std::optional<WindowId> selectedWindow_;
    bool userOwnsSelection_{};
    bool panTransactionActive_{};
    bool keyboardPanActive_{};
};

} // namespace hyprvx::runtime
