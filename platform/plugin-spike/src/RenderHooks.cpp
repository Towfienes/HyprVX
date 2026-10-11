// Hand-written wrapper hooks for the delivery spike: the patch hunks that only add code around an unchanged stock
// body (0002 renderMonitor noSimplify, camera hint around the window pass, DS/solitary blocks; 0004 damage union).
// Each calls the original and adds what the patch adds.

#include "Hooks.hpp"
#include "Camera.hpp"

#include <src/desktop/Workspace.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/output/Monitor.hpp>
#include <src/render/Renderer.hpp>
#include <src/render/pass/RendererHintsPassElement.hpp>
#include <src/state/MonitorState.hpp>

using namespace Render;

namespace {
    CFunctionHook* g_renderMonitor     = nullptr;
    CFunctionHook* g_renderAllClients  = nullptr;
    CFunctionHook* g_renderWsWindows   = nullptr;
    CFunctionHook* g_isDSBlocked       = nullptr;
    CFunctionHook* g_isSolitaryBlocked = nullptr;
    CFunctionHook* g_damageBox         = nullptr;
    CFunctionHook* g_damageWindow      = nullptr;

    // renderAllClientsForWorkspace's workspace modifier (its local RENDERMODIFDATA), rebuilt from its arguments the way
    // stock builds it, for the camera hint pushed around renderWorkspaceWindows.
    struct SPassContext {
        PHLMONITOR       monitor;
        PHLWORKSPACE     workspace;
        SRenderModifData outer;
    };
    std::optional<SPassContext> g_pass;

    void hkRenderMonitor(IHyprRenderer* self, PHLMONITOR pMonitor, bool commit) {
        // 0002 (RC2): no pass simplification while a camera applies.
        const bool NOSIMPLIFY = !self->m_renderData.noSimplify && HyprVXSpike::activeCamera(*pMonitor).has_value();
        if UNLIKELY (NOSIMPLIFY)
            self->m_renderData.noSimplify = true;
        HyprVXSpike::original<void (*)(IHyprRenderer*, PHLMONITOR, bool)>(g_renderMonitor)(self, pMonitor, commit);
        if UNLIKELY (NOSIMPLIFY)
            self->m_renderData.noSimplify = false;
    }

    void hkRenderAllClients(IHyprRenderer* self, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time, const Vector2D& translate, const float& scale) {
        SRenderModifData outer;
        if (translate != Vector2D{0, 0})
            outer.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, translate);
        if (scale != 1.f)
            outer.modifs.emplace_back(SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, scale);

