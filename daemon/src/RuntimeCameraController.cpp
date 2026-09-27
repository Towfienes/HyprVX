#include "hyprvx/runtime/RuntimeCameraController.hpp"

#include "hyprvx/runtime/SpatialProjection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace hyprvx::runtime {

RuntimeCameraController::RuntimeCameraController(
    Camera& camera,
    World& world,
    Vec2 viewportPixels,
    SpatialProjector* projector,
    CameraMotionConfig motion,
    std::size_t historyCapacity)
    : camera_(camera), world_(world), viewportPixels_(viewportPixels), projector_(projector),
      ownedMotion_(std::make_unique<CameraController>(camera_, motion)), motion_(*ownedMotion_),
      history_(historyCapacity), panHistory_(history_) {
    setViewport(viewportPixels);
}

RuntimeCameraController::RuntimeCameraController(
    Camera& camera,
    CameraController& motion,
    World& world,
    Vec2 viewportPixels,
    SpatialProjector* projector,
    std::size_t historyCapacity)
    : camera_(camera), world_(world), viewportPixels_(viewportPixels), projector_(projector),
      motion_(motion), history_(historyCapacity), panHistory_(history_) {
    setViewport(viewportPixels);
}

void RuntimeCameraController::setViewport(Vec2 viewportPixels) {
    if (!std::isfinite(viewportPixels.x) || !std::isfinite(viewportPixels.y) ||
        viewportPixels.x <= 0.0 || viewportPixels.y <= 0.0) {
        throw std::invalid_argument("runtime camera viewport must be finite and positive");
    }
    viewportPixels_ = viewportPixels;
}

CameraView RuntimeCameraController::currentView() const noexcept {
    return {camera_.position(), camera_.zoom()};
}

bool RuntimeCameraController::hasProjectedWindow(WindowId id) const {
    if (!world_.windowExists(id)) return false;
    if (projector_ == nullptr) return true;
    for (const auto& [address, record] : projector_->windows()) {
        static_cast<void>(address);
        if (record.windowId == id && record.placementStatus == PlacementStatus::Projected) return true;
    }
    return false;
}

bool RuntimeCameraController::panBegin() {
    if (!panTransactionActive_) {
        motion_.stop();
        keyboardPanActive_ = false;
        panHistory_.begin(currentView());
        panTransactionActive_ = true;
        selectionOwnership_ = SelectionOwnership::UserOwned;
    }
    return true;
}

void RuntimeCameraController::panByWorld(Vec2 delta) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y)) return;
    static_cast<void>(panBegin());
    motion_.cancelFlight();
    motion_.setVelocity({});
    camera_.moveBy(delta);
}

void RuntimeCameraController::panByScreen(Vec2 deltaPixels) {
    if (!std::isfinite(deltaPixels.x) || !std::isfinite(deltaPixels.y)) return;
    panByWorld({-deltaPixels.x / camera_.zoom(), -deltaPixels.y / camera_.zoom()});
}

bool RuntimeCameraController::panEnd() {
    if (!panTransactionActive_) return false;
    motion_.setPanInput({});
    motion_.setVelocity({});
    const bool changed = panHistory_.finish(currentView());
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
    selectionOwnership_ = SelectionOwnership::UserOwned;
    return changed;
}

void RuntimeCameraController::setPanInput(Vec2 direction) {
    const bool active = direction.lengthSquared() > 1e-12;
    if (active && !keyboardPanActive_) {
        static_cast<void>(panBegin());
        keyboardPanActive_ = true;
    }
    motion_.setPanInput(direction);
    if (active) selectionOwnership_ = SelectionOwnership::UserOwned;
    else if (keyboardPanActive_) keyboardPanActive_ = false;
}

