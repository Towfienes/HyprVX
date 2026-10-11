// Generated for the HyprVX plugin form S (Feature 002 T052; delivery spike T066) from the patched draft tree
// (hyprland-draft bf59c232 = Hyprland efb5099 + patches 0001-0013), src/render/Renderer.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Copy-only (not hooked; called by a wrapper as HyprVXSpike::copied_<name>): renderWorkspaceWindows.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied
#include "../SpikeAccess.hpp"

#include <src/render/Renderer.hpp>
#include <src/Compositor.hpp>
#include <src/helpers/math/Math.hpp>
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <cmath>
#include <filesystem>
#include <src/config/ConfigValue.hpp>
#include <src/config/legacy/ConfigManager.hpp>
#include <src/pointer/cursor/CursorManager.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/animation/AnimationManager.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/desktop/view/GlobalViewMethods.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/FadingOutState.hpp>
#include <src/protocols/SessionLock.hpp>
#include <src/protocols/LayerShell.hpp>
#include <src/protocols/XDGShell.hpp>
#include <src/protocols/PresentationTime.hpp>
#include <src/protocols/core/DataDevice.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/protocols/DRMSyncobj.hpp>
#include <src/protocols/LinuxDMABUF.hpp>
#include <src/protocols/InputCapture.hpp>
#include <src/errorOverlay/Overlay.hpp>
#include <src/debug/Overlay.hpp>
#include <src/notification/NotificationOverlay.hpp>
#include <src/layout/LayoutManager.hpp>
#include <src/layout/space/Space.hpp>
#include <src/i18n/Engine.hpp>
#include <src/desktop/DesktopTypes.hpp>
#include <src/event/EventBus.hpp>
#include <src/helpers/CursorShapes.hpp>
#include <src/helpers/MainLoopExecutor.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/WorkspaceState.hpp>
#include <src/macros.hpp>
#include <src/render/pass/TexPassElement.hpp>
#include <src/render/pass/ClearPassElement.hpp>
#include <src/render/pass/RectPassElement.hpp>
#include <src/render/pass/RendererHintsPassElement.hpp>
#include <src/render/pass/SurfacePassElement.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/protocols/ColorManagement.hpp>
#include <src/protocols/types/ContentType.hpp>
#include <src/render/AsyncResourceGatherer.hpp>
#include <src/render/ElementRenderer.hpp>
#include <src/render/Framebuffer.hpp>
#include <src/render/OpenGL.hpp>
#include <src/render/Texture.hpp>
#include <src/render/pass/PreBlurElement.hpp>
#include <src/protocols/types/SurfaceState.hpp>
#include <src/render/types.hpp>
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Mat3x3.hpp>
#include <hyprutils/math/Region.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <optional>
#include <pango/pangocairo.h>
#include <type_traits>
#include <unordered_map>

#include <hyprutils/utils/ScopeGuard.hpp>
#include <random>

#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using namespace NColorManagement;
using namespace Desktop::View;
using namespace Render;

struct SpikeRenderer : public Render::IHyprRenderer {
        void damageSurface(SP<CWLSurfaceResource> pSurface, double x, double y, double scale);
        void renderWorkspaceWindows(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time);

    static void hk_damageSurface(Render::IHyprRenderer* self, SP<CWLSurfaceResource> pSurface, double x, double y, double scale) {
        static_cast<SpikeRenderer*>(self)->damageSurface(pSurface, x, y, scale);
    }
};

