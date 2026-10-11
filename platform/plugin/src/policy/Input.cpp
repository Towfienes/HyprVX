// Built-in Drift inputs (contracts/drift-control.md 1.0, research R4). Listeners on Hyprland's input event bus, which
// runs before keybinds and clients (InputManager: a cancelled event goes nowhere else). Every listener returns at once
// while no monitor is in DRIFT (FR-005: input is not intercepted with Drift OFF), and acts only when the monitor under
// the pointer is in DRIFT.
//
// - Super + Escape: hyprvx:off on the focused monitor; only that key press is cancelled (its release is not in
//   Hyprland's pressed-key list, so it is dropped too).
// - Super + vertical wheel: zoom by zoom_step per detent at the pointer; steps = -value120 / 120 (wheel up zooms in).
// - Super + left button on empty canvas (no window, no top/overlay layer surface under the pointer, FR-007): pan drag,
//   content follows the pointer. Press, motion and release are cancelled while the drag lasts.
// The move drag (Super + left button on a window) is added in T048.

#include "policy/Plugin.hpp"

#include <src/desktop/state/ViewHitTester.hpp>
#include <src/desktop/state/ViewState.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/devices/IKeyboard.hpp>
#include <src/devices/IPointer.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/protocols/LayerShell.hpp>

#include <linux/input-event-codes.h>

#include <cmath>
#include <vector>

namespace {
    std::vector<CHyprSignalListener> g_listeners;
    // The left button press that started a Drift drag was cancelled; its release is cancelled too.
    bool g_dragButton = false;

    bool superHeld() {
        return g_pInputManager->getModsFromAllKBs() & HL_MODIFIER_META;
    }

    hyprvx::cammath::Vec pointer() {
        const auto POS = g_pInputManager->getMouseCoordsInternal();
        return {POS.x, POS.y};
    }

    // FR-007: empty canvas = no window content (through the camera and offsets, patch 0011) and no top or overlay
    // layer surface (or layer popup) under the pointer.
    bool emptyCanvas(const PHLMONITOR& monitor, const Vector2D& pos) {
        const auto& hit = Desktop::viewState()->hitTest();
        Vector2D local;
        PHLLS    layer;
        if (hit.layerPopupSurfaceAt(pos, monitor, &local, &layer))
            return false;
        if (hit.layerSurfaceAt(pos, &monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &local, &layer))
            return false;
        if (hit.layerSurfaceAt(pos, &monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &local, &layer))
            return false;
        return !hit.windowAt(pos, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);
    }

    void onKey(const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
        if (event.keycode != KEY_ESC || event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
            return;
        if (!superHeld() || !HyprVXPolicy::driftUnderPointer())
            return;
        info.cancelled = true;
        HyprVXPolicy::disableFocused();
    }

    void onAxis(const IPointer::SAxisEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
        if (event.axis != WL_POINTER_AXIS_VERTICAL_SCROLL || event.source != WL_POINTER_AXIS_SOURCE_WHEEL || !superHeld())
            return;
        const auto MONITOR = HyprVXPolicy::driftMonitorUnderPointer();
        if (!MONITOR)
            return;
        info.cancelled = true;
        if (event.deltaDiscrete == 0)
            return;
        const double STEPS = -event.deltaDiscrete / 120.0;
        HyprVXPolicy::zoomAt(MONITOR, g_pInputManager->getMouseCoordsInternal(), std::pow(HyprVXPolicy::zoomStep(), STEPS));
    }

    void onButton(const IPointer::SButtonEvent& event, Event::SCallbackInfo& info) {
        if (event.button != BTN_LEFT)
            return;
        if (event.state == WL_POINTER_BUTTON_STATE_RELEASED) {
            if (!g_dragButton)
                return;
            g_dragButton = false;
            HyprVXPolicy::model().dragEnd();
            info.cancelled = true;
            return;
        }
        if (!HyprVXPolicy::anyDrift() || !superHeld())
            return;
        const auto MONITOR = HyprVXPolicy::driftMonitorUnderPointer();
        if (!MONITOR || !emptyCanvas(MONITOR, g_pInputManager->getMouseCoordsInternal()))
            return;
        if (!HyprVXPolicy::model().beginPan(MONITOR->m_id, pointer()))
            return;
        g_dragButton   = true;
        info.cancelled = true;
    }

    void onMove(Event::SCallbackInfo& info) {
        // A drag ended by Drift leaving DRIFT (or panic) stops taking motion; only the release stays cancelled.
        if (!g_dragButton || !HyprVXPolicy::model().drag())
            return;
        info.cancelled = true;
        if (HyprVXPolicy::model().dragMotion(pointer()))
            HyprVXPolicy::sync(true);
    }
}

void HyprVXPolicy::initInput() {
    auto& input = Event::bus()->m_events.input;
    g_listeners.push_back(input.keyboard.key.listen([](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) { onKey(event, info); }));
    g_listeners.push_back(input.mouse.axis.listen([](IPointer::SAxisEvent event, Event::SCallbackInfo& info) { onAxis(event, info); }));
    g_listeners.push_back(input.mouse.button.listen([](IPointer::SButtonEvent event, Event::SCallbackInfo& info) { onButton(event, info); }));
    g_listeners.push_back(input.mouse.move.listen([](Vector2D, Event::SCallbackInfo& info) { onMove(info); }));
}

void HyprVXPolicy::shutdownInput() {
    g_listeners.clear();
    g_dragButton = false;
}
