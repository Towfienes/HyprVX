// Window policy (Feature 002 data-model WindowOffset): offsets set when a window opens. Research R9: an X11
// override-redirect window (menu, tooltip) inherits its offset at map time
//   1. from the window of its WM_TRANSIENT_FOR parent (CWindow::x11Parent, XSurface m_parent);
//   2. else from the focused X11 window of the same client;
//   3. else 0, logged.
// window.open is emitted inside mapWindow after the window is mapped and before its first frame (Window.cpp), so the
// offset is in place for that frame. Spawn placement (R6, T051) and the move drag (T048) are added here later.

#include "policy/Plugin.hpp"

#include <src/debug/log/Logger.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/event/EventBus.hpp>

#include <vector>

namespace {
    std::vector<CHyprSignalListener> g_listeners;

    hyprvx::drift::WindowId idOf(const PHLWINDOW& window) {
        return rc<uintptr_t>(window.get());
    }

    void inheritOverrideRedirect(const PHLWINDOW& window) {
        PHLWINDOW   from   = window->x11Parent();
        const char* source = "parent";
        if (!from) {
            const auto FOCUSED = Desktop::focusState()->window();
            if (FOCUSED && FOCUSED != window && FOCUSED->m_isX11 && FOCUSED->getPID() == window->getPID())
                from = FOCUSED;
            source = "focused window of its client";
        }
        if (!from) {
            Log::logger->log(Log::INFO, "[hyprvx] override-redirect window 0x{:x} has no parent and no focused window of its client; offset 0",
                             idOf(window));
            return;
        }
        const auto OFFSET = HyprVXPolicy::model().offset(idOf(from));
        if (OFFSET.x == 0 && OFFSET.y == 0)
            return;
        HyprVXPolicy::model().setOffset(idOf(window), OFFSET);
        Log::logger->log(Log::DEBUG, "[hyprvx] override-redirect window 0x{:x} takes offset ({}, {}) of its {} 0x{:x}", idOf(window), OFFSET.x, OFFSET.y,
                         source, idOf(from));
        HyprVXPolicy::sync();
    }

    void onOpen(const PHLWINDOW& window) {
        if (!window)
            return;
        if (window->isX11OverrideRedirect())
            inheritOverrideRedirect(window);
    }
}

void HyprVXPolicy::initWindows() {
    g_listeners.push_back(Event::bus()->m_events.window.open.listen([](PHLWINDOW w) { onOpen(w); }));
}

void HyprVXPolicy::shutdownWindows() {
    g_listeners.clear();
}