void SpikeRenderer::damageSurface(SP<CWLSurfaceResource> pSurface, double x, double y, double scale) {
    if (!pSurface)
        return; // wut?

    const auto WLSURF = Desktop::View::CWLSurface::fromResource(pSurface);
    if (!WLSURF) {
        Log::logger->log(Log::ERR, "BUG THIS: No CWLSurface for surface in damageSurface!!!");
        return;
    }

    // HyprVX (Feature 002 research R8): window content is drawn at its owner window's world offset
    const auto HYPRVXOFFSET = HyprVXSpike::appliedContentOffset(pSurface);

    // hack: schedule frame events
    if (!WLSURF->resource()->m_current.callbacks.empty() && pSurface->m_hlSurface) {
        auto BOX = pSurface->m_hlSurface->getSurfaceBoxGlobal();
        if (BOX)
            BOX->translate(HYPRVXOFFSET);
        if (BOX && !BOX->empty()) {
            for (auto const& m : State::monitorState()->monitors()) {
                if (!m->m_output)
                    continue;

                if (BOX->overlaps(m->logicalBox()) || HyprVXSpike::cameraShows(*m, *BOX))
                    m->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
            }
        }
    }

    CRegion damageBox = WLSURF->computeDamage();
    if (damageBox.empty())
        return;

    if (scale != 1.0)
        damageBox.scale(scale);

    damageBox.translate({x + HYPRVXOFFSET.x, y + HYPRVXOFFSET.y});

    const auto EXTENTS = damageBox.getExtents();

    CRegion    damageBoxForEach;

    for (auto const& m : State::monitorState()->monitors()) {
        if (!m->m_output)
            continue;

        // HyprVX (FR-005): damage-union rule, see HyprVXSpike::addCameraDamage
        HyprVXSpike::addCameraDamage(*m, damageBox);

        if (!EXTENTS.overlaps(m->logicalBox()))
            continue;

        damageBoxForEach.set(damageBox);
        damageBoxForEach.translate({-m->m_position.x, -m->m_position.y}).scale(m->m_scale);

        m->addDamage(damageBoxForEach);
    }

    static auto PLOGDAMAGE = CConfigValue<Config::INTEGER>("debug:log_damage");

    if (*PLOGDAMAGE)
        Log::logger->log(Log::DEBUG, "Damage: Surface (extents): xy: {}, {} wh: {}, {}", damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y1,
                         damageBox.pixman()->extents.x2 - damageBox.pixman()->extents.x1, damageBox.pixman()->extents.y2 - damageBox.pixman()->extents.y1);
}
void SpikeRenderer::renderWorkspaceWindows(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    PHLWINDOW lastWindow;

    // HyprVX (Feature 002 research R1): a window with a world offset is drawn under its own offset camera hint, then the
    // pass hint returns. Nothing is added for a window without an offset (FR-003).
    const auto renderSceneWindow = [&](PHLWINDOW w, eRenderPassMode mode) {
        const auto OFFSET = HyprVXSpike::windowPass().active ? HyprVXSpike::appliedOffset(*w) : Vector2D{};
        if LIKELY (OFFSET == Vector2D{}) {
            renderWindow(w, pMonitor, time, true, mode);
            return;
        }
        HyprVXSpike::notePassElements(*pMonitor, 2);
        m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{HyprVXSpike::windowRenderModif(*pMonitor, OFFSET)}));
        renderWindow(w, pMonitor, time, true, mode);
        m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{HyprVXSpike::passRenderModif(*pMonitor)}));
    };

    Event::bus()->m_events.render.stage.emit(RENDER_PRE_WINDOWS);

    std::vector<PHLWINDOWREF> windows;
    windows.reserve(Desktop::windowState()->windows().size());

    for (auto const& w : Desktop::windowState()->windows()) {
        const bool isNotRenderable = w->isHidden() || !w->m_isMapped;

        if (isNotRenderable)
            continue;

        if (!shouldRenderWindow(w, pMonitor))
            continue;

        windows.emplace_back(w);
    }

    // Non-floating main
    for (auto& w : windows) {
        if (w->m_isFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render active window after all others of this pass
        if (w == Desktop::focusState()->window()) {
            lastWindow = w.lock();
            continue;
        }

        // render the bad boy
        renderSceneWindow(w.lock(), RENDER_PASS_MAIN);
        w.reset();
    }

    if (lastWindow)
        renderSceneWindow(lastWindow, RENDER_PASS_MAIN);

    lastWindow.reset();

    renderFadeouts(pMonitor, Desktop::FADEOUT_PLANE_WINDOW_TILED, pWorkspace);

    // Non-floating popup
    for (auto& w : windows) {
        if (!w)
            continue;

        if (w->m_isFloating)
            continue; // floating are in the second pass

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        // render the bad boy
        renderSceneWindow(w.lock(), RENDER_PASS_POPUP);
        w.reset();
    }

    // floating on top
    for (auto& w : windows) {
        if (!w)
            continue;

        if (!w->m_isFloating || w->m_pinned)
            continue;

        // some things may force us to ignore the special/not special disparity
        const bool IGNORE_SPECIAL_CHECK = w->m_monitorMovedFrom != -1 && (w->m_workspace && !w->m_workspace->isVisible());

        if (!IGNORE_SPECIAL_CHECK && pWorkspace->m_isSpecialWorkspace != w->onSpecialWorkspace())
            continue;

        if (pWorkspace->m_isSpecialWorkspace && w->m_monitor != pWorkspace->m_monitor)
            continue; // special on another are rendered as a part of the base pass

        // render the bad boy
        renderSceneWindow(w.lock(), RENDER_PASS_ALL);
    }
    renderFadeouts(pMonitor, Desktop::FADEOUT_PLANE_WINDOW_FLOATING, pWorkspace);
}

void HyprVXSpike::copied_renderWorkspaceWindows(Render::IHyprRenderer* self, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
    static_cast<SpikeRenderer*>(self)->renderWorkspaceWindows(pMonitor, pWorkspace, time);
}

void HyprVXSpike::addCopiedRendererHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"damageSurface", "Render::IHyprRenderer::damageSurface(Hyprutils::Memory::CSharedPointer<CWLSurfaceResource>, double, double, double)", reinterpret_cast<void*>(&SpikeRenderer::hk_damageSurface), nullptr});
}
