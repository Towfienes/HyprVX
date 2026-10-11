#include "Camera.hpp"

#include <src/debug/log/Logger.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/desktop/state/ViewState.hpp>
#include <src/desktop/view/Popup.hpp>
#include <src/desktop/view/WLSurface.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/output/Monitor.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/render/Renderer.hpp>
#include <src/state/MonitorQuery.hpp>
#include <src/state/MonitorState.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace hyprvx;
using namespace Render;

namespace {
    std::unordered_map<MONITORID, HyprVXSpike::SMonitorCamera>& cameras() {
        static std::unordered_map<MONITORID, HyprVXSpike::SMonitorCamera> CAMERAS;
        return CAMERAS;
    }

    const char* errorName(cammath::Error error) {
        switch (error) {
            case cammath::Error::None: return "none";
            case cammath::Error::NonFinite: return "non-finite";
            case cammath::Error::NonPositiveZoom: return "non-positive zoom";
            case cammath::Error::ZoomOutOfRange: return "zoom out of range";
        }
        return "unknown";
    }

    void damageOnChange(Monitor::CMonitor& monitor) {
        if (g_pHyprRenderer)
            g_pHyprRenderer->damageMonitor(monitor.m_self.lock());
    }

    bool sameState(const cammath::CameraState& a, const cammath::CameraState& b) {
        return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.zoom == b.zoom;
    }
}

HyprVXSpike::SMonitorCamera& HyprVXSpike::stored(const Monitor::CMonitor& monitor) {
    return cameras()[monitor.m_id];
}

cammath::Vec HyprVXSpike::monitorOrigin(const Monitor::CMonitor& monitor) {
    return {monitor.m_position.x, monitor.m_position.y};
}

cammath::CameraState HyprVXSpike::cameraOf(const Monitor::CMonitor& monitor) {
    const auto& CAMERA = stored(monitor);
    return CAMERA.active ? CAMERA.state : cammath::identityFor(monitorOrigin(monitor));
}

bool HyprVXSpike::cameraIsIdentity(const Monitor::CMonitor& monitor) {
    return cammath::isIdentity(cameraOf(monitor), monitorOrigin(monitor));
}

bool HyprVXSpike::setCamera(Monitor::CMonitor& monitor, const cammath::CameraState& camera) {
    if (const auto error = cammath::validate(camera, CAMERA_ZOOM_LIMITS); error != cammath::Error::None) {
        Log::logger->log(Log::ERR, "[hyprvx] camera rejected on {}: {} (origin {}, {} zoom {})", monitor.m_name, errorName(error), camera.origin.x, camera.origin.y, camera.zoom);
        return false;
    }

    if (sameState(cameraOf(monitor), camera))
        return true;

    auto& CAMERA  = stored(monitor);
    CAMERA.active = !cammath::isIdentity(camera, monitorOrigin(monitor));
    CAMERA.state  = camera;
    CAMERA.revision++;
    damageOnChange(monitor);
    return true;
}

void HyprVXSpike::resetCamera(Monitor::CMonitor& monitor) {
    auto& CAMERA = stored(monitor);
    if (!CAMERA.active)
        return;

    CAMERA.active = false;
    CAMERA.state  = cammath::identityFor(monitorOrigin(monitor));
    CAMERA.revision++;
    damageOnChange(monitor);
}

std::optional<cammath::CameraState> HyprVXSpike::activeCamera(const Monitor::CMonitor& monitor) {
    if (cameraIsIdentity(monitor))
        return std::nullopt;

    const auto& workspace = monitor.m_activeWorkspace;
    if (!workspace || Fullscreen::controller()->hasFullscreen(workspace))
        return std::nullopt;

    return cameraOf(monitor);
}

SRenderModifData HyprVXSpike::cameraRenderModif(const Monitor::CMonitor& monitor, const cammath::CameraState& camera, const SRenderModifData& outer) {
    const auto       M      = monitorOrigin(monitor);
    const auto       ORIGIN = cammath::forward(camera, M, M);

    SRenderModifData modif;
    modif.enabled = outer.enabled;
    modif.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, Vector2D{0, 0}); // marker
    modif.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, sc<float>(camera.zoom));
    modif.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, Vector2D{ORIGIN.x - M.x, ORIGIN.y - M.y} * monitor.m_scale);
    modif.modifs.insert(modif.modifs.end(), outer.modifs.begin(), outer.modifs.end());
    return modif;
}