        auto saved = std::exchange(g_pass, SPassContext{pMonitor, pWorkspace, outer});
        HyprVXSpike::original<void (*)(IHyprRenderer*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&, const Vector2D&, const float&)>(g_renderAllClients)(
            self, pMonitor, pWorkspace, time, translate, scale);
        g_pass = std::move(saved);
    }

    void hkRenderWorkspaceWindows(IHyprRenderer* self, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& time) {
        // 0002: camera hint over the window pass of the monitor's active workspace. The patch also covers the special
        // workspace windows rendered inline in renderAllClientsForWorkspace; a wrapper cannot reach those (known gap).
        std::optional<hyprvx::cammath::CameraState> camera;
        if (g_pass && g_pass->monitor == pMonitor && g_pass->workspace == pWorkspace && pWorkspace == pMonitor->m_activeWorkspace)
            camera = HyprVXSpike::activeCamera(*pMonitor);

        if UNLIKELY (camera) {
            self->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{HyprVXSpike::cameraRenderModif(*pMonitor, *camera, g_pass->outer)}));
            HyprVXSpike::stored(*pMonitor).passElementsAdded++;
        }
        HyprVXSpike::original<void (*)(IHyprRenderer*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&)>(g_renderWsWindows)(self, pMonitor, pWorkspace, time);
        if UNLIKELY (camera) {
            self->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{g_pass->outer}));
            HyprVXSpike::stored(*pMonitor).passElementsAdded++;
        }
    }

    uint16_t hkIsDSBlocked(Monitor::CMonitor* self, bool full) {
        auto reasons = HyprVXSpike::original<uint16_t (*)(Monitor::CMonitor*, bool)>(g_isDSBlocked)(self, full);
        if UNLIKELY (!HyprVXSpike::cameraIsIdentity(*self) && (full || !reasons))
            reasons |= Monitor::CMonitor::DS_BLOCK_TRANSFORM;
        return reasons;
    }

    uint32_t hkIsSolitaryBlocked(Monitor::CMonitor* self, bool full) {
        auto reasons = HyprVXSpike::original<uint32_t (*)(Monitor::CMonitor*, bool)>(g_isSolitaryBlocked)(self, full);
        if UNLIKELY (!HyprVXSpike::cameraIsIdentity(*self) && (full || !reasons))
            reasons |= Monitor::CMonitor::SC_TRANSFORM;
        return reasons;
    }

    void hkDamageBox(IHyprRenderer* self, const CBox& box, bool skipFrameSchedule) {
        HyprVXSpike::original<void (*)(IHyprRenderer*, const CBox&, bool)>(g_damageBox)(self, box, skipFrameSchedule);
        // 0004: damage-union rule, on the monitors the stock loop damages.
        if (skipFrameSchedule)
            return;
        for (auto const& m : State::monitorState()->monitors()) {
            if (!m->isMirror())
                HyprVXSpike::addCameraDamage(*m, box);
        }
    }

    void hkDamageWindow(IHyprRenderer* self, PHLWINDOW pWindow, bool forceFull) {
        HyprVXSpike::original<void (*)(IHyprRenderer*, PHLWINDOW, bool)>(g_damageWindow)(self, pWindow, forceFull);
        // 0004: damage-union rule with the stock window box, on the monitors the stock loop damages.
        CBox       windowBox        = pWindow->getFullWindowBoundingBox();
        const auto PWINDOWWORKSPACE = pWindow->m_workspace;
        if (PWINDOWWORKSPACE && PWINDOWWORKSPACE->m_renderOffset->isBeingAnimated() && !pWindow->m_pinned)
            windowBox.translate(PWINDOWWORKSPACE->m_renderOffset->value());
        windowBox.translate(pWindow->m_floatingOffset);
        for (auto const& m : State::monitorState()->monitors()) {
            if (forceFull || self->shouldRenderWindow(pWindow, m))
                HyprVXSpike::addCameraDamage(*m, windowBox);
        }
    }
}

void HyprVXSpike::addRenderWrapperHooks(std::vector<SHookSpec>& specs) {
    const std::string MON = "Hyprutils::Memory::CSharedPointer<Monitor::CMonitor>";
    const std::string WS  = "Hyprutils::Memory::CSharedPointer<CWorkspace>";
    const std::string TP  = "std::chrono::time_point<std::chrono::_V2::steady_clock, std::chrono::duration<long, std::ratio<1l, 1000000000l> > > const&";
    specs.push_back({"renderMonitor", "Render::IHyprRenderer::renderMonitor(" + MON + ", bool)", reinterpret_cast<void*>(&hkRenderMonitor), &g_renderMonitor});
    specs.push_back({"renderAllClientsForWorkspace",
                     "Render::IHyprRenderer::renderAllClientsForWorkspace(" + MON + ", " + WS + ", " + TP + ", Hyprutils::Math::Vector2D const&, float const&)",
                     reinterpret_cast<void*>(&hkRenderAllClients), &g_renderAllClients});
    specs.push_back({"renderWorkspaceWindows", "Render::IHyprRenderer::renderWorkspaceWindows(" + MON + ", " + WS + ", " + TP + ")", reinterpret_cast<void*>(&hkRenderWorkspaceWindows),
                     &g_renderWsWindows});
    specs.push_back({"isDSBlocked", "Monitor::CMonitor::isDSBlocked(bool)", reinterpret_cast<void*>(&hkIsDSBlocked), &g_isDSBlocked});
    specs.push_back({"isSolitaryBlocked", "Monitor::CMonitor::isSolitaryBlocked(bool)", reinterpret_cast<void*>(&hkIsSolitaryBlocked), &g_isSolitaryBlocked});
    specs.push_back({"damageBox", "Render::IHyprRenderer::damageBox(Hyprutils::Math::CBox const&, bool)", reinterpret_cast<void*>(&hkDamageBox), &g_damageBox});
    specs.push_back({"damageWindow", "Render::IHyprRenderer::damageWindow(Hyprutils::Memory::CSharedPointer<Desktop::View::CWindow>, bool)", reinterpret_cast<void*>(&hkDamageWindow),
                     &g_damageWindow});
}
