// Generated for the HyprVX plugin form S (Feature 002 T052; delivery spike T066) from the patched draft tree
// (hyprland-draft bf59c232 = Hyprland efb5099 + patches 0001-0013), src/layout/supplementary/DragController.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Copy-only (not hooked; called by a wrapper as HyprVXSpike::copied_<name>): none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied
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
        bool updateDragWindow();
        void mouseMove(const Vector2D& screenPos);

    static void hk_dragBegin(Layout::Supplementary::CDragStateController* self, SP<ITarget> target, eMouseBindMode mode, std::optional<eRectCorner> forcedEdge, bool exclusiveDeviceGrab) {
        static_cast<SpikeDragController*>(self)->dragBegin(target, mode, forcedEdge, exclusiveDeviceGrab);
    }
    static bool hk_updateDragWindow(Layout::Supplementary::CDragStateController* self) {
        return static_cast<SpikeDragController*>(self)->updateDragWindow();
    }
    static void hk_mouseMove(Layout::Supplementary::CDragStateController* self, const Vector2D& screenPos) {
        static_cast<SpikeDragController*>(self)->mouseMove(screenPos);
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
    // HyprVX: a compositor drag writes the window's logical geometry. While the camera of the window's monitor is
    // non-identity or the window has a world offset:
    //  - a move drag is blocked and logged before any drag state is set (FR-012, DV3-11); moving a window in Drift
    //    changes its world offset O(w) (policy, Feature 002);
    //  - a resize drag of a floating window runs through the camera (Feature 002 FR-009): the pointer is taken in the
    //    window's L space (HyprVXSpike::windowLocalPoint), so the anchor uses L + O and the delta is worldDelta(C, d);
    //  - a resize drag of a tiled window is blocked and logged once per drag.
    if (const auto WINDOW = target ? target->window() : nullptr; WINDOW) {
        const auto PMONITOR = WINDOW->m_monitor.lock();
        if ((PMONITOR && HyprVXSpike::activeCamera(*PMONITOR)) || HyprVXSpike::appliedOffset(*WINDOW) != Vector2D{}) {
            if (!isResizeMode(mode) || !target->floating()) {
                Log::logger->log(Log::WARN, "HyprVX: blocked a compositor {} drag of {} window {:x} under a camera or world offset (FR-012, FR-009)",
                                 isResizeMode(mode) ? "resize" : "move", target->floating() ? "floating" : "tiled", rc<uintptr_t>(WINDOW.get()));
                return;
            }
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
bool SpikeDragController::updateDragWindow() {
    const auto DRAGGINGTARGET = m_target.lock();
    const bool WAS_FULLSCREEN = DRAGGINGTARGET->window() ? Fullscreen::controller()->isFullscreen(DRAGGINGTARGET->window()) : false;

    if (m_dragThresholdReached) {
        if (WAS_FULLSCREEN) {
            Log::logger->log(Log::DEBUG, "Dragging a fullscreen window");
            Fullscreen::controller()->setFullscreenMode(DRAGGINGTARGET->window(), Fullscreen::FSMODE_NONE);
        }

        const auto PWORKSPACE     = DRAGGINGTARGET->workspace();
        const auto DRAGGINGWINDOW = DRAGGINGTARGET->window();

        if (Fullscreen::controller()->hasFullscreen(PWORKSPACE) && (!DRAGGINGTARGET->floating() || !DRAGGINGWINDOW->isAllowedOverFullscreen())) {
            Log::logger->log(Log::DEBUG, "Rejecting drag on a fullscreen workspace. (window under fullscreen)");
            CKeybindManager::changeMouseBindMode(MBIND_INVALID);
            return true;
        }
    }

    m_draggingTiled                   = false;
    m_draggingWindowOriginalFloatSize = DRAGGINGTARGET->lastFloatingSize();

    if (WAS_FULLSCREEN && DRAGGINGTARGET->floating() && m_dragThresholdReached) {
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        DRAGGINGTARGET->setPositionGlobal(CBox{MOUSECOORDS - DRAGGINGTARGET->position().size() / 2.F, DRAGGINGTARGET->position().size()});
    } else if (!DRAGGINGTARGET->floating() && m_dragMode == MBIND_MOVE) {
        Vector2D MINSIZE = DRAGGINGTARGET->minSize().value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
        DRAGGINGTARGET->rememberFloatingSize((DRAGGINGTARGET->position().size() * 0.8489).clamp(MINSIZE, Vector2D{}).floor());

        if (m_dragThresholdReached) {
            DRAGGINGTARGET->setPositionGlobal(CBox{g_pInputManager->getMouseCoordsInternal() - DRAGGINGTARGET->position().size() / 2.F, DRAGGINGTARGET->position().size()});
            g_layoutManager->changeFloatingMode(DRAGGINGTARGET);
            m_draggingTiled = true;
        }
    }

    const auto DRAG_ORIGINAL_BOX = DRAGGINGTARGET->position();

    // HyprVX (Feature 002): the pointer in the window's L space; unchanged on the stock path
    m_beginDragXY         = DRAGGINGTARGET->window() ? HyprVXSpike::windowLocalPoint(*DRAGGINGTARGET->window(), g_pInputManager->getMouseCoordsInternal()) :
                                                       g_pInputManager->getMouseCoordsInternal();
    m_beginDragPositionXY = DRAG_ORIGINAL_BOX.pos();
    m_beginDragSizeXY     = DRAG_ORIGINAL_BOX.size();
    m_lastDragXY          = m_beginDragXY;

    return false;
}
void SpikeDragController::mouseMove(const Vector2D& screenPos) {
    if (m_target.expired())
        return;

    const auto  DRAGGINGTARGET = m_target.lock();
    // HyprVX (Feature 002): the pointer in the window's L space, as m_beginDragXY; unchanged on the stock path
    const auto  mousePos = DRAGGINGTARGET->window() ? HyprVXSpike::windowLocalPoint(*DRAGGINGTARGET->window(), screenPos) : screenPos;
    static auto PDRAGTHRESHOLD = CConfigValue<Config::INTEGER>("binds:drag_threshold");

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if ((!validMapped(DRAGGINGTARGET->window()) || m_beginDragSizeXY == Vector2D())) {
        CKeybindManager::changeMouseBindMode(MBIND_INVALID);
        return;
    }

    // Yoink dragged window here instead if using drag_threshold and it has been reached
    if (*PDRAGTHRESHOLD > 0 && !m_dragThresholdReached) {
        if ((m_beginDragXY.distanceSq(mousePos) <= std::pow(*PDRAGTHRESHOLD, 2) && m_beginDragXY == m_lastDragXY))
            return;
        m_dragThresholdReached = true;
        if (updateDragWindow())
            return;
    }

    static auto TIMER = std::chrono::high_resolution_clock::now(), MSTIMER = TIMER;

    const auto  DELTA     = Vector2D(mousePos.x - m_beginDragXY.x, mousePos.y - m_beginDragXY.y);
    const auto  TICKDELTA = Vector2D(mousePos.x - m_lastDragXY.x, mousePos.y - m_lastDragXY.y);

    static auto SNAPENABLED = CConfigValue<Config::INTEGER>("general:snap:enabled");

    const auto  TIMERDELTA    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - TIMER).count();
    const auto  MSDELTA       = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - MSTIMER).count();
    const auto  MSMONITOR     = 1000.0 / (g_pHyprRenderer->m_mostHzMonitor ? g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 60.0);
    static int  totalMs       = 0;
    bool        canSkipUpdate = true;

    MSTIMER = std::chrono::high_resolution_clock::now();

    if (m_mouseMoveEventCount == 1)
        totalMs = 0;

    if (MSMONITOR > 16.0) {
        totalMs += MSDELTA < MSMONITOR ? MSDELTA : std::round(totalMs * 1.0 / m_mouseMoveEventCount);
        m_mouseMoveEventCount += 1;

        // check if time-window is enough to skip update on 60hz monitor
        canSkipUpdate = std::clamp(MSMONITOR - TIMERDELTA, 0.0, MSMONITOR) > totalMs * 1.0 / m_mouseMoveEventCount;
    }

    if ((abs(TICKDELTA.x) < 1.f && abs(TICKDELTA.y) < 1.f) || (TIMERDELTA < MSMONITOR && canSkipUpdate && (m_dragMode != MBIND_MOVE)))
        return;

    TIMER = std::chrono::high_resolution_clock::now();

    m_lastDragXY = mousePos;

    DRAGGINGTARGET->damageEntire();

    const auto MOTIONWINDOW = DRAGGINGTARGET->window();
    const bool TRACKMOTION  = validMapped(MOTIONWINDOW);
    CBox       previousFull;

    if (TRACKMOTION)
        previousFull = MOTIONWINDOW->getFullWindowBoundingBox();

    if (m_dragMode == MBIND_MOVE) {

        Vector2D newPos  = m_beginDragPositionXY + DELTA;
        Vector2D newSize = DRAGGINGTARGET->position().size();

        if (*SNAPENABLED && !m_draggingTiled)
            g_layoutManager->performSnap(newPos, newSize, DRAGGINGTARGET, MBIND_MOVE, -1, m_beginDragSizeXY);

        newPos = newPos.round();

        DRAGGINGTARGET->setPositionGlobal({newPos, newSize});
        DRAGGINGTARGET->warpPositionSize();
    } else if (isResizeMode(m_dragMode)) {
        if (DRAGGINGTARGET->floating()) {

            Vector2D MINSIZE = DRAGGINGTARGET->minSize().value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
            Vector2D MAXSIZE = DRAGGINGTARGET->maxSize().value_or(Math::VECTOR2D_MAX);

            Vector2D newSize = m_beginDragSizeXY;
            Vector2D newPos  = m_beginDragPositionXY;

            if (edgeRight(m_grabbedCorner))
                newSize.x += DELTA.x;
            else if (edgeLeft(m_grabbedCorner))
                newSize.x -= DELTA.x;

            if (edgeBottom(m_grabbedCorner))
                newSize.y += DELTA.y;
            else if (edgeTop(m_grabbedCorner))
                newSize.y -= DELTA.y;

            eMouseBindMode mode = m_dragMode;
            if (DRAGGINGTARGET->window() && DRAGGINGTARGET->window()->m_ruleApplicator->keepAspectRatio().valueOrDefault() && mode != MBIND_RESIZE_BLOCK_RATIO)
                mode = MBIND_RESIZE_FORCE_RATIO;

            if (m_beginDragSizeXY.x >= 1 && m_beginDragSizeXY.y >= 1 && mode == MBIND_RESIZE_FORCE_RATIO) {

                const float RATIO = m_beginDragSizeXY.y / m_beginDragSizeXY.x;

                if (MINSIZE.x * RATIO > MINSIZE.y)
                    MINSIZE = Vector2D(MINSIZE.x, MINSIZE.x * RATIO);
                else
                    MINSIZE = Vector2D(MINSIZE.y / RATIO, MINSIZE.y);

                if (MAXSIZE.x * RATIO < MAXSIZE.y)
                    MAXSIZE = Vector2D(MAXSIZE.x, MAXSIZE.x * RATIO);
                else
                    MAXSIZE = Vector2D(MAXSIZE.y / RATIO, MAXSIZE.y);

                if (newSize.x * RATIO > newSize.y)
                    newSize = Vector2D(newSize.x, newSize.x * RATIO);
                else
                    newSize = Vector2D(newSize.y / RATIO, newSize.y);
            }

            newSize = newSize.clamp(MINSIZE, MAXSIZE);

            if (edgeLeft(m_grabbedCorner))
                newPos.x += (m_beginDragSizeXY - newSize).x;
            if (edgeTop(m_grabbedCorner))
                newPos.y += (m_beginDragSizeXY - newSize).y;

            if (*SNAPENABLED) {
                g_layoutManager->performSnap(newPos, newSize, DRAGGINGTARGET, mode, m_grabbedCorner, m_beginDragSizeXY);
                newSize = newSize.clamp(MINSIZE, MAXSIZE);
            }

            CBox wb = {newPos, newSize};
            wb.round();

            DRAGGINGTARGET->setPositionGlobal(wb);
            DRAGGINGTARGET->warpPositionSize();
        } else {
            g_layoutManager->resizeTarget(TICKDELTA, DRAGGINGTARGET, m_grabbedCorner);
            DRAGGINGTARGET->warpPositionSize();
        }
    }

    if (TRACKMOTION)
        MOTIONWINDOW->recordMotionBlur(previousFull, MOTIONWINDOW->getFullWindowBoundingBox());

    // get middle point
    Vector2D middle = DRAGGINGTARGET->position().middle();

    // and check its monitor
    const auto PMONITOR = State::monitorState()->query().vec(middle).run();

    if (PMONITOR && PMONITOR->m_activeWorkspace && DRAGGINGTARGET->floating() /* If we're resizing a tiled target, don't do this */) {
        const auto WS = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
        DRAGGINGTARGET->assignToSpace(WS->m_space);
    }

    DRAGGINGTARGET->damageEntire();
}


void HyprVXSpike::addCopiedDragControllerHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"dragBegin", "Layout::Supplementary::CDragStateController::dragBegin(Hyprutils::Memory::CSharedPointer<Layout::ITarget>, eMouseBindMode, std::optional<Layout::eRectCorner>, bool)", reinterpret_cast<void*>(&SpikeDragController::hk_dragBegin), nullptr});
    specs.push_back({"updateDragWindow", "Layout::Supplementary::CDragStateController::updateDragWindow()", reinterpret_cast<void*>(&SpikeDragController::hk_updateDragWindow), nullptr});
    specs.push_back({"mouseMove", "Layout::Supplementary::CDragStateController::mouseMove(Hyprutils::Math::Vector2D const&)", reinterpret_cast<void*>(&SpikeDragController::hk_mouseMove), nullptr});
}
