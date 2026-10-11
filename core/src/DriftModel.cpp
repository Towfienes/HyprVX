#include "hyprvx/core/DriftModel.hpp"

#include <cmath>

namespace hyprvx::drift {

namespace {

bool finite(cammath::Vec v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

} // namespace

// An OFF session that keeps the monitor's origin.
DriftModel::Session DriftModel::offSession(cammath::Vec origin) {
    Session s;
    s.origin = origin;
    return s;
}

namespace {

} // namespace

// ---- state machine ------------------------------------------------------------------------------------------------

const DriftModel::Session* DriftModel::session(MonitorId monitor) const {
    const auto it = sessions_.find(monitor);
    return it == sessions_.end() ? nullptr : &it->second;
}

Mode DriftModel::mode(MonitorId monitor) const {
    const auto* s = session(monitor);
    return s ? s->mode : Mode::Off;
}

std::optional<WorkspaceId> DriftModel::sceneWorkspace(MonitorId monitor) const {
    const auto* s = session(monitor);
    return s && s->mode == Mode::Drift ? s->scene : std::nullopt;
}

void DriftModel::setMonitorOrigin(MonitorId monitor, cammath::Vec origin) {
    sessions_[monitor].origin = origin;
}

cammath::Vec DriftModel::originOf(MonitorId monitor) const {
    const auto* s = session(monitor);
    return s ? s->origin : cammath::Vec{};
}

Status DriftModel::enable(MonitorId monitor, WorkspaceId active, bool special) {
    auto& s = sessions_[monitor];
    if (s.mode == Mode::Entering || s.mode == Mode::Drift)
        return Status::NoOp;
    if (s.mode == Mode::Exiting)
        return Status::State; // transitions complete within their command; EXITING is never seen between commands
    if (special || isSpecialWorkspaceId(active))
        return Status::State;
    s.mode = Mode::Entering;
    s.pending = active;
    s.scene.reset();
    return Status::Ok;
}

Status DriftModel::entered(MonitorId monitor, bool valid) {
    auto it = sessions_.find(monitor);
    if (it == sessions_.end() || it->second.mode != Mode::Entering)
        return Status::NoOp;
    if (!valid) {
        it->second = offSession(it->second.origin);
        return Status::Incompatible;
    }
    it->second.mode = Mode::Drift;
    it->second.scene = it->second.pending;
    return Status::Ok;
}

Status DriftModel::disable(MonitorId monitor) {
    auto it = sessions_.find(monitor);
    if (it == sessions_.end())
        return Status::NoOp;
    switch (it->second.mode) {
        case Mode::Off:
        case Mode::Exiting: return Status::NoOp;
        case Mode::Entering: it->second = offSession(it->second.origin); return Status::Ok;
        case Mode::Drift: break;
    }
    endDragOn(monitor);
    it->second.mode = Mode::Exiting;
    it->second.scene.reset();
    return Status::Ok;
}

Status DriftModel::exited(MonitorId monitor) {
    auto it = sessions_.find(monitor);
    if (it == sessions_.end() || it->second.mode != Mode::Exiting)
        return Status::NoOp;
    it->second = offSession(it->second.origin);
    return Status::Ok;
}

void DriftModel::panic() {
    drag_.reset();
    for (auto& [monitor, s] : sessions_)
        s = offSession(s.origin);
}

Status DriftModel::invalidCamera(MonitorId monitor) {
    return mode(monitor) == Mode::Drift ? disable(monitor) : Status::NoOp;
}

void DriftModel::monitorRemoved(MonitorId monitor) {
    endDragOn(monitor);
    sessions_.erase(monitor);
}

void DriftModel::workspaceActivated(MonitorId monitor, WorkspaceId workspace, bool special) {
    auto it = sessions_.find(monitor);
    if (it == sessions_.end() || it->second.mode != Mode::Drift || special || isSpecialWorkspaceId(workspace))
        return;
    if (it->second.scene == workspace)
        return;
    endDragOn(monitor);
    it->second.scene = workspace;
    it->second.pending = workspace;
}

// ---- cameras ------------------------------------------------------------------------------------------------------

cammath::CameraState DriftModel::camera(WorkspaceId workspace, cammath::Vec monitorOrigin) const {
    const auto it = cameras_.find(workspace);
    return it == cameras_.end() ? cammath::identityFor(monitorOrigin) : it->second;
}

std::optional<cammath::CameraState> DriftModel::storedCamera(WorkspaceId workspace) const {
    const auto it = cameras_.find(workspace);
    return it == cameras_.end() ? std::nullopt : std::optional{it->second};
}

bool DriftModel::setCamera(WorkspaceId workspace, const cammath::CameraState& camera) {
    if (isSpecialWorkspaceId(workspace) || cammath::validate(camera, DRIFT_ZOOM_LIMITS) != cammath::Error::None)
        return false;
    cameras_[workspace] = camera;
    return true;
}

cammath::CameraState DriftModel::monitorCamera(MonitorId monitor, cammath::Vec monitorOrigin) const {
    const auto scene = sceneWorkspace(monitor);
    return scene ? camera(*scene, monitorOrigin) : cammath::identityFor(monitorOrigin);
}

Status DriftModel::pan(MonitorId monitor, cammath::Vec screenDelta) {
    const auto scene = sceneWorkspace(monitor);
    if (!scene)
        return Status::State;
    if (!finite(screenDelta))
        return Status::Invalid;
    const auto next = cammath::panByScreen(camera(*scene, originOf(monitor)), screenDelta);
    return setCamera(*scene, next) ? Status::Ok : Status::Invalid;
}

Status DriftModel::zoom(MonitorId monitor, cammath::Vec monitorOrigin, cammath::Vec anchor, double factor) {
    const auto scene = sceneWorkspace(monitor);
    if (!scene)
        return Status::State;
    if (!finite(anchor) || !std::isfinite(factor) || !(factor > 0.0))
        return Status::Invalid;
    const auto current = camera(*scene, monitorOrigin);
    const auto next = cammath::anchorZoom(current, monitorOrigin, anchor, current.zoom * factor, DRIFT_ZOOM_LIMITS);
    return setCamera(*scene, next) ? Status::Ok : Status::Invalid;
}

Status DriftModel::reset(MonitorId monitor, cammath::Vec monitorOrigin) {
    const auto scene = sceneWorkspace(monitor);
    if (!scene)
        return Status::State;
    return setCamera(*scene, cammath::identityFor(monitorOrigin)) ? Status::Ok : Status::Invalid;
}

// ---- window offsets -----------------------------------------------------------------------------------------------

cammath::Vec DriftModel::offset(WindowId window) const {
    const auto it = offsets_.find(window);
    return it == offsets_.end() ? cammath::Vec{} : it->second;
}

bool DriftModel::setOffset(WindowId window, cammath::Vec offset) {
    if (!finite(offset))
        return false;
    offsets_[window] = offset;
    return true;
}

void DriftModel::windowClosed(WindowId window) {
    offsets_.erase(window);
    windowLeftScene(window);
}

void DriftModel::windowLeftScene(WindowId window) {
    if (drag_ && drag_->kind == DragKind::Move && drag_->window == window)
        drag_.reset();
}

cammath::Vec DriftModel::mechanismOffset(WindowId window, WorkspaceId workspace, MonitorId monitor, bool mapped) const {
    const auto scene = sceneWorkspace(monitor);
    return mapped && scene && *scene == workspace ? offset(window) : cammath::Vec{};
}

// ---- drags --------------------------------------------------------------------------------------------------------

bool DriftModel::beginPan(MonitorId monitor, cammath::Vec screen) {
    const auto scene = sceneWorkspace(monitor);
    if (!scene || drag_ || !finite(screen))
        return false;
    drag_ = Drag{.kind = DragKind::Pan, .monitor = monitor, .startScreen = screen, .startOffset = {}, .startCamera = camera(*scene, originOf(monitor)), .workspace = *scene};
    return true;
}

bool DriftModel::beginMove(MonitorId monitor, WindowId window, cammath::Vec screen) {
    const auto scene = sceneWorkspace(monitor);
    if (!scene || drag_ || !finite(screen))
        return false;
    drag_ = Drag{.kind = DragKind::Move,
                 .monitor = monitor,
                 .window = window,
                 .startScreen = screen,
                 .startOffset = offset(window),
                 .startCamera = camera(*scene, originOf(monitor)),
                 .workspace = *scene};
    return true;
}

bool DriftModel::dragMotion(cammath::Vec screen) {
    if (!drag_ || !finite(screen))
        return false;
    const cammath::Vec delta{screen.x - drag_->startScreen.x, screen.y - drag_->startScreen.y};
    if (drag_->kind == DragKind::Move) {
        const auto world = cammath::worldDelta(drag_->startCamera, delta);
        return setOffset(drag_->window, {drag_->startOffset.x + world.x, drag_->startOffset.y + world.y});
    }
    return setCamera(drag_->workspace, cammath::panByScreen(drag_->startCamera, delta));
}

void DriftModel::dragEnd() {
    drag_.reset();
}

void DriftModel::endDragOn(MonitorId monitor) {
    if (drag_ && drag_->monitor == monitor)
        drag_.reset();
}

// ---- spawn placement ----------------------------------------------------------------------------------------------

cammath::Vec DriftModel::spawnOffset(const cammath::CameraState& camera, cammath::Vec monitorSize, cammath::Box logical) {
    const auto visible = cammath::visibleWorldRect(camera, monitorSize);
    return {visible.pos.x + visible.size.x / 2.0 - (logical.pos.x + logical.size.x / 2.0),
            visible.pos.y + visible.size.y / 2.0 - (logical.pos.y + logical.size.y / 2.0)};
}

} // namespace hyprvx::drift
