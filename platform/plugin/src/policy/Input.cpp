// Built-in Drift inputs (contracts/drift-control.md 1.0, research R4). Listeners on Hyprland's input event bus, which
// runs before keybinds and clients (InputManager onKeyboardKey: a cancelled event goes nowhere else). Every listener
// returns at once while no monitor is in DRIFT (FR-005: input is not intercepted with Drift OFF).
//
// Super + Escape: hyprvx:off on the focused monitor; only that key press is cancelled (its release is not in Hyprland's
// pressed-key list, so it is dropped too). Pan, zoom and move inputs are added in T038 and T048.

#include "policy/Plugin.hpp"

#include <src/devices/IKeyboard.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/input/InputManager.hpp>

#include <linux/input-event-codes.h>

#include <vector>

namespace {
    std::vector<CHyprSignalListener> g_listeners;

    void onKey(const IKeyboard::SKeyEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
        if (event.keycode != KEY_ESC || event.state != WL_KEYBOARD_KEY_STATE_PRESSED)
            return;
        if (!(g_pInputManager->getModsFromAllKBs() & HL_MODIFIER_META) || !HyprVXPolicy::driftUnderPointer())
            return;
        info.cancelled = true;
        HyprVXPolicy::disableFocused();
    }
}

void HyprVXPolicy::initInput() {
    g_listeners.push_back(Event::bus()->m_events.input.keyboard.key.listen([](IKeyboard::SKeyEvent event, Event::SCallbackInfo& info) { onKey(event, info); }));
}

void HyprVXPolicy::shutdownInput() {
    g_listeners.clear();
}
