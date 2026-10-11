// Generated for the HyprVX delivery spike (T066) from the patched draft tree (hyprland-draft 43b8c24e = Hyprland
// efb5099 + patches 0001-0009), src/layout/supplementary/DragController.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin-spike/gen_copies.py platform/plugin-spike/src/copied
#include "../SpikeAccess.hpp"

#include <src/layout/supplementary/DragController.hpp>

#include <src/layout/LayoutManager.hpp>
#include <src/layout/space/Space.hpp>

#include <src/Compositor.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/Group.hpp>
#include <src/protocols/XDGShell.hpp>
#include <src/render/Renderer.hpp>
#include <src/state/MonitorState.hpp>

#include <string_view>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

struct SpikeDragController : public Layout::Supplementary::CDragStateController {
        void dragBegin(SP<ITarget> target, eMouseBindMode mode, std::optional<eRectCorner> forcedEdge, bool exclusiveDeviceGrab);

    static void hk_dragBegin(Layout::Supplementary::CDragStateController* self, SP<ITarget> target, eMouseBindMode mode, std::optional<eRectCorner> forcedEdge, bool exclusiveDeviceGrab) {
        static_cast<SpikeDragController*>(self)->dragBegin(target, mode, forcedEdge, exclusiveDeviceGrab);
    }
};

static bool isResizeMode(eMouseBindMode mode) {
    return mode == MBIND_RESIZE || mode == MBIND_RESIZE_FORCE_RATIO || mode == MBIND_RESIZE_BLOCK_RATIO;
}
static std::string_view cursorForResizeEdge(eRectCorner edge) {
    if (edgeTop(edge) && edgeLeft(edge))
        return "nw-resize";
    if (edgeTop(edge) && edgeRight(edge))
        return "ne-resize";
    if (edgeBottom(edge) && edgeRight(edge))
        return "se-resize";
    if (edgeBottom(edge) && edgeLeft(edge))
        return "sw-resize";
    if (edgeTop(edge))
        return "n-resize";
    if (edgeBottom(edge))
        return "s-resize";
    if (edgeLeft(edge))
        return "w-resize";
    if (edgeRight(edge))
        return "e-resize";

    return "se-resize";
}
static void setXDGResizingState(SP<ITarget> target, bool resizing) {
    if (!target || !target->window() || target->window()->m_isX11 || !target->window()->m_xdgSurface || !target->window()->m_xdgSurface->m_toplevel)
        return;

    target->window()->m_xdgSurface->m_toplevel->setResizing(resizing);
}
void SpikeDragController::dragBegin(SP<ITarget> target, eMouseBindMode mode, std::optional<eRectCorner> forcedEdge, bool exclusiveDeviceGrab) {
    // HyprVX: a compositor move/resize drag writes the window's logical geometry. While the camera of the window's monitor
    // is non-identity it is blocked and logged before any drag state is set (FR-012, DV3-11); moving windows in Drift
    // changes the world offset O(w) and belongs to Feature 002.
    if (const auto WINDOW = target ? target->window() : nullptr; WINDOW) {
        if (const auto PMONITOR = WINDOW->m_monitor.lock(); PMONITOR && HyprVXSpike::activeCamera(*PMONITOR)) {
            Log::logger->log(Log::WARN, "HyprVX: blocked a compositor {} drag of window {:x} while the camera is non-identity (FR-012)",
                             isResizeMode(mode) ? "resize" : "move", rc<uintptr_t>(WINDOW.get()));
            return;
        }
    }

    m_target              = target;
    m_dragMode            = mode;
    m_forcedGrabbedCorner = forcedEdge;
    m_exclusiveDeviceGrab = exclusiveDeviceGrab;
    m_grabbedCorner       = CORNER_NONE;

    const auto  DRAGGINGTARGET = m_target.lock();
    static auto PDRAGTHRESHOLD = CConfigValue<Config::INTEGER>("binds:drag_threshold");

    m_mouseMoveEventCount = 1;
    m_beginDragSizeXY     = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!validMapped(DRAGGINGTARGET->window())) {
        Log::logger->log(Log::ERR, "Dragging attempted on an invalid window (not mapped)");
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    if (!DRAGGINGTARGET->workspace()) {
        Log::logger->log(Log::ERR, "Dragging attempted on an invalid window (no workspace)");
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Try to pick up dragged window now if drag_threshold is disabled
    // or at least update dragging related variables for the cursors
    m_dragThresholdReached = *PDRAGTHRESHOLD <= 0;
    if (updateDragWindow())
        return;

    // get the grab corner
    static auto RESIZECORNER = CConfigValue<Config::INTEGER>("general:resize_corner");
    if (m_forcedGrabbedCorner && *m_forcedGrabbedCorner != CORNER_NONE) {
        m_grabbedCorner = *m_forcedGrabbedCorner;
        Pointer::Cursor::overrideController->setOverride(std::string{cursorForResizeEdge(m_grabbedCorner)}, Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
    } else if (*RESIZECORNER != 0 && *RESIZECORNER <= 4 && DRAGGINGTARGET->floating()) {
        switch (*RESIZECORNER) {
            case 1:
                m_grabbedCorner = CORNER_TOPLEFT;
                Pointer::Cursor::overrideController->setOverride("nw-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 2:
                m_grabbedCorner = CORNER_TOPRIGHT;
                Pointer::Cursor::overrideController->setOverride("ne-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 3:
                m_grabbedCorner = CORNER_BOTTOMRIGHT;
                Pointer::Cursor::overrideController->setOverride("se-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
            case 4:
                m_grabbedCorner = CORNER_BOTTOMLEFT;
                Pointer::Cursor::overrideController->setOverride("sw-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
                break;
        }
    } else if (m_beginDragXY.x < m_beginDragPositionXY.x + m_beginDragSizeXY.x / 2.F) {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.F) {
            m_grabbedCorner = CORNER_TOPLEFT;
            Pointer::Cursor::overrideController->setOverride("nw-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        } else {
            m_grabbedCorner = CORNER_BOTTOMLEFT;
            Pointer::Cursor::overrideController->setOverride("sw-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        }
    } else {
        if (m_beginDragXY.y < m_beginDragPositionXY.y + m_beginDragSizeXY.y / 2.F) {
            m_grabbedCorner = CORNER_TOPRIGHT;
            Pointer::Cursor::overrideController->setOverride("ne-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        } else {
            m_grabbedCorner = CORNER_BOTTOMRIGHT;
            Pointer::Cursor::overrideController->setOverride("se-resize", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);
        }
    }

    if (!isResizeMode(m_dragMode))
        Pointer::Cursor::overrideController->setOverride("grabbing", Pointer::Cursor::CURSOR_OVERRIDE_SPECIAL_ACTION);

    if (m_exclusiveDeviceGrab)
        g_pSeatManager->setPointerFocus(nullptr, {});

    DRAGGINGTARGET->damageEntire();

    g_pKeybindManager->shadowKeybinds();

    if (DRAGGINGTARGET->window()) {
        Desktop::focusState()->rawWindowFocus(DRAGGINGTARGET->window(), Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
        Desktop::windowState()->raise(DRAGGINGTARGET->window());
    }

    if (isResizeMode(m_dragMode))
        setXDGResizingState(DRAGGINGTARGET, true);
}

void HyprVXSpike::addCopiedDragControllerHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"dragBegin", "Layout::Supplementary::CDragStateController::dragBegin(Hyprutils::Memory::CSharedPointer<Layout::ITarget>, eMouseBindMode, std::optional<Layout::eRectCorner>, bool)", reinterpret_cast<void*>(&SpikeDragController::hk_dragBegin), nullptr});
}
