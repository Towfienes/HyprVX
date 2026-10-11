// Generated for the HyprVX delivery spike (T066) from the patched draft tree (hyprland-draft 43b8c24e = Hyprland
// efb5099 + patches 0001-0009), src/managers/input/InputManager.cpp. Bodies are verbatim apart from: the class renamed to the spike subclass,
// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().
// Prologues from gen_copies.py PROLOGUES: none.
// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with
// python3 -I platform/plugin-spike/gen_copies.py platform/plugin-spike/src/copied
#include "../SpikeAccess.hpp"

#include <src/managers/input/InputManager.hpp>
#include <src/Compositor.hpp>
#include <aquamarine/output/Output.hpp>
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include <ranges>
#include <algorithm>
#include <src/config/ConfigValue.hpp>
#include <src/config/shared/actions/ConfigActions.hpp>
#include <src/config/legacy/ConfigManager.hpp>

#include <src/desktop/view/WLSurface.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/protocols/CursorShape.hpp>
#include <src/protocols/IdleInhibit.hpp>
#include <src/protocols/RelativePointer.hpp>
#include <src/protocols/PointerConstraints.hpp>
#include <src/protocols/PointerGestures.hpp>
#include <src/protocols/IdleNotify.hpp>
#include <src/protocols/SessionLock.hpp>
#include <src/protocols/InputMethodV2.hpp>
#include <src/protocols/VirtualKeyboard.hpp>
#include <src/protocols/VirtualPointer.hpp>
#include <src/protocols/LayerShell.hpp>
#include <src/protocols/core/Seat.hpp>
#include <src/protocols/core/DataDevice.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <src/protocols/XDGShell.hpp>
#include <src/protocols/InputCapture.hpp>

#include <src/devices/Mouse.hpp>
#include <src/devices/VirtualPointer.hpp>
#include <src/devices/Keyboard.hpp>
#include <src/devices/VirtualKeyboard.hpp>
#include <src/devices/TouchDevice.hpp>

#include <src/pointer/PointerManager.hpp>
#include <src/pointer/PointerController.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/managers/KeybindManager.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>

#include <src/managers/EventManager.hpp>
#include <src/managers/permissions/DynamicPermissionManager.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/MonitorLayoutController.hpp>

#include <src/helpers/time/Time.hpp>
#include <src/helpers/MiscFunctions.hpp>

#include <src/layout/LayoutManager.hpp>

#include <src/event/EventBus.hpp>

#include <src/render/Renderer.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/pointer/cursor/CursorShapeOverrideController.hpp>

#include <aquamarine/input/Input.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/VarList2.hpp>


#include "../Camera.hpp"
#include "../Hooks.hpp"

using namespace Hyprutils::String;

struct SpikeInputManager : public CInputManager {
        void mouseMoveUnified(uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos);
        void processMouseDownNormal(const IPointer::SButtonEvent& e, SP<IPointer> mouse);
        void onMouseWheel(IPointer::SAxisEvent e, SP<IPointer> pointer);
        void sendMotionEventsToFocused();

    static void hk_mouseMoveUnified(CInputManager* self, uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {
        static_cast<SpikeInputManager*>(self)->mouseMoveUnified(time, refocus, mouse, overridePos);
    }
    static void hk_processMouseDownNormal(CInputManager* self, const IPointer::SButtonEvent& e, SP<IPointer> mouse) {
        static_cast<SpikeInputManager*>(self)->processMouseDownNormal(e, mouse);
    }
    static void hk_onMouseWheel(CInputManager* self, IPointer::SAxisEvent e, SP<IPointer> pointer) {
        static_cast<SpikeInputManager*>(self)->onMouseWheel(e, pointer);
    }
    static void hk_sendMotionEventsToFocused(CInputManager* self) {
        static_cast<SpikeInputManager*>(self)->sendMotionEventsToFocused();
    }
};

void SpikeInputManager::mouseMoveUnified(uint32_t time, bool refocus, bool mouse, std::optional<Vector2D> overridePos) {
    m_lastInputMouse = mouse;

    if (g_pCompositor->m_isShuttingDown)
        return;

    Vector2D const mouseCoords        = overridePos.value_or(getMouseCoordsInternal());
    auto const     MOUSECOORDSFLOORED = mouseCoords.floor();

    if (MOUSECOORDSFLOORED == m_lastCursorPosFloored && !refocus)
        return;

    static auto PFOLLOWMOUSE          = CConfigValue<Config::INTEGER>("input:follow_mouse");
    static auto PFOLLOWMOUSETHRESHOLD = CConfigValue<Config::FLOAT>("input:follow_mouse_threshold");
    static auto PMOUSEREFOCUS         = CConfigValue<Config::INTEGER>("input:mouse_refocus");
    static auto PFOLLOWONDND          = CConfigValue<Config::INTEGER>("misc:always_follow_on_dnd");
    static auto PFLOATBEHAVIOR        = CConfigValue<Config::INTEGER>("input:float_switch_override_focus");
    static auto PMOUSEFOCUSMON        = CConfigValue<Config::INTEGER>("misc:mouse_move_focuses_monitor");
    static auto PRESIZEONBORDER       = CConfigValue<Config::INTEGER>("general:resize_on_border");
    static auto PRESIZECURSORICON     = CConfigValue<Config::INTEGER>("general:hover_icon_on_border");

    const auto  FOLLOWMOUSE = *PFOLLOWONDND && PROTO::data->dndActive() ? 1 : *PFOLLOWMOUSE;

    if (FOLLOWMOUSE == 1 && m_lastCursorMovement.getSeconds() < 0.5)
        m_mousePosDelta += MOUSECOORDSFLOORED.distance(m_lastCursorPosFloored);
    else
        m_mousePosDelta = 0;

    m_foundSurfaceToFocus.reset();
    m_foundLSToFocus.reset();
    m_foundWindowToFocus.reset();
    SP<CWLSurfaceResource> foundSurface;
    Vector2D               surfaceCoords;
    Vector2D               surfacePos = Vector2D(-1337, -1337);
    PHLWINDOW              pFoundWindow;
    PHLLS                  pFoundLayerSurface;
    const auto             FOCUS_REASON = refocus ? Desktop::FOCUS_REASON_CLICK : Desktop::FOCUS_REASON_FFM;

    Event::SCallbackInfo   info;
    Event::bus()->m_events.input.mouse.move.emit(MOUSECOORDSFLOORED, info);
    if (info.cancelled)
        return;

    m_lastCursorPosFloored = MOUSECOORDSFLOORED;

    // use mouseCoords specifically in case touch sent overridePos, otherwise touch doesn't work on non-focused monitor
    const auto PMONITOR = isLocked() && Desktop::focusState()->monitor() ? Desktop::focusState()->monitor() : State::monitorState()->query().vec(mouseCoords).run();

    // this can happen if there are no displays hooked up to Hyprland
    if (PMONITOR == nullptr)
        return;

    // HyprVX: the pointer in window-content (world) coordinates; equal to mouseCoords when no camera applies.
    const auto WINDOWCOORDS = HyprVXSpike::screenToWorld(*PMONITOR, mouseCoords);

    if (PMONITOR->m_cursorZoom->value() != 1.f)
        g_pHyprRenderer->damageMonitor(PMONITOR);

    bool       skipFrameSchedule = PMONITOR->shouldSkipScheduleFrameOnMouseEvent();

    const auto solitary = PMONITOR->m_solitaryClient.lock();
    const auto self     = PMONITOR->m_self.lock();

    if (!solitary && g_pHyprRenderer->shouldRenderCursor() && Pointer::mgr()->softwareLockedFor(self) && !skipFrameSchedule)
        PMONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);