void RuntimeCameraController::zoomAtAnchor(double factor, Vec2 screenAnchor) {
    if (!std::isfinite(factor) || factor <= 0.0 || !std::isfinite(screenAnchor.x) || !std::isfinite(screenAnchor.y)) return;
    motion_.cancelFlight();
    motion_.setPanInput({});
    motion_.zoomByFactorAroundScreenPoint(factor, screenAnchor);
}

void RuntimeCameraController::cancelInputGesture() noexcept {
    motion_.stop();
    panHistory_.cancel();
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
}

void RuntimeCameraController::finishPanIfSettled() {
    if (panTransactionActive_ && !keyboardPanActive_ && !motion_.isFlying() &&
        motion_.velocity().lengthSquared() <= motion_.config().stopSpeed * motion_.config().stopSpeed) {
        motion_.stop();
        static_cast<void>(panHistory_.finish(currentView()));
        panTransactionActive_ = false;
    }
}

bool RuntimeCameraController::transitionTo(CameraView target, bool recordHistory) {
    if (!std::isfinite(target.position.x) || !std::isfinite(target.position.y) ||
        !std::isfinite(target.zoom) || target.zoom <= 0.0) return false;
    motion_.stop();
    panHistory_.cancel();
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
    if (recordHistory) history_.record(currentView());
    motion_.flyToView(target.position, target.zoom);
    return true;
}

bool RuntimeCameraController::selectAndFrame(WindowId id, ViewportFramingPolicy policy) {
    if (!hasProjectedWindow(id)) return false;
    const auto geometry = world_.windowGeometry(id);
    if (!geometry) return false;
    const auto framed = frameRectInViewport(*geometry, viewportPixels_, camera_.zoom(), policy);
    if (!transitionTo({framed.cameraPosition, framed.zoom})) return false;
    selectedWindow_ = id;
    selectionOwnership_ = SelectionOwnership::UserOwned;
    return true;
}

bool RuntimeCameraController::gotoWindow(WindowId id, ViewportFramingPolicy policy) {
    return selectAndFrame(id, policy);
}

bool RuntimeCameraController::selectWindow(WindowId id) {
    if (!hasProjectedWindow(id)) return false;
    selectedWindow_ = id;
    selectionOwnership_ = SelectionOwnership::UserOwned;
    return true;
}

bool RuntimeCameraController::gotoExternalAddress(std::string_view address, ViewportFramingPolicy policy) {
    if (projector_ == nullptr) return false;
    const auto id = projector_->mappedId(address);
    return id && selectAndFrame(*id, policy);
}

bool RuntimeCameraController::gotoWorkspace(std::string_view workspace, ViewportFramingPolicy policy) {
    if (projector_ == nullptr || projector_->workspaceMap().find(workspace) == nullptr) return false;
    const auto framed = projector_->viewForWorkspace(workspace, viewportPixels_, camera_.zoom(), policy);
    if (!framed) return false;
    const bool changed = transitionTo({framed->position, framed->zoom});
    if (changed) selectionOwnership_ = SelectionOwnership::UserOwned;
    return changed;
}

bool RuntimeCameraController::gotoView(CameraView view) {
    const bool changed = transitionTo(view);
    if (changed) selectionOwnership_ = SelectionOwnership::UserOwned;
    return changed;
}

bool RuntimeCameraController::focusDirection(SpatialDirection direction, ViewportFramingPolicy policy) {
    if (projector_ != nullptr) reconcileProjector();
    if (!selectedWindow_ || !hasProjectedWindow(*selectedWindow_)) return false;
    const auto next = navigator_.findNeighbor(world_, *selectedWindow_, direction);
    if (!next) return false;
    return selectAndFrame(next->windowId, policy);
}

bool RuntimeCameraController::historyBack() {
    if (!history_.canGoBack()) return false;
    motion_.stop();
    const auto target = history_.back(currentView());
    if (!target) return false;
    panHistory_.cancel();
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
    motion_.flyToView(target->position, target->zoom);
    selectionOwnership_ = SelectionOwnership::UserOwned;
    return true;
}

