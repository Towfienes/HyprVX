// Form S world offsets (Feature 002 T053); see WindowOffset.hpp. Ported from patch 0010 src/hyprvx/WindowOffset.cpp and
// the 0010/0012 additions to SceneCamera.cpp and InputCamera.cpp: same storage (weak window refs, no member added to
// CWindow), same damage and fullscreen rules. The camera parts are the delivery spike's (Camera.cpp).

#include "SpikeAccess.hpp"

#include "WindowOffset.hpp"
#include "Camera.hpp"

#include <src/debug/log/Logger.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/desktop/view/Popup.hpp>
#include <src/desktop/view/WLSurface.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/output/Monitor.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/protocols/core/Subcompositor.hpp>
#include <src/render/Renderer.hpp>

#include <cmath>
#include <vector>

using namespace hyprvx;
using namespace Render;

namespace {
    struct SEntry {
        PHLWINDOWREF window;
        Vector2D     offset;
    };

    std::vector<SEntry>& entries() {
        static std::vector<SEntry> ENTRIES;
        return ENTRIES;
    }

    void purge() {
        std::erase_if(entries(), [](const SEntry& e) { return e.window.expired(); });
    }

    const SEntry* find(const Desktop::View::CWindow* window) {
        for (auto const& e : entries()) {
            if (!e.window.expired() && e.window.get() == window)
                return &e;
        }
        return nullptr;
    }

    void damageDrawn(const PHLWINDOW& window) {
        if (!g_pHyprRenderer || !window)
            return;
        g_pHyprRenderer->damageWindow(window, true);
        if (window->m_popupHead) {
            window->m_popupHead->breadthfirst(
                [](SP<Desktop::View::CPopup> popup, void*) {
                    if (popup->m_mapped)
                        g_pHyprRenderer->damageBox(CBox{popup->coordsGlobal(), popup->size()}.expand(1));
                },
                nullptr);
        }
    }
}

bool HyprVXSpike::setWindowOffset(const PHLWINDOW& window, const Vector2D& offset) {
    if (!window)
        return false;
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) {
        Log::logger->log(Log::ERR, "[hyprvx] window offset rejected for {:x}: non-finite ({}, {})", rc<uintptr_t>(window.get()), offset.x, offset.y);
        return false;
    }

    purge();
    const auto* ENTRY = find(window.get());
    if ((ENTRY ? ENTRY->offset : Vector2D{}) == offset)
        return true;

    damageDrawn(window);
    std::erase_if(entries(), [&window](const SEntry& e) { return e.window.get() == window.get(); });
    if (offset != Vector2D{})
        entries().push_back({window, offset});
    damageDrawn(window);
    return true;
}

Vector2D HyprVXSpike::windowOffset(const PHLWINDOW& window) {
    const auto* ENTRY = window ? find(window.get()) : nullptr;
    return ENTRY ? ENTRY->offset : Vector2D{};
}

PHLWINDOW HyprVXSpike::contentOwner(const SP<CWLSurfaceResource>& surface) {
    SP<CWLSurfaceResource> root = surface;
    for (int depth = 0; root && root->m_role && root->m_role->role() == SURFACE_ROLE_SUBSURFACE && depth < 32; ++depth) {
        const auto SUBSURFACE = sc<CSubsurfaceRole*>(root->m_role.get())->m_subsurface.lock();
        root                  = SUBSURFACE ? SUBSURFACE->m_parent.lock() : nullptr;
    }

    const auto HLSURFACE = root ? Desktop::View::CWLSurface::fromResource(root) : nullptr;
    const auto VIEW      = HLSURFACE ? HLSURFACE->view() : nullptr;
    if (!VIEW)
        return nullptr;

    switch (VIEW->type()) {
        case Desktop::View::VIEW_TYPE_WINDOW: return Desktop::View::CWindow::fromView(VIEW);
        case Desktop::View::VIEW_TYPE_POPUP: {
            const auto POPUP = Desktop::View::CPopup::fromView(VIEW);
            if (!POPUP || POPUP->layerOwner())
                return nullptr;
            const auto T1 = POPUP->getT1Owner();
            return T1 && T1->view() ? Desktop::View::CWindow::fromView(T1->view()) : nullptr;
        }
        default: return nullptr;
    }
}