bool HyprVXSpike::isCameraModif(const SRenderModifData& modif) {
    if (!modif.enabled || modif.modifs.size() < 3)
        return false;

    const auto& [TYPE, VALUE] = modif.modifs.front();
    if (TYPE != SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE || modif.modifs[1].first != SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE)
        return false;

    const auto* OFFSET = std::any_cast<Vector2D>(&VALUE);
    return OFFSET && OFFSET->x == 0 && OFFSET->y == 0;
}

CBox HyprVXSpike::roundOutBox(const CBox& box) {
    const double X0 = std::floor(box.x), Y0 = std::floor(box.y);
    return {X0, Y0, std::ceil(box.x + box.w) - X0, std::ceil(box.y + box.h) - Y0};
}

CBox HyprVXSpike::roundInBox(const CBox& box) {
    const double X0 = std::ceil(box.x), Y0 = std::ceil(box.y);
    return {X0, Y0, std::max(0.0, std::floor(box.x + box.w) - X0), std::max(0.0, std::floor(box.y + box.h) - Y0)};
}

bool HyprVXSpike::cameraShows(const Monitor::CMonitor& monitor, const CBox& globalLogical) {
    const auto CAMERA = activeCamera(monitor);
    if (!CAMERA)
        return false;

    const auto VISIBLE = cammath::visibleWorldRect(*CAMERA, {monitor.m_size.x, monitor.m_size.y});
    return globalLogical.overlaps({VISIBLE.pos.x, VISIBLE.pos.y, VISIBLE.size.x, VISIBLE.size.y});
}

void HyprVXSpike::addCameraDamage(Monitor::CMonitor& monitor, const CBox& globalLogical) {
    const auto CAMERA = activeCamera(monitor);
    if (!CAMERA || globalLogical.empty())
        return;

    const auto M      = monitorOrigin(monitor);
    const auto SCREEN = cammath::damageBox(*CAMERA, M, {{globalLogical.x, globalLogical.y}, {globalLogical.w, globalLogical.h}});
    monitor.addDamage(CBox{SCREEN.pos.x - M.x, SCREEN.pos.y - M.y, SCREEN.size.x, SCREEN.size.y}.scale(monitor.m_scale).round());
}

void HyprVXSpike::addCameraDamage(Monitor::CMonitor& monitor, const CRegion& globalLogical) {
    if (!activeCamera(monitor))
        return;

    globalLogical.forEachRect([&monitor](const auto& RECT) { addCameraDamage(monitor, CBox(RECT.x1, RECT.y1, RECT.x2 - RECT.x1, RECT.y2 - RECT.y1)); });
}

Vector2D HyprVXSpike::screenToWorld(const Vector2D& screen) {
    const auto PMONITOR = State::monitorState()->query().vec(screen).run();
    return PMONITOR ? screenToWorld(*PMONITOR, screen) : screen;
}

Vector2D HyprVXSpike::screenToWorld(const Monitor::CMonitor& monitor, const Vector2D& screen) {
    const auto CAMERA = activeCamera(monitor);
    if (!CAMERA)
        return screen;

    const auto WORLD = cammath::inverse(*CAMERA, monitorOrigin(monitor), {screen.x, screen.y});
    return {WORLD.x, WORLD.y};
}

Vector2D HyprVXSpike::worldToScreen(const Monitor::CMonitor& monitor, const Vector2D& world) {
    const auto CAMERA = activeCamera(monitor);
    if (!CAMERA)
        return world;

    const auto SCREEN = cammath::forward(*CAMERA, monitorOrigin(monitor), {world.x, world.y});
    return {SCREEN.x, SCREEN.y};
}

bool HyprVXSpike::isWindowContent(const SP<CWLSurfaceResource>& surface) {
    const auto HLSURFACE = surface ? Desktop::View::CWLSurface::fromResource(surface) : nullptr;
    const auto VIEW      = HLSURFACE ? HLSURFACE->view() : nullptr;
    if (!VIEW)
        return false;

    switch (VIEW->type()) {
        case Desktop::View::VIEW_TYPE_WINDOW: return true;
        case Desktop::View::VIEW_TYPE_POPUP: {
            const auto POPUP = Desktop::View::CPopup::fromView(VIEW);
            return POPUP && !POPUP->layerOwner();
        }
        case Desktop::View::VIEW_TYPE_SUBSURFACE:
            return !Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_LAYER_SURFACE).surface(surface).runLayer();
        default: return false;
    }
}
