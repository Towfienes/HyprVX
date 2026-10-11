// Generated for the HyprVX plugin form S (Feature 002 T052; delivery spike T066) from the patched draft tree
// (hyprland-draft bf59c232 = Hyprland efb5099 + patches 0001-0013), src/desktop/view/Window.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: warpCursor.
// Copy-only (not hooked; called by a wrapper as HyprVXSpike::copied_<name>): none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied
#include "../SpikeAccess.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <span>
#include <hyprutils/animation/AnimatedVariable.hpp>
#include <re2/re2.h>

#include <src/desktop/view/Group.hpp>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <any>
#include <bit>
#include <fstream>
#include <string_view>
#include <src/desktop/view/Window.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/FloatState.hpp>
#include <src/desktop/state/FadingOutState.hpp>
#include <src/desktop/state/GlobalWindowController.hpp>
#include <src/desktop/state/WindowFadeout.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/history/WindowHistoryTracker.hpp>
#include <src/Compositor.hpp>
#include <src/render/decorations/CHyprDropShadowDecoration.hpp>
#include <src/render/decorations/CHyprInnerGlowDecoration.hpp>
#include <src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <src/render/decorations/CHyprBorderDecoration.hpp>
#include <src/config/ConfigValue.hpp>
#include <src/config/shared/actions/ConfigActions.hpp>
#include <src/config/ConfigManager.hpp>
#include <src/config/shared/animation/AnimationTree.hpp>
#include <src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/WorkspaceState.hpp>
#include <src/managers/TokenManager.hpp>
#include <src/animation/AnimationManager.hpp>
#include <src/managers/ANRManager.hpp>
#include <src/managers/eventLoop/EventLoopManager.hpp>
#include <src/managers/eventLoop/EventLoopTimer.hpp>
#include <src/protocols/XDGShell.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/protocols/core/Subcompositor.hpp>
#include <src/protocols/ContentType.hpp>
#include <src/protocols/LayerShell.hpp>
#include <src/xwayland/XWayland.hpp>
#include <src/helpers/Color.hpp>
#include <src/helpers/math/Expression.hpp>
#include <src/managers/XWaylandManager.hpp>
#include <src/render/Renderer.hpp>
#include <src/render/transformer/MotionBlurTransformer.hpp>
#include <src/managers/EventManager.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/pointer/PointerController.hpp>
#include <src/managers/KeybindManager.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/layout/algorithm/Algorithm.hpp>
#include <src/layout/space/Space.hpp>
#include <src/layout/LayoutManager.hpp>
#include <src/layout/target/WindowTarget.hpp>
#include <src/layout/target/WindowGroupTarget.hpp>
#include <src/event/EventBus.hpp>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/VarList2.hpp>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Hyprutils::String;
using namespace Hyprutils::Animation;
using namespace Desktop;
using namespace Desktop::View;

struct SpikeWindow : public Desktop::View::CWindow {
        bool checkInputOnDecos(const eInputType type, const Vector2D& screenCoords, std::any data);
        bool visibleOnMonitor(PHLMONITOR pMonitor);
        void warpCursor(bool force);

    static bool hk_checkInputOnDecos(Desktop::View::CWindow* self, const eInputType type, const Vector2D& screenCoords, std::any data) {
        return static_cast<SpikeWindow*>(self)->checkInputOnDecos(type, screenCoords, data);
    }
    static bool hk_visibleOnMonitor(Desktop::View::CWindow* self, PHLMONITOR pMonitor) {
        return static_cast<SpikeWindow*>(self)->visibleOnMonitor(pMonitor);
    }
    static void hk_warpCursor(Desktop::View::CWindow* self, bool force) {
        static_cast<SpikeWindow*>(self)->warpCursor(force);
    }
};