    // constraints
    auto confineToRegion = [&](const CRegion& rg, SP<Desktop::View::CWLSurface> surf) {
        if (!surf)
            return;

        // HyprVX: a window-content constraint region is in world coordinates; the cursor goes to its screen image.
        const bool WORLD        = HyprVXSpike::isWindowContent(surf->resource());
        const auto CLOSEST      = rg.closestPoint(WORLD ? WINDOWCOORDS : mouseCoords);
        const auto BOX          = surf->getSurfaceBoxGlobal();
        const auto WINDOW       = Desktop::View::CWindow::fromView(surf->view());
        const auto CLOSESTLOCAL = (CLOSEST - (BOX.has_value() ? BOX->pos() : Vector2D{})) * (WINDOW ? WINDOW->m_X11SurfaceScaledBy : 1.0);

        if (g_pSeatManager->m_state.pointerFocus != surf->resource())
            g_pSeatManager->setPointerFocus(surf->resource(), CLOSESTLOCAL);

        Pointer::pointerController()->warpTo(WORLD ? HyprVXSpike::worldToScreen(*PMONITOR, CLOSEST) : CLOSEST, true);
        g_pSeatManager->sendPointerMotion(time, CLOSESTLOCAL);
        PROTO::relativePointer->sendRelativeMotion(sc<uint64_t>(time) * 1000, {}, {});
    };

    if (!g_pSeatManager->m_mouse.expired()) {
        const auto SURF = Desktop::View::CWLSurface::fromResource(Desktop::focusState()->surface());

        if (isConstrained()) {
            const auto CONSTRAINT = SURF ? SURF->constraint() : nullptr;

            if (CONSTRAINT) {
                if (CONSTRAINT->isLocked()) {
                    const auto HINT = CONSTRAINT->logicPositionHint();
                    Pointer::pointerController()->warpTo(HyprVXSpike::isWindowContent(SURF->resource()) ? HyprVXSpike::worldToScreen(*PMONITOR, HINT) : HINT, true);
                } else {
                    confineToRegion(CONSTRAINT->logicConstraintRegion(), SURF);
                }

                return;
            } else {
                Log::logger->log(Log::ERR, "BUG THIS: Null SURF/CONSTRAINT in mouse refocus. Ignoring constraints. {:x} {:x}", rc<uintptr_t>(SURF.get()),
                                 rc<uintptr_t>(CONSTRAINT.get()));
            }
        } else {
            const auto WINDOW = SURF ? Desktop::View::CWindow::fromView(SURF->view()) : nullptr;
            if (WINDOW) {
                if (WINDOW->m_ruleApplicator->confinePointer().valueOrDefault()) {
                    const auto BOX = SURF->getSurfaceBoxGlobal();
                    if (BOX.has_value()) {
                        CRegion rg;
                        rg.set(*BOX);
                        confineToRegion(rg, SURF);
                    }
                    return;
                }
            }
        }
    }

    if (PMONITOR != Desktop::focusState()->monitor() && (*PMOUSEFOCUSMON || refocus) && m_forcedFocus.expired())
        Desktop::focusState()->rawMonitorFocus(PMONITOR);