bool RuntimeCameraController::historyForward() {
    if (!history_.canGoForward()) return false;
    motion_.stop();
    const auto target = history_.forward(currentView());
    if (!target) return false;
    panHistory_.cancel();
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
    motion_.flyToView(target->position, target->zoom);
    selectionOwnership_ = SelectionOwnership::UserOwned;
    return true;
}

bool RuntimeCameraController::home(
    ViewportFramingPolicy policy,
    std::optional<std::string_view> focusedWorkspace) {
    if (selectionOwnership_ == SelectionOwnership::UserOwned && selectedWindow_ &&
        selectAndFrame(*selectedWindow_, policy)) return true;
    if (projector_ != nullptr) {
        if (const auto focused = projector_->focusedWindowId(); focused && selectAndFrame(*focused, policy)) return true;
        if (focusedWorkspace) {
            if (const auto view = projector_->viewForWorkspace(*focusedWorkspace, viewportPixels_, camera_.zoom(), policy)) {
                return transitionTo({view->position, view->zoom});
            }
        }
    }
    if (selectionOwnership_ == SelectionOwnership::CompositorSeeded && selectedWindow_ &&
        selectAndFrame(*selectedWindow_, policy)) return true;
    if (projector_ != nullptr) {
        const ProjectedWindowRecord* nearest{};
        double nearestDistance = std::numeric_limits<double>::infinity();
        for (const auto& [address, record] : projector_->windows()) {
            static_cast<void>(address);
            if (record.placementStatus != PlacementStatus::Projected || !world_.windowExists(record.windowId)) continue;
            const Vec2 delta = record.geometry.center() - camera_.position();
            const double distance = delta.lengthSquared();
            if (distance < nearestDistance || (distance == nearestDistance && nearest && record.windowId < nearest->windowId)) {
                nearest = &record;
                nearestDistance = distance;
            }
        }
        if (nearest && selectAndFrame(nearest->windowId, policy)) return true;
    }
    return transitionTo({{0.0, 0.0}, camera_.zoom()});
}

void RuntimeCameraController::observeCompositorSelection(std::optional<WindowId> id) {
    if (selectionOwnership_ == SelectionOwnership::UserOwned) return;
    selectedWindow_ = id && hasProjectedWindow(*id) ? id : std::nullopt;
}

void RuntimeCameraController::observeCompositorFocus(std::string_view externalAddress) {
    if (selectionOwnership_ == SelectionOwnership::UserOwned || projector_ == nullptr) return;
    observeCompositorSelection(projector_->mappedId(externalAddress));
}

void RuntimeCameraController::resetSelectionOwnership() noexcept {
    selectionOwnership_ = SelectionOwnership::CompositorSeeded;
}

void RuntimeCameraController::reconcileProjector() {
    if (selectedWindow_ && !hasProjectedWindow(*selectedWindow_)) selectedWindow_.reset();
}

void RuntimeCameraController::endEpoch() noexcept {
    selectedWindow_.reset();
    selectionOwnership_ = SelectionOwnership::CompositorSeeded;
    panTransactionActive_ = false;
    keyboardPanActive_ = false;
    panHistory_.cancel();
    motion_.stop();
}

bool RuntimeCameraController::tick(double deltaSeconds) {
    if (!needsFrame()) return false;
    const auto before = currentView();
    motion_.update(deltaSeconds);
    finishPanIfSettled();
    reconcileProjector();
    const auto after = currentView();
    return before.position.x != after.position.x || before.position.y != after.position.y || before.zoom != after.zoom;
}

bool RuntimeCameraController::needsFrame() const noexcept {
    return motion_.isFlying() || motion_.panInput().lengthSquared() > 1e-12 ||
        motion_.velocity().lengthSquared() > motion_.config().stopSpeed * motion_.config().stopSpeed;
}

} // namespace hyprvx::runtime