bool SpikeWindow::checkInputOnDecos(const eInputType type, const Vector2D& screenCoords, std::any data) {
    // HyprVX: decoration boxes and popups are window content, selected at the world point (FR-012), in the window's L
    // space (Feature 002: minus its world offset).
    const auto mouseCoords = HyprVXSpike::screenToWorld(screenCoords) - HyprVXSpike::appliedOffset(*this);

    if (type != INPUT_TYPE_DRAG_END && hasPopupAt(mouseCoords))
        return false;

    for (auto const& wd : m_windowDecorations) {
        if (!(wd->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT))
            continue;

        if (!g_pDecorationPositioner->getWindowDecorationBox(wd.get()).containsPoint(mouseCoords))
            continue;

        if (wd->onInputOnDeco(type, mouseCoords, data))
            return true;
    }

    return false;
}
bool SpikeWindow::visibleOnMonitor(PHLMONITOR pMonitor) {
    CBox wbox = {m_realPosition->value(), m_realSize->value()};

    if (m_isFloating)
        wbox = getFullWindowBoundingBox();

    // HyprVX (Feature 002 research R8): culled at the world box L + O(w)
    wbox.translate(HyprVXSpike::appliedOffset(*this));

    // HyprVX (FR-006): under a camera the monitor shows its visible world rect instead of its logical rect
    if UNLIKELY (const auto CAMERA = HyprVXSpike::activeCamera(*pMonitor); CAMERA) {
        const auto VISIBLE = hyprvx::cammath::visibleWorldRect(*CAMERA, {pMonitor->m_size.x, pMonitor->m_size.y});
        return !wbox.intersection({VISIBLE.pos.x, VISIBLE.pos.y, VISIBLE.size.x, VISIBLE.size.y}).empty();
    }

    return !wbox.intersection({pMonitor->m_position, pMonitor->m_size}).empty();
}
void SpikeWindow::warpCursor(bool force) {
    // HyprVX spike prologue (gen_copies.py PROLOGUES)
    if (const auto PMON = m_monitor.lock(); PMON && m_relativeCursorCoordsOnLastWarp.x > 0 && m_relativeCursorCoordsOnLastWarp.y > 0)
        m_relativeCursorCoordsOnLastWarp =
            HyprVXSpike::screenToWorld(*PMON, m_relativeCursorCoordsOnLastWarp + layoutBox().pos()) - HyprVXSpike::appliedOffset(*this) - layoutBox().pos();
    static auto PERSISTENTWARPS        = CConfigValue<Config::INTEGER>("cursor:persistent_warps");
    const auto  coords                 = m_relativeCursorCoordsOnLastWarp;
    m_relativeCursorCoordsOnLastWarp.x = -1; // reset m_vRelativeCursorCoordsOnLastWarp

    const auto BOX      = layoutBox();
    const auto PMONITOR = m_monitor.lock();
    // HyprVX: warp to where the window is drawn (cammath::forward); unchanged without a camera.
    // Feature 002: the window is drawn at its world box L + O(w).
    const auto OFFSET   = HyprVXSpike::appliedOffset(*this);
    const auto toScreen = [&PMONITOR, &OFFSET](const Vector2D& world) { return PMONITOR ? HyprVXSpike::worldToScreen(*PMONITOR, world + OFFSET) : world + OFFSET; };

    if (*PERSISTENTWARPS && coords.x > 0 && coords.y > 0 && coords < BOX.size()) // don't warp cursor outside the window
        Pointer::pointerController()->warpTo(toScreen(BOX.pos() + coords), force);
    else
        Pointer::pointerController()->warpTo(toScreen(middle()), force);
}


void HyprVXSpike::addCopiedWindowHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"checkInputOnDecos", "Desktop::View::CWindow::checkInputOnDecos(eInputType, Hyprutils::Math::Vector2D const&, std::any)", reinterpret_cast<void*>(&SpikeWindow::hk_checkInputOnDecos), nullptr});
    specs.push_back({"visibleOnMonitor", "Desktop::View::CWindow::visibleOnMonitor(Hyprutils::Memory::CSharedPointer<Monitor::CMonitor>)", reinterpret_cast<void*>(&SpikeWindow::hk_visibleOnMonitor), nullptr});
    specs.push_back({"warpCursor", "Desktop::View::CWindow::warpCursor(bool)", reinterpret_cast<void*>(&SpikeWindow::hk_warpCursor), nullptr});
}