    // IME popups essentially always exist on the top - they are transient,
    // and pretty much always need to be visible and accessible.
    if (!foundSurface) {
        auto popup = g_pInputManager->m_relay.popupFromCoords(mouseCoords);
        if (popup) {
            foundSurface = popup->getSurface();
            surfacePos   = popup->globalBox().pos();
        }
    }

    // check for windows that have focus priority like our permission popups
    if (!foundSurface) {
        pFoundWindow = Desktop::viewState()->hitTest().windowAt(mouseCoords, Desktop::View::FOCUS_PRIORITY);
        if (pFoundWindow)
            foundSurface = Desktop::viewState()->hitTest().windowSurfaceAt(mouseCoords, pFoundWindow, surfaceCoords);
    }

    if (!foundSurface && g_pSessionLockManager->isSessionLocked()) {

        // set keyboard focus on session lock surface regardless of layers
        const auto PSESSIONLOCKSURFACE = g_pSessionLockManager->getSessionLockSurfaceForMonitor(PMONITOR->m_id);
        const auto foundLockSurface    = PSESSIONLOCKSURFACE ? PSESSIONLOCKSURFACE->surface->surface() : nullptr;

        Desktop::focusState()->rawSurfaceFocus(foundLockSurface);

        // search for interactable abovelock surfaces for pointer focus, or use session lock surface if not found
        for (auto& lsl : PMONITOR->m_layerSurfaceLayers | std::views::reverse) {
            foundSurface = Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &lsl, &surfaceCoords, &pFoundLayerSurface, true);

            if (foundSurface)
                break;
        }

        if (!foundSurface) {
            surfaceCoords = mouseCoords - PMONITOR->m_position;
            foundSurface  = foundLockSurface;
        }

        if (refocus) {
            m_foundLSToFocus      = pFoundLayerSurface;
            m_foundWindowToFocus  = pFoundWindow;
            m_foundSurfaceToFocus = foundSurface;
        }

        g_pSeatManager->setPointerFocus(foundSurface, surfaceCoords);
        g_pSeatManager->sendPointerMotion(time, surfaceCoords);

