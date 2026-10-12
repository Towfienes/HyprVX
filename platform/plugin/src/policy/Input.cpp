// Built-in Drift inputs (contracts/drift-control.md 1.0, research R4). Listeners on Hyprland's input event bus, which
// runs before keybinds and clients (InputManager: a cancelled event goes nowhere else). Every listener returns at once
// while no monitor is in DRIFT (FR-005: input is not intercepted with Drift OFF), and acts only when the monitor under
// the pointer is in DRIFT.
//
// - Super + Escape: hyprvx:off on the focused monitor; only that key press is cancelled (its release is not in
//   Hyprland's pressed-key list, so it is dropped too).
// - Super + vertical wheel: zoom by zoom_step per detent at the pointer; steps = -value120 / 120 (wheel up zooms in).
// - Super + left button on empty canvas (no window, no top/overlay layer surface under the pointer, FR-007): pan drag,
//   content follows the pointer.
// - Super + left button on a window (FR-008): move drag, O(w) = startOffset + worldDelta(C, S - startScreen); only the
//   offset changes, Hyprland's window model is never written. The user's own Super + left bind never sees the press.
// Press, motion and release are cancelled while a drag lasts. The model ends a drag on every data-model end condition
// (release, leaving DRIFT, panic, the window closing or leaving the scene, a workspace change); motion is then no
// longer taken, and only the release of the cancelled press stays cancelled.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

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
    // The window of a move drag.
    PHLWINDOWREF g_dragWindow;


    hyprvx::cammath::Vec pointer() {
        const auto POS = g_pInputManager->getMouseCoordsInternal();
        return {POS.x, POS.y};
    }


    void onKey(const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
        if (event.keycode != KEY_ESC || event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
            return;
        if (!HyprVXPolicy::superHeld() || !HyprVXPolicy::driftUnderPointer())
            return;
        info.cancelled = true;
        HyprVXPolicy::disableFocused();
    }

    void onAxis(const IPointer::SAxisEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
        if (event.axis != WL_POINTER_AXIS_VERTICAL_SCROLL || event.source != WL_POINTER_AXIS_SOURCE_WHEEL || !HyprVXPolicy::superHeld())
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
            g_dragWindow.reset();
            HyprVXPolicy::model().dragEnd();
            info.cancelled = true;
            return;
        }
        if (!HyprVXPolicy::anyDrift() || !HyprVXPolicy::superHeld())
            return;
        const auto MONITOR = HyprVXPolicy::driftMonitorUnderPointer();
        if (!MONITOR)
            return;
        const auto TARGET = HyprVXPolicy::pointerTarget(MONITOR, g_pInputManager->getMouseCoordsInternal());
        if (TARGET.layer)
            return;
        bool started = false;
        if (TARGET.window) {
            started      = HyprVXPolicy::model().beginMove(MONITOR->m_id, rc<uintptr_t>(TARGET.window.get()), pointer());
            g_dragWindow = TARGET.window;
        } else
            started = HyprVXPolicy::model().beginPan(MONITOR->m_id, pointer());
        if (!started)
            return;
        g_dragButton   = true;
        info.cancelled = true;
    }

    void onMove(Event::SCallbackInfo& info) {
        // A drag the model ended stops taking motion; only the release stays cancelled.
        const auto& DRAG = HyprVXPolicy::model().drag();
        if (!g_dragButton || !DRAG)
            return;
        info.cancelled = true;
        if (!HyprVXPolicy::model().dragMotion(pointer()))
            return;
        if (DRAG->kind == hyprvx::drift::DragKind::Pan) {
            HyprVXPolicy::sync(true);
            return;
        }
        // Move: only the dragged window's offset changes (it is a scene window while the drag lasts).
        if (const auto WINDOW = g_dragWindow.lock()) {
            const auto OFFSET = HyprVXPolicy::model().offset(DRAG->window);
            HyprVX::setWindowOffset(WINDOW, Vector2D{OFFSET.x, OFFSET.y});
        }
    }
}

bool HyprVXPolicy::superHeld() {
    return g_pInputManager->getModsFromAllKBs() & HL_MODIFIER_META;
}

// What the pointer is over: a top/overlay layer surface or layer popup, a window (hit through the camera and offsets,
// patch 0011) or empty canvas. Super + left uses it for pan/move (FR-007); trackpad sequences decide their target
// with it at their start (Feature 003 R3/R4).
HyprVXPolicy::SPointerTarget HyprVXPolicy::pointerTarget(const PHLMONITOR& monitor, const Vector2D& pos) {
    const auto& hit = Desktop::viewState()->hitTest();
    Vector2D    local;
    PHLLS       layer;
    if (hit.layerPopupSurfaceAt(pos, monitor, &local, &layer) ||
        hit.layerSurfaceAt(pos, &monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &local, &layer) ||
        hit.layerSurfaceAt(pos, &monitor->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &local, &layer))
        return {.layer = true};
    return {.window = hit.windowAt(pos, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING)};
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
    g_dragWindow.reset();
}