Vector2D HyprVXSpike::contentOffset(const SP<CWLSurfaceResource>& surface) {
    if (entries().empty())
        return {};
    return windowOffset(contentOwner(surface));
}

void HyprVXSpike::resetWindowOffsets() {
    purge();
    std::vector<PHLWINDOW> windows;
    for (auto const& e : entries())
        windows.emplace_back(e.window.lock());
    for (auto const& w : windows)
        damageDrawn(w);
    entries().clear();
    for (auto const& w : windows)
        damageDrawn(w);
}

Vector2D HyprVXSpike::appliedOffset(const Desktop::View::CWindow& window) {
    if LIKELY (entries().empty())
        return {};
    const auto* ENTRY = find(&window);
    if (!ENTRY)
        return {};
    if (window.m_workspace && Fullscreen::controller()->hasFullscreen(window.m_workspace))
        return {};
    return ENTRY->offset;
}

Vector2D HyprVXSpike::appliedContentOffset(const SP<CWLSurfaceResource>& surface) {
    if LIKELY (entries().empty())
        return {};
    const auto OWNER = contentOwner(surface);
    return OWNER ? appliedOffset(*OWNER) : Vector2D{};
}

bool HyprVXSpike::anyWindowOffset() {
    if LIKELY (entries().empty())
        return false;
    purge();
    return !entries().empty();
}

void HyprVXSpike::addOffsetDamage(Monitor::CMonitor& monitor, const CBox& globalLogical) {
    if LIKELY (entries().empty() || globalLogical.empty())
        return;

    const bool CAMERA = activeCamera(monitor).has_value();
    for (auto const& e : entries()) {
        const auto WINDOW = e.window.lock();
        if (!WINDOW)
            continue;
        const auto OFFSET = appliedOffset(*WINDOW);
        if (OFFSET == Vector2D{})
            continue;

        const auto MOVED = globalLogical.copy().translate(OFFSET);
        if (CAMERA)
            addCameraDamage(monitor, MOVED);
        else
            monitor.addDamage(MOVED.copy().translate(-monitor.m_position).scale(monitor.m_scale).round());
    }
}

std::vector<std::pair<PHLWINDOW, Vector2D>> HyprVXSpike::windowOffsets() {
    purge();
    std::vector<std::pair<PHLWINDOW, Vector2D>> out;
    for (auto const& e : entries())
        out.emplace_back(e.window.lock(), e.offset);
    return out;
}

HyprVXSpike::SWindowPass& HyprVXSpike::windowPass() {
    static SWindowPass PASS;
    return PASS;
}

SRenderModifData HyprVXSpike::windowRenderModif(const Monitor::CMonitor& monitor, const Vector2D& offset) {
    const auto& PASS   = windowPass();
    const auto  CAMERA = PASS.camera.value_or(cammath::identityFor(monitorOrigin(monitor)));
    return cameraRenderModif(monitor, cammath::offsetCamera(CAMERA, {offset.x, offset.y}), PASS.outer);
}

SRenderModifData HyprVXSpike::passRenderModif(const Monitor::CMonitor& monitor) {
    const auto& PASS = windowPass();
    return PASS.camera ? cameraRenderModif(monitor, *PASS.camera, PASS.outer) : PASS.outer;
}

void HyprVXSpike::notePassElements(Monitor::CMonitor& monitor, uint64_t count) {
    stored(monitor).passElementsAdded += count;
}

Vector2D HyprVXSpike::windowLocalPoint(const Desktop::View::CWindow& window, const Vector2D& screen) {
    return screenToWorld(screen) - appliedOffset(window);
}