        return;
    }

    PHLWINDOW forcedFocus = m_forcedFocus.lock();

    if (!forcedFocus)
        forcedFocus = Desktop::viewState()->query().forceFocus().runWindow();

    if (forcedFocus && !foundSurface) {
        pFoundWindow = forcedFocus;
        surfacePos   = pFoundWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
        foundSurface = pFoundWindow->wlSurface()->resource();
    }

    // if we are holding a pointer button,
    // and we're not dnd-ing, don't refocus. Keep focus on last surface.
    if (!overridePos.has_value() && !PROTO::data->dndActive() && !m_currentlyHeldButtons.empty() && Desktop::focusState()->surface() &&
        Desktop::focusState()->surface()->m_mapped && g_pSeatManager->m_state.pointerFocus && !m_hardInput) {
        foundSurface = g_pSeatManager->m_state.pointerFocus.lock();

        // IME popups aren't desktop-like elements
        // TODO: make them.
        CInputPopup* foundPopup = m_relay.popupFromSurface(foundSurface);
        if (foundPopup) {
            surfacePos             = foundPopup->globalBox().pos();
            m_focusHeldByButtons   = true;
            m_refocusHeldByButtons = refocus;
        } else {
            auto HLSurface = Desktop::View::CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX) {
                    const auto PWINDOW = Desktop::View::CWindow::fromView(HLSurface->view());
                    const auto LS      = Desktop::View::CLayerSurface::fromView(HLSurface->view());
                    surfacePos         = BOX->pos();

                    if (PWINDOW && PWINDOW->acceptsInput())
                        pFoundWindow = PWINDOW;
                    else if (LS)
                        pFoundLayerSurface = LS;

                } else // reset foundSurface, find one normally
                    foundSurface = nullptr;
            } else // reset foundSurface, find one normally
                foundSurface = nullptr;
        }
    }

    g_layoutManager->moveMouse(getMouseCoordsInternal());

    if (g_layoutManager->dragController()->exclusiveDeviceGrab()) {
        g_pSeatManager->setPointerFocus(nullptr, {});
        return;
    }

    // forced above all
    if (!g_pInputManager->m_exclusiveLSes.empty()) {
        if (!foundSurface)
            foundSurface = Desktop::viewState()->hitTest().layerPopupSurfaceAt(mouseCoords, &g_pInputManager->m_exclusiveLSes, &surfaceCoords, &pFoundLayerSurface);

        if (!foundSurface)
            foundSurface = Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &g_pInputManager->m_exclusiveLSes, &surfaceCoords, &pFoundLayerSurface);

        if (!foundSurface) {
            foundSurface = (*g_pInputManager->m_exclusiveLSes.begin())->wlSurface()->resource();
            surfacePos   = (*g_pInputManager->m_exclusiveLSes.begin())->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        }
    }

    if (!foundSurface)
        foundSurface = Desktop::viewState()->hitTest().layerPopupSurfaceAt(mouseCoords, PMONITOR, &surfaceCoords, &pFoundLayerSurface);

    // overlays are above fullscreen
    if (!foundSurface)
        foundSurface =
            Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords, &pFoundLayerSurface);

    // also top layers
    if (!foundSurface)
        foundSurface =
            Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &pFoundLayerSurface);

    // then, we check if the workspace doesn't have a fullscreen window
    const auto PWORKSPACE = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;

    bool       windowIdealQueried = false;
    PHLWINDOW  pWindowIdeal;
    const auto getWindowIdeal = [&]() -> const PHLWINDOW& {
        if (!windowIdealQueried) {
            pWindowIdeal = Desktop::viewState()->hitTest().windowAt(
                mouseCoords, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING | Desktop::View::FOLLOW_MOUSE_CHECK);
            windowIdealQueried = true;
        }

        return pWindowIdeal;
    };

    const bool HAS_EXCLUSIVE_FULLSCREEN =
        (Fullscreen::controller()->hasFullscreen(PWORKSPACE) && Fullscreen::controller()->getFullscreenModes(PWORKSPACE).internal == Fullscreen::FSMODE_FULLSCREEN) ||
        (Fullscreen::controller()->hasFullscreen(PMONITOR) && Fullscreen::controller()->getFullscreenModes(PMONITOR).internal == Fullscreen::FSMODE_FULLSCREEN);

    if (HAS_EXCLUSIVE_FULLSCREEN) {
        const auto IS_LS_UNFOCUSABLE = pFoundLayerSurface &&
            (pFoundLayerSurface->m_layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP ||
             (pFoundLayerSurface->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP && !pFoundLayerSurface->m_aboveFullscreen));

        if (IS_LS_UNFOCUSABLE) {
            foundSurface       = nullptr;
            pFoundLayerSurface = nullptr;

            pFoundWindow = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);

            if (!pFoundWindow) {
                return;
            }

            const auto& PWINDOWIDEAL = getWindowIdeal();
            if (PWINDOWIDEAL &&
                ((PWINDOWIDEAL->m_isFloating && PWINDOWIDEAL->isAllowedOverFullscreen()) /* floating over fullscreen or pinned */
                 || (PMONITOR->m_activeSpecialWorkspace == PWINDOWIDEAL->m_workspace) /* on an open special workspace */))
                pFoundWindow = PWINDOWIDEAL;

            if (!pFoundWindow->m_isX11) {
                foundSurface = Desktop::viewState()->hitTest().windowSurfaceAt(mouseCoords, pFoundWindow, surfaceCoords);
                surfacePos   = Vector2D(-1337, -1337);
            } else {
                foundSurface = pFoundWindow->wlSurface()->resource();
                surfacePos   = pFoundWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
            }
        }
    }

    // then windows
    if (!foundSurface) {
        if (Fullscreen::controller()->hasFullscreen(PWORKSPACE) && Fullscreen::controller()->getFullscreenModes(PWORKSPACE).internal == Fullscreen::FSMODE_MAXIMIZED) {
            if (!foundSurface) {
                if (PMONITOR->m_activeSpecialWorkspace) {
                    const auto& PWINDOWIDEAL = getWindowIdeal();
                    if (pFoundWindow != PWINDOWIDEAL)
                        pFoundWindow = PWINDOWIDEAL;

                    if (pFoundWindow && !pFoundWindow->onSpecialWorkspace()) {
                        pFoundWindow = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);
                    }
                } else {
                    // if we have a maximized window, allow focusing on a bar or something if in reserved area.
                    if (State::monitorLayoutController()->isPointOnReservedArea(mouseCoords, PMONITOR)) {
                        foundSurface = Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
                                                                                      &surfaceCoords, &pFoundLayerSurface);
                    }

                    if (!foundSurface) {
                        const auto& PWINDOWIDEAL = getWindowIdeal();
                        if (pFoundWindow != PWINDOWIDEAL)
                            pFoundWindow = PWINDOWIDEAL;

                        if (!(pFoundWindow && (pFoundWindow->m_isFloating && pFoundWindow->isAllowedOverFullscreen())))
                            pFoundWindow = Fullscreen::controller()->getFullscreenWindow(PWORKSPACE);
                    }
                }
            }

        } else {
            const auto& PWINDOWIDEAL = getWindowIdeal();
            if (pFoundWindow != PWINDOWIDEAL)
                pFoundWindow = PWINDOWIDEAL;
        }

        if (pFoundWindow) {
            if (!pFoundWindow->m_isX11) {
                foundSurface = Desktop::viewState()->hitTest().windowSurfaceAt(mouseCoords, pFoundWindow, surfaceCoords);
                if (!foundSurface) {
                    foundSurface = pFoundWindow->wlSurface()->resource();
                    surfacePos   = pFoundWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
                }
            } else {
                foundSurface = pFoundWindow->wlSurface()->resource();
                surfacePos   = pFoundWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
            }
        }
    }

    // then surfaces below
    if (!foundSurface)
        foundSurface =
            Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], &surfaceCoords, &pFoundLayerSurface);

    if (!foundSurface)
        foundSurface =
            Desktop::viewState()->hitTest().layerSurfaceAt(mouseCoords, &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], &surfaceCoords, &pFoundLayerSurface);

    if (Pointer::mgr()->softwareLockedFor(self) > 0 && !skipFrameSchedule) {
        if (const auto PMONITOR = Desktop::focusState()->monitor())
            PMONITOR->scheduleFrame(Aquamarine::IOutput::AQ_SCHEDULE_CURSOR_MOVE);
    }

    // FIXME: This will be disabled during DnD operations because we do not exactly follow the spec
    // xdg-popup grabs should be keyboard-only, while they are absolute in our case...
    if (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(foundSurface) && !PROTO::data->dndActive()) {
        if (m_hardInput || refocus) {
            g_pSeatManager->setGrab(nullptr);
            return; // setGrab will refocus
        } else {
            // we need to grab the last surface.
            foundSurface = g_pSeatManager->m_state.pointerFocus.lock();

            auto HLSurface = Desktop::View::CWLSurface::fromResource(foundSurface);

            if (HLSurface) {
                const auto BOX = HLSurface->getSurfaceBoxGlobal();

                if (BOX.has_value())
                    surfacePos = BOX->pos();
            }
        }
    }

    if (!foundSurface) {
        if (!m_emptyFocusCursorSet) {
            g_pHyprRenderer->setCursorFromName("left_ptr");
            m_emptyFocusCursorSet = true;
        }

        g_pSeatManager->setPointerFocus(nullptr, {});

        if (refocus || !Desktop::focusState()->window()) // if we are forcing a refocus, and we don't find a surface, clear the kb focus too!
            Desktop::focusState()->rawWindowFocus(nullptr, FOCUS_REASON);

        return;
    }

    m_emptyFocusCursorSet = false;

    // HyprVX: surfacePos of window content is a world position, so the pointer must be in world coordinates too.
    Vector2D surfaceLocal = surfacePos == Vector2D(-1337, -1337) ? surfaceCoords : (HyprVXSpike::isWindowContent(foundSurface) ? WINDOWCOORDS : mouseCoords) - surfacePos;

    if (pFoundWindow && pFoundWindow->m_isX11) // for x11 force scale zero
        surfaceLocal = surfaceLocal * pFoundWindow->m_X11SurfaceScaledBy;

    bool allowKeyboardRefocus = true;

    if (!refocus && Desktop::focusState()->surface()) {
        const auto PLS = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_LAYER_SURFACE).surface(Desktop::focusState()->surface()).runLayer();

        if (PLS && PLS->m_layerSurface->m_current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
            allowKeyboardRefocus = false;
    }

    // set the values for use
    if (refocus) {
        m_foundLSToFocus      = pFoundLayerSurface;
        m_foundWindowToFocus  = pFoundWindow;
        m_foundSurfaceToFocus = foundSurface;
    }

    if (g_layoutManager->dragController()->target() && (!pFoundWindow || pFoundWindow->layoutTarget() != g_layoutManager->dragController()->target())) {
        g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
        return;
    }

    if (pFoundWindow && foundSurface == pFoundWindow->wlSurface()->resource() && !m_cursorImageOverridden) {
        const auto BOX = pFoundWindow->getWindowMainSurfaceBox();
        if (VECNOTINRECT(WINDOWCOORDS, BOX.x, BOX.y, BOX.x + BOX.width, BOX.y + BOX.height))
            g_pHyprRenderer->setCursorFromName("left_ptr");
        else
            restoreCursorIconToApp();
    }

    if (pFoundWindow) {
        // change cursor icon if hovering over border
        // HyprVX: no resize icon while a camera applies (border resize is blocked, FR-012).
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && !HyprVXSpike::activeCamera(*PMONITOR)) {
            if (!Fullscreen::controller()->isFullscreen(pFoundWindow) && !pFoundWindow->hasPopupAt(WINDOWCOORDS))
                setCursorIconOnBorder(pFoundWindow);
            else if (m_borderIconDirection != BORDERICON_NONE) {
                m_borderIconDirection = BORDERICON_NONE;
                Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_WINDOW_EDGE);
            }
        } else if (m_borderIconDirection != BORDERICON_NONE) {
            m_borderIconDirection = BORDERICON_NONE;
            Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_WINDOW_EDGE);
        }

        if (FOLLOWMOUSE != 1 && !refocus) {
            if (pFoundWindow != Desktop::focusState()->window() && Desktop::focusState()->window() &&
                ((pFoundWindow->m_isFloating && *PFLOATBEHAVIOR == 2) || (Desktop::focusState()->window()->m_isFloating != pFoundWindow->m_isFloating && *PFLOATBEHAVIOR != 0))) {
                // enter if change floating style
                if (FOLLOWMOUSE != 3 && allowKeyboardRefocus)
                    Desktop::focusState()->rawWindowFocus(pFoundWindow, FOCUS_REASON, foundSurface);
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
            } else if (FOLLOWMOUSE == 2 || FOLLOWMOUSE == 3)
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (pFoundWindow == Desktop::focusState()->window())
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (FOLLOWMOUSE != 0 || pFoundWindow == Desktop::focusState()->window())
                g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);

            if (g_pSeatManager->m_state.pointerFocus == foundSurface)
                g_pSeatManager->sendPointerMotion(time, surfaceLocal);

            m_lastFocusOnLS = false;
            return; // don't enter any new surfaces
        } else {
            auto lastFocus = m_lastMouseFocus.lock();

            if (allowKeyboardRefocus && ((FOLLOWMOUSE != 3 && (*PMOUSEREFOCUS || lastFocus != pFoundWindow)) || refocus)) {
                if (lastFocus != pFoundWindow || Desktop::focusState()->window() != pFoundWindow || Desktop::focusState()->surface() != foundSurface || refocus) {

                    m_lastMouseFocus = pFoundWindow;

                    // TODO: this looks wrong. When over a popup, it constantly is switching.
                    // Temp fix until that's figured out. Otherwise spams windowrule lookups and other shit.
                    if (m_lastMouseFocus.lock() != pFoundWindow || Desktop::focusState()->window() != pFoundWindow) {
                        if (m_mousePosDelta > *PFOLLOWMOUSETHRESHOLD || refocus) {
                            const bool hasNoFollowMouse = pFoundWindow && pFoundWindow->m_ruleApplicator->noFollowMouse().valueOrDefault();

                            if (refocus || !hasNoFollowMouse)
                                Desktop::focusState()->rawWindowFocus(pFoundWindow, FOCUS_REASON, foundSurface);
                        }
                    } else
                        Desktop::focusState()->rawSurfaceFocus(foundSurface, pFoundWindow);
                }
            }
        }

        if (g_pSeatManager->m_state.keyboardFocus == nullptr)
            Desktop::focusState()->rawWindowFocus(pFoundWindow, FOCUS_REASON, foundSurface);

        m_lastFocusOnLS = false;
    } else {
        if (*PRESIZEONBORDER && *PRESIZECURSORICON && m_borderIconDirection != BORDERICON_NONE) {
            m_borderIconDirection = BORDERICON_NONE;
            Pointer::Cursor::overrideController->unsetOverride(Pointer::Cursor::CURSOR_OVERRIDE_WINDOW_EDGE);
        }

        if (pFoundLayerSurface && (pFoundLayerSurface->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) && FOLLOWMOUSE != 3 &&
            (allowKeyboardRefocus || pFoundLayerSurface->m_layerSurface->m_current.interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)) {
            Desktop::focusState()->rawSurfaceFocus(foundSurface);
        }

        if (pFoundLayerSurface)
            m_lastFocusOnLS = true;
    }

    if (!overridePos.has_value()) {
        g_pSeatManager->setPointerFocus(foundSurface, surfaceLocal);
        g_pSeatManager->sendPointerMotion(time, surfaceLocal);
    }
}
void SpikeInputManager::processMouseDownNormal(const IPointer::SButtonEvent& e, SP<IPointer> mouse) {

    // notify the keybind manager
    static auto PPASSMOUSE        = CConfigValue<Config::INTEGER>("binds:pass_mouse_when_bound");
    const auto  PASS              = g_pKeybindManager->onMouseEvent(e, mouse);
    static auto PFOLLOWMOUSE      = CConfigValue<Config::INTEGER>("input:follow_mouse");
    static auto PRESIZEONBORDER   = CConfigValue<Config::INTEGER>("general:resize_on_border");
    static auto PBORDERSIZE       = CConfigValue<Config::INTEGER>("general:border_size");
    static auto PBORDERGRABEXTEND = CConfigValue<Config::INTEGER>("general:extend_border_grab_area");
    const auto  BORDER_GRAB_AREA  = *PRESIZEONBORDER ? *PBORDERSIZE + *PBORDERGRABEXTEND : 0;

    if (!PASS && !*PPASSMOUSE)
        return;

    const auto mouseCoords = g_pInputManager->getMouseCoordsInternal();
    const auto w           = Desktop::viewState()->hitTest().windowAt(mouseCoords, Desktop::View::ALLOW_FLOATING | Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);
    // HyprVX: window geometry is in world coordinates (border selection by the inverse transform, FR-012).
    const auto worldCoords = HyprVXSpike::screenToWorld(mouseCoords);

    if (w && !m_lastFocusOnLS && !g_pSessionLockManager->isSessionLocked() && w->checkInputOnDecos(INPUT_TYPE_BUTTON, mouseCoords, e))
        return;

    // clicking on border triggers resize
    // TODO detect click on LS properly
    if (*PRESIZEONBORDER && !g_pSessionLockManager->isSessionLocked() && !m_lastFocusOnLS && e.state == WL_POINTER_BUTTON_STATE_PRESSED && (!w || !w->isX11OverrideRedirect())) {
        if (w && !Fullscreen::controller()->isFullscreen(w)) {
            const CBox real = w->geometricBox(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
            const CBox grab = {real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA, real.width + 2 * BORDER_GRAB_AREA, real.height + 2 * BORDER_GRAB_AREA};

            if ((grab.containsPoint(worldCoords) && (!real.containsPoint(worldCoords) || w->isInCurvedCorner(worldCoords.x, worldCoords.y))) && !w->hasPopupAt(worldCoords)) {
                g_pKeybindManager->resizeWithBorder(e);
                return;
            }
        }
    }

    switch (e.state) {
        case WL_POINTER_BUTTON_STATE_PRESSED: {
            if (*PFOLLOWMOUSE == 3) // don't refocus on full loose
                break;

            if ((g_pSeatManager->m_mouse.expired() || !isConstrained()) /* No constraints */
                && (w && Desktop::focusState()->window() != w) /* window should change */) {
                // a bit hacky
                // if we only pressed one button, allow us to refocus. m_lCurrentlyHeldButtons.size() > 0 will stick the focus
                if (m_currentlyHeldButtons.size() == 1) {
                    const auto COPY = m_currentlyHeldButtons;
                    m_currentlyHeldButtons.clear();
                    refocus();
                    m_currentlyHeldButtons = COPY;
                } else
                    refocus();
            }

            // if clicked on a floating window make it top
            if (!g_pSeatManager->m_state.pointerFocus)
                break;

            auto HLSurf = Desktop::View::CWLSurface::fromResource(g_pSeatManager->m_state.pointerFocus.lock());

            // pointerFocus can target a surface without a Desktop::View (e.g. IME popups), so view() may be null.
            const auto PVIEW = HLSurf ? HLSurf->view() : nullptr;
            if (PVIEW && PVIEW->type() == Desktop::View::VIEW_TYPE_WINDOW)
                Desktop::windowState()->raise(dynamicPointerCast<Desktop::View::CWindow>(PVIEW));

            break;
        }
        case WL_POINTER_BUTTON_STATE_RELEASED: break;
    }

    // notify app if we didn't handle it
    g_pSeatManager->sendPointerButton(e.timeMs, e.button, e.state);

    if (const auto PMON = State::monitorState()->query().vec(mouseCoords).run(); PMON != Desktop::focusState()->monitor() && PMON)
        Desktop::focusState()->rawMonitorFocus(PMON);

    if (g_pSeatManager->m_seatGrab && e.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        m_hardInput = true;
        simulateMouseMovement();
        m_hardInput = false;
    }
}
void SpikeInputManager::onMouseWheel(IPointer::SAxisEvent e, SP<IPointer> pointer) {
    static auto POFFWINDOWAXIS        = CConfigValue<Config::INTEGER>("input:off_window_axis_events");
    static auto PINPUTSCROLLFACTOR    = CConfigValue<Config::FLOAT>("input:scroll_factor");
    static auto PTOUCHPADSCROLLFACTOR = CConfigValue<Config::FLOAT>("input:touchpad:scroll_factor");
    static auto PEMULATEDISCRETE      = CConfigValue<Config::INTEGER>("input:emulate_discrete_scroll");
    static auto PFOLLOWMOUSE          = CConfigValue<Config::INTEGER>("input:follow_mouse");

    // some virtual pointers send smooth-only wheel events: synthesize the missing discrete value (15 smooth units per 120-unit detent)
    if (e.source == WL_POINTER_AXIS_SOURCE_WHEEL && e.deltaDiscrete == 0 && e.delta != 0)
        e.deltaDiscrete = std::round(e.delta * 8.0);

    const bool ISTOUCHPADSCROLL = *PTOUCHPADSCROLLFACTOR <= 0.f || e.source == WL_POINTER_AXIS_SOURCE_FINGER;
    auto       factor           = ISTOUCHPADSCROLL ? *PTOUCHPADSCROLLFACTOR : *PINPUTSCROLLFACTOR;

    if (pointer && pointer->m_scrollFactor.has_value())
        factor = *pointer->m_scrollFactor;

    Event::SCallbackInfo info;
    Event::bus()->m_events.input.mouse.axis.emit(e, info);
    if (info.cancelled)
        return;

    if (e.mouse)
        recheckMouseWarpOnMouseInput();

    PROTO::inputCapture->axis(e.axis, e.delta);
    if (e.source == 0)
        PROTO::inputCapture->axisValue120(e.axis, e.delta);
    else if (e.delta == 0)
        PROTO::inputCapture->axisStop(e.axis);
    PROTO::inputCapture->frame();

    bool passEvent = !PROTO::inputCapture->isCaptured() && g_pKeybindManager->onAxisEvent(e, pointer);

    if (!passEvent)
        return;

    if (!m_lastFocusOnLS) {
        const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
        const auto PWINDOW = Desktop::viewState()->hitTest().windowAt(MOUSECOORDS, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

        if (PWINDOW) {
            if (PWINDOW->checkInputOnDecos(INPUT_TYPE_AXIS, MOUSECOORDS, e))
                return;

            if (*POFFWINDOWAXIS != 1) {
                // HyprVX: the window box is in world coordinates; a warp goes to its screen image.
                const auto BOX         = PWINDOW->getWindowMainSurfaceBox();
                const auto WORLDCOORDS = HyprVXSpike::screenToWorld(MOUSECOORDS);

                if (!BOX.containsPoint(WORLDCOORDS) && !PWINDOW->hasPopupAt(WORLDCOORDS)) {
                    if (*POFFWINDOWAXIS == 0)
                        return;

                    const auto TEMPCURX = std::clamp(WORLDCOORDS.x, BOX.x, BOX.x + BOX.w - 1);
                    const auto TEMPCURY = std::clamp(WORLDCOORDS.y, BOX.y, BOX.y + BOX.h - 1);

                    if (*POFFWINDOWAXIS == 3) {
                        const auto PMONITOR = State::monitorState()->query().vec(MOUSECOORDS).run();
                        Pointer::pointerController()->warpTo(PMONITOR ? HyprVXSpike::worldToScreen(*PMONITOR, {TEMPCURX, TEMPCURY}) : Vector2D{TEMPCURX, TEMPCURY}, true);
                    }

                    g_pSeatManager->sendPointerMotion(e.timeMs, Vector2D{TEMPCURX, TEMPCURY} - BOX.pos());
                    g_pSeatManager->sendPointerFrame();
                }
            }

            if (g_pSeatManager->m_state.pointerFocus) {
                const auto PCURRWINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(g_pSeatManager->m_state.pointerFocus.lock()).runWindow();

                if (*PFOLLOWMOUSE == 1 && PCURRWINDOW && PWINDOW != PCURRWINDOW)
                    simulateMouseMovement();
            }

            if (!ISTOUCHPADSCROLL && PWINDOW->isScrollMouseOverridden())
                factor = PWINDOW->getScrollMouse();
            else if (ISTOUCHPADSCROLL && PWINDOW->isScrollTouchpadOverridden())
                factor = PWINDOW->getScrollTouchpad();
        }
    }

    double discrete = (e.deltaDiscrete != 0) ? (factor * e.deltaDiscrete / std::abs(e.deltaDiscrete)) : 0;
    double delta    = e.delta * factor;

    if (e.source == 0) {
        // if an application supports v120, it should ignore discrete anyways
        if ((*PEMULATEDISCRETE >= 1 && std::abs(e.deltaDiscrete) != 120) || *PEMULATEDISCRETE >= 2) {

            const int interval = factor != 0 ? std::round(120 * (1 / factor)) : 120;

            // reset the accumulator when timeout is reached or direction/axis has changed
            if (std::signbit(e.deltaDiscrete) != m_scrollWheelState.lastEventSign || e.axis != m_scrollWheelState.lastEventAxis ||
                e.timeMs - m_scrollWheelState.lastEventTime > 500 /* 500ms taken from libinput default timeout */) {

                m_scrollWheelState.accumulatedScroll = 0;
                // send 1 discrete on first event for responsiveness
                discrete = std::copysign(1, e.deltaDiscrete);
            } else
                discrete = 0;

            for (int ac = m_scrollWheelState.accumulatedScroll; ac >= interval; ac -= interval) {
                discrete += std::copysign(1, e.deltaDiscrete);
                m_scrollWheelState.accumulatedScroll -= interval;
            }

            m_scrollWheelState.lastEventSign = std::signbit(e.deltaDiscrete);
            m_scrollWheelState.lastEventAxis = e.axis;
            m_scrollWheelState.lastEventTime = e.timeMs;
            m_scrollWheelState.accumulatedScroll += std::abs(e.deltaDiscrete);

            delta = 15.0 * discrete * factor;
        }
    }

    int32_t value120      = std::round(factor * e.deltaDiscrete);
    int32_t deltaDiscrete = std::abs(discrete) != 0 && std::abs(discrete) < 1 ? std::copysign(1, discrete) : std::round(discrete);

    g_pSeatManager->sendPointerAxis(e.timeMs, e.axis, delta, deltaDiscrete, value120, e.source, WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);

    const bool deferPointerFrame = e.source == WL_POINTER_AXIS_SOURCE_FINGER || e.source == WL_POINTER_AXIS_SOURCE_CONTINUOUS;
    if (deferPointerFrame) {
        m_pointerAxisFramePending = true;
        return;
    }

    m_pointerAxisFramePending = false;
    g_pSeatManager->sendPointerFrame();
}
void SpikeInputManager::sendMotionEventsToFocused() {
    if (!Desktop::focusState()->surface() || isConstrained())
        return;

    const auto POINTERSURF = g_pSeatManager->m_state.pointerFocus.lock();
    if (POINTERSURF) {
        const auto POINTERHLSURF = Desktop::View::CWLSurface::fromResource(POINTERSURF);

        // Keep pointer focus on desktop components such as bars or popups that are
        // currently under the cursor. Keyboard focus changes should not steal it.
        if (POINTERHLSURF && (!POINTERHLSURF->view() || POINTERHLSURF->view()->type() != Desktop::View::VIEW_TYPE_WINDOW))
            return;
    }

    const auto SURF = Desktop::focusState()->surface();

    if (!SURF)
        return;

    const auto HLSurf = Desktop::View::CWLSurface::fromResource(SURF);

    if (!HLSurf || !HLSurf->view())
        return;

    const auto VIEW = HLSurf->view();

    if (!VIEW->aliveAndVisible())
        return;

    const auto BOX = HLSurf->getSurfaceBoxGlobal();

    if (!BOX)
        return;

    m_emptyFocusCursorSet = false;

    // HyprVX: the box of window content is in world coordinates.
    const auto POINTER = getMouseCoordsInternal().floor();
    g_pSeatManager->setPointerFocus(SURF, (HyprVXSpike::isWindowContent(SURF) ? HyprVXSpike::screenToWorld(POINTER) : POINTER) - BOX->pos());
}

void HyprVXSpike::addCopiedInputManagerHooks(std::vector<SHookSpec>& specs) {
    specs.push_back({"mouseMoveUnified", "CInputManager::mouseMoveUnified(unsigned int, bool, bool, std::optional<Hyprutils::Math::Vector2D>)", reinterpret_cast<void*>(&SpikeInputManager::hk_mouseMoveUnified), nullptr});
    specs.push_back({"processMouseDownNormal", "CInputManager::processMouseDownNormal(IPointer::SButtonEvent const&, Hyprutils::Memory::CSharedPointer<IPointer>)", reinterpret_cast<void*>(&SpikeInputManager::hk_processMouseDownNormal), nullptr});
    specs.push_back({"onMouseWheel", "CInputManager::onMouseWheel(IPointer::SAxisEvent, Hyprutils::Memory::CSharedPointer<IPointer>)", reinterpret_cast<void*>(&SpikeInputManager::hk_onMouseWheel), nullptr});
    specs.push_back({"sendMotionEventsToFocused", "CInputManager::sendMotionEventsToFocused()", reinterpret_cast<void*>(&SpikeInputManager::hk_sendMotionEventsToFocused), nullptr});
}
