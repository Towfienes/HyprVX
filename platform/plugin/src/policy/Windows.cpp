// Window policy (Feature 002 data-model WindowOffset): offsets set when a window opens.
//
// Spawn placement (R6, US5): a window mapped on the scene workspace of a DRIFT monitor gets
// O(w) = centre(visibleWorldRect(C)) - centre(L(w)) (DriftModel::spawnOffset), so it opens in the middle of the
// viewport under any pan and zoom; L(w) is never written. Event choice (recorded in integration-log.md): window.open,
// the earliest window event with the final L(w): window.openEarly is emitted before the layout places the window
// (Window.cpp:2391, newTarget follows), window.open after it (Window.cpp:2454), both inside mapWindow, so no frame is
// drawn before the offset is set.
//
// Research R9: an X11 override-redirect window (menu, tooltip) inherits its offset at map time
//   1. from the window of its WM_TRANSIENT_FOR parent (CWindow::x11Parent, XSurface m_parent);
//   2. else from the focused X11 window of the same client;
//   3. else 0, logged.
// window.open is emitted inside mapWindow after the window is mapped and before its first frame (Window.cpp), so the
// offset is in place for that frame.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include <src/debug/log/Logger.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/event/EventBus.hpp>
#include <src/output/Monitor.hpp>

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

    void placeSpawned(const PHLWINDOW& window) {
        const auto WS      = window->m_workspace;
        const auto MONITOR = WS ? WS->m_monitor.lock() : nullptr;
        auto&      model   = HyprVXPolicy::model();
        if (!MONITOR || model.sceneWorkspace(MONITOR->m_id) != WS->m_id)
            return;
        const auto CAMERA = model.monitorCamera(MONITOR->m_id, HyprVX::monitorOrigin(*MONITOR));
        const auto POS    = window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        const auto SIZE   = window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);
        const auto OFFSET = hyprvx::drift::DriftModel::spawnOffset(CAMERA, {MONITOR->m_size.x, MONITOR->m_size.y}, {{POS.x, POS.y}, {SIZE.x, SIZE.y}});
        if (!model.setOffset(idOf(window), OFFSET))
            return;
        Log::logger->log(Log::DEBUG, "[hyprvx] window 0x{:x} mapped in Drift on {}: offset ({}, {}) centres it in the viewport", idOf(window), MONITOR->m_name,
                         OFFSET.x, OFFSET.y);
        HyprVXPolicy::sync();
    }

    void onOpen(const PHLWINDOW& window) {
        if (!window)
            return;
        if (window->isX11OverrideRedirect())
            inheritOverrideRedirect(window);
        else
            placeSpawned(window);
    }
}

void HyprVXPolicy::initWindows() {
    g_listeners.push_back(Event::bus()->m_events.window.open.listen([](PHLWINDOW w) { onOpen(w); }));
}

void HyprVXPolicy::shutdownWindows() {
    g_listeners.clear();
}
