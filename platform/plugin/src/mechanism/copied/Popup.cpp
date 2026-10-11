// Generated for the HyprVX delivery spike (T066) from the patched draft tree (hyprland-draft 43b8c24e = Hyprland
// efb5099 + patches 0001-0009), src/desktop/view/Popup.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin-spike/gen_copies.py platform/plugin-spike/src/copied
#include "../SpikeAccess.hpp"

#include <src/desktop/view/Popup.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/config/shared/animation/AnimationTree.hpp>
#include <src/Compositor.hpp>
#include <src/desktop/state/FadingOutState.hpp>
#include <src/desktop/state/PopupFadeout.hpp>
#include <src/protocols/LayerShell.hpp>
#include <src/protocols/XDGShell.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/animation/AnimationManager.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/managers/eventLoop/EventLoopManager.hpp>
#include <src/render/Renderer.hpp>
#include <src/render/OpenGL.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorState.hpp>
#include <array>
#include <ranges>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Desktop;
using namespace Desktop::View;

struct SpikePopup : public Desktop::View::CPopup {
        void onMap();
        void reposition();

    static void hk_onMap(Desktop::View::CPopup* self) {
        static_cast<SpikePopup*>(self)->onMap();
    }
    static void hk_reposition(Desktop::View::CPopup* self) {
        static_cast<SpikePopup*>(self)->reposition();
    }
};

void SpikePopup::onMap() {
    if (m_mapped)
        return;

    m_mapped   = true;
    m_lastSize = m_resource->m_surface->m_surface->m_current.size;

    const auto COORDS = coordsGlobal();
    // HyprVX: a popup mapped outside every monitor (possible in world coordinates, and for any client) falls back to its
    // owner's monitor instead of dereferencing null (spec edge case, research R3).
    auto PMONITOR = State::monitorState()->query().vec(COORDS).run();
    if (!PMONITOR)
        PMONITOR = getMonitor();

    CBox       box = m_wlSurface->resource()->extends();
    box.translate(COORDS).expand(4);
    g_pHyprRenderer->damageBox(box);

    m_lastPos = coordsRelativeToParent();

    invalidateTreeExtentsCache();

    g_pInputManager->simulateMouseMovement();

    m_subsurfaceHead = CSubsurface::create(m_self);

    //unconstrain();
    sendScale();
    if (PMONITOR)
        m_wlSurface->resource()->breadthfirst([PMONITOR](SP<CWLSurfaceResource> s, const Vector2D& offset, void* d) { s->enter(PMONITOR->m_self.lock()); }, nullptr);

    if (!m_layerOwner.expired() && m_layerOwner->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
        if (m_layerOwner->m_monitor)
            m_layerOwner->m_monitor->m_blurFBDirty = true;
    }

    m_alpha.get(POPUP_ALPHA_FADE)->setConfig(Config::animationTree()->getAnimationPropertyConfig("fadePopupsIn"));
    m_alpha.get(POPUP_ALPHA_FADE)->setValueAndWarp(0.F);
    *m_alpha.get(POPUP_ALPHA_FADE) = 1.F;

    Log::logger->log(Log::DEBUG, "popup {:x}: mapped", rc<uintptr_t>(this));
}
void SpikePopup::reposition() {
    const auto COORDS = t1ParentCoords();

    // HyprVX: while the owner window's monitor has an active camera, the parent is placed in world coordinates and the
    // popup is constrained to the world rectangle shown in the monitor's usable area (cammath::inverseBox).
    if (const auto OWNERMONITOR = m_windowOwner ? m_windowOwner->m_monitor.lock() : nullptr; OWNERMONITOR) {
        if (const auto CAMERA = HyprVXSpike::activeCamera(*OWNERMONITOR); CAMERA) {
            const auto AREA  = OWNERMONITOR->logicalBoxMinusReserved();
            const auto WORLD = hyprvx::cammath::inverseBox(*CAMERA, HyprVXSpike::monitorOrigin(*OWNERMONITOR), {{AREA.x, AREA.y}, {AREA.w, AREA.h}});
            m_resource->applyPositioning(CBox{WORLD.pos.x, WORLD.pos.y, WORLD.size.x, WORLD.size.y}, COORDS);
            return;
        }
    }

    const auto PMONITOR = State::monitorState()->query().vec(COORDS).run();

    if (!PMONITOR)
        return;

    m_resource->applyPositioning(m_windowOwner ? PMONITOR->logicalBoxMinusReserved() : PMONITOR->logicalBox(), COORDS);
}

void HyprVXSpike::addCopiedPopupHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"onMap", "Desktop::View::CPopup::onMap()", reinterpret_cast<void*>(&SpikePopup::hk_onMap), nullptr});
    specs.push_back({"reposition", "Desktop::View::CPopup::reposition()", reinterpret_cast<void*>(&SpikePopup::hk_reposition), nullptr});
}
