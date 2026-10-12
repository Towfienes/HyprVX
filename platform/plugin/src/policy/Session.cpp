// Drift session adapter (Feature 002, data-model.md, contracts/drift-control.md 1.0): one DriftModel for the
// compositor (a session per monitor), the hyprvx:toggle/on/off/panic dispatchers, the entering validation (R10), the
// mechanism sync, the monitor/workspace/window listeners (camera per workspace R2, no switch animation in Drift R7),
// `hyprctl hyprvx state` (bookkeeping only, Constitution II) and the unload path.
//
// The mechanism holds no policy state: after every transition syncMechanism() pushes what the model says, i.e. the
// scene camera of each DRIFT monitor (identity elsewhere) and O(w) for mapped windows of a DRIFT scene (0 for every
// other window). Leaving DRIFT therefore leaves the mechanism with no non-identity state (FR-003), while the stored
// cameras and offsets stay in the model for re-entry (US1.4).

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include "hyprvx/core/DriftModel.hpp"

#include <src/debug/HyprCtl.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/event/EventBus.hpp>
#include <src/helpers/MiscFunctions.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorQuery.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/WorkspaceQuery.hpp>
#include <src/state/WorkspaceState.hpp>

#include <format>
#include <string_view>
#include <vector>

using namespace hyprvx;

namespace {
    struct SSession {
        drift::DriftModel                model;
        SP<SHyprCtlCommand>              hyprctl;
        std::vector<CHyprSignalListener> listeners;
    };
    SSession* g_session  = nullptr;
    bool      g_anyDrift = false;

    drift::WindowId idOf(const PHLWINDOW& window) {
        return rc<uintptr_t>(window.get());
    }

    const char* modeName(drift::Mode mode) {
        switch (mode) {
            case drift::Mode::Off: return "OFF";
            case drift::Mode::Entering: return "ENTERING";
            case drift::Mode::Drift: return "DRIFT";
            case drift::Mode::Exiting: return "EXITING";
        }
        return "?";
    }

    SDispatchResult error(std::string text) {
        return {.success = false, .error = std::move(text)};
    }

    // Pushes the model to the mechanism (see the file comment). A camera the mechanism rejects ends Drift on that
    // monitor (data-model: invalid camera -> EXITING -> OFF), which also needs the window pass.
    void syncMechanism(bool camerasOnly = false) {
        auto& model = g_session->model;
        bool  any   = false;
        for (auto const& m : State::monitorState()->monitors()) {
            const auto ORIGIN = HyprVX::monitorOrigin(*m);
            model.setMonitorOrigin(m->m_id, ORIGIN);
            if (model.mode(m->m_id) != drift::Mode::Drift) {
                if (!HyprVX::cameraIsIdentity(*m))
                    HyprVX::resetCamera(*m);
                continue;
            }
            if (HyprVX::setCamera(*m, model.monitorCamera(m->m_id, ORIGIN))) {
                any = true;
                continue;
            }
            Log::logger->log(Log::ERR, "[hyprvx] camera rejected on {}; Drift OFF", m->m_name);
            model.invalidCamera(m->m_id);
            HyprVX::resetCamera(*m);
            model.exited(m->m_id);
            camerasOnly = false;
        }
        g_anyDrift = any;
        if (camerasOnly)
            return;
        for (auto const& w : Desktop::windowState()->windows()) {
            const auto WS     = w->m_workspace;
            const auto MON    = WS ? WS->m_monitor.lock() : nullptr;
            const auto OFFSET = model.mechanismOffset(idOf(w), WS ? WS->m_id : WORKSPACE_INVALID, MON ? MON->m_id : MONITOR_INVALID, w->m_isMapped);
            HyprVX::setWindowOffset(w, Vector2D{OFFSET.x, OFFSET.y});
        }
    }

    // R7: a workspace switch in Drift is not animated. The switch animations are started in changeWorkspace before
    // workspace.active is emitted (Monitor.cpp), so they are finished here, before the next frame.
    void finishSwitchAnimation(const PHLWORKSPACE& workspace) {
        if (!workspace)
            return;
        workspace->m_renderOffset->warp();
        workspace->m_alpha->warp();
    }

    void workspaceActive(const PHLWORKSPACE& workspace) {
        HyprVXPolicy::stopGlide();  // Feature 003 FR-016: a workspace change stops a glide
        if (!workspace || workspace->m_isSpecialWorkspace)
            return;
        const auto MONITOR = workspace->m_monitor.lock();
        if (!MONITOR)
            return;
        auto&      model    = g_session->model;
        const auto PREVIOUS = model.sceneWorkspace(MONITOR->m_id);
        if (model.mode(MONITOR->m_id) != drift::Mode::Drift || PREVIOUS == workspace->m_id)
            return;
        model.workspaceActivated(MONITOR->m_id, workspace->m_id, false);
        finishSwitchAnimation(workspace);
        if (PREVIOUS)
            finishSwitchAnimation(State::workspaceState()->query().id(*PREVIOUS).run());
        syncMechanism();
    }

    SDispatchResult enableOn(const PHLMONITOR& monitor) {
        if (!monitor)
            return error("err:state");
        auto&      model   = g_session->model;
        const auto WS      = monitor->m_activeWorkspace;
        const bool SPECIAL = !WS || WS->m_isSpecialWorkspace || monitor->m_activeSpecialWorkspace;
        model.setMonitorOrigin(monitor->m_id, HyprVX::monitorOrigin(*monitor));
        switch (model.enable(monitor->m_id, WS ? WS->m_id : WORKSPACE_INVALID, SPECIAL)) {
            case drift::Status::Ok: break;
            case drift::Status::NoOp: return {};
            default: return error("err:state");
        }
        const auto CHECK = HyprVXPolicy::checkEntering(HyprVXPolicy::PHANDLE);
        if (model.entered(monitor->m_id, CHECK.ok) != drift::Status::Ok) {
            const auto TEXT = std::format("HyprVX: Drift Mode unavailable, built for {} but running {}", CHECK.expected, CHECK.running);
            HyprlandAPI::addNotification(HyprVXPolicy::PHANDLE, TEXT, CHyprColor{1.0, 0.3, 0.3, 1.0}, 8000);
            Log::logger->log(Log::ERR, "[hyprvx] {}", TEXT);
            return error("err:incompatible");
        }
        syncMechanism();
        Log::logger->log(Log::INFO, "[hyprvx] Drift ON on {} (workspace {})", monitor->m_name, WS->m_id);
        return {};
    }

    SDispatchResult disableOn(const PHLMONITOR& monitor) {
        if (!monitor)
            return {};
        HyprVXPolicy::stopGlide();  // Feature 003 FR-016
        auto& model = g_session->model;
        if (model.disable(monitor->m_id) == drift::Status::NoOp)
            return {};
        syncMechanism(); // EXITING: identity camera, offset 0 for the scene's windows
        model.exited(monitor->m_id);
        Log::logger->log(Log::INFO, "[hyprvx] Drift OFF on {}", monitor->m_name);
        return {};
    }

    SDispatchResult toggle(std::string) {
        const auto MONITOR = Desktop::focusState()->monitor();
        if (MONITOR && g_session->model.mode(MONITOR->m_id) != drift::Mode::Off)
            return disableOn(MONITOR);
        return enableOn(MONITOR);
    }

    SDispatchResult on(std::string) {
        return enableOn(Desktop::focusState()->monitor());
    }

    SDispatchResult off(std::string) {
        return disableOn(Desktop::focusState()->monitor());
    }

    SDispatchResult panic(std::string) {
        HyprVXPolicy::stopGlide();
        g_session->model.panic();
        syncMechanism();
        Log::logger->log(Log::INFO, "[hyprvx] panic: Drift OFF on every monitor");
        return {};
    }

    void monitorGone(const PHLMONITOR& monitor) {
        if (!monitor)
            return;
        if (HyprVXPolicy::glideActive(monitor->m_id))
            HyprVXPolicy::stopGlide();
        const bool WAS_ON = g_session->model.mode(monitor->m_id) != drift::Mode::Off;
        g_session->model.monitorRemoved(monitor->m_id);
        syncMechanism();
        if (WAS_ON)
            Log::logger->log(Log::INFO, "[hyprvx] monitor {} removed; its Drift session is OFF", monitor->m_name);
    }

    std::string stateJSON() {
        auto&       model = g_session->model;
        std::string monitors;
        for (auto const& m : State::monitorState()->monitors()) {
            const auto CAMERA = HyprVX::cameraOf(*m);
            const auto WS     = m->m_activeWorkspace ? m->m_activeWorkspace->m_id : WORKSPACE_INVALID;
            if (!monitors.empty())
                monitors += ",";
            monitors += std::format(R"({{"monitor":"{}","state":"{}","workspace":{},"P":[{:.17g},{:.17g}],"z":{:.17g},"glide":{}}})",
                                    escapeJSONStrings(m->m_name), modeName(model.mode(m->m_id)), WS, CAMERA.origin.x, CAMERA.origin.y, CAMERA.zoom,
                                    HyprVXPolicy::glideActive(m->m_id) ? "true" : "false");
        }
        std::string windows;
        for (auto const& [address, offset] : model.offsets()) {
            WORKSPACEID workspace = WORKSPACE_INVALID;
            for (auto const& w : Desktop::windowState()->windows()) {
                if (idOf(w) == address && w->m_workspace)
                    workspace = w->m_workspace->m_id;
            }
            if (!windows.empty())
                windows += ",";
            windows += std::format(R"({{"address":"0x{:x}","workspace":{},"offset":[{:.17g},{:.17g}]}})", address, workspace, offset.x, offset.y);
        }
        return std::format(R"({{"monitors":[{}],"windows":[{}],"build":{}}})", monitors, windows, HyprVXPolicy::buildJSON(HyprVXPolicy::PHANDLE));
    }

    // Registered non-exact, so Hyprland hands over every request that starts with "hyprvx" (the first matching non-exact
    // command wins, HyprCtl.cpp), "hyprvx-dev ..." included. Only the command word "hyprvx" is ours: for any other an
    // empty result makes Hyprland answer "unknown request", so a build without the dev control rejects hyprvx-dev
    // (FR-016). In test builds the dev control registers before the policy and is matched first.
    std::string hyprctlCommand(eHyprCtlOutputFormat, std::string in) {
        const std::string_view REQUEST = in;
        if (REQUEST != "hyprvx" && !REQUEST.starts_with("hyprvx "))
            return "";
        if (REQUEST != "hyprvx state")
            return "err:invalid";
        return stateJSON();
    }
}

hyprvx::drift::DriftModel& HyprVXPolicy::model() {
    return g_session->model;
}

void HyprVXPolicy::sync(bool camerasOnly) {
    if (g_session)
        syncMechanism(camerasOnly);
}

PHLMONITOR HyprVXPolicy::driftMonitorUnderPointer() {
    if (!g_anyDrift || !g_session)
        return nullptr;
    const auto MONITOR = State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run();
    return MONITOR && g_session->model.mode(MONITOR->m_id) == drift::Mode::Drift ? MONITOR : nullptr;
}

bool HyprVXPolicy::anyDrift() {
    return g_anyDrift;
}

bool HyprVXPolicy::driftUnderPointer() {
    return driftMonitorUnderPointer() != nullptr;
}

void HyprVXPolicy::disableFocused() {
    if (g_session)
        disableOn(Desktop::focusState()->monitor());
}

void HyprVXPolicy::initSession(HANDLE handle) {
    g_session          = new SSession();
    g_session->hyprctl = HyprlandAPI::registerHyprCtlCommand(handle, SHyprCtlCommand{.name = "hyprvx", .exact = false, .fn = hyprctlCommand});
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:toggle", toggle);
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:on", on);
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:off", off);
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:panic", panic);

    auto& events = Event::bus()->m_events;
    g_session->listeners.push_back(events.monitor.removed.listen([](PHLMONITOR m) { monitorGone(m); }));
    g_session->listeners.push_back(events.monitor.destroyMon.listen([](PHLMONITOR m) { monitorGone(m); }));
    g_session->listeners.push_back(events.window.close.listen([](PHLWINDOW w) {
        if (w)
            g_session->model.windowClosed(idOf(w));
    }));
    g_session->listeners.push_back(events.workspace.active.listen([](PHLWORKSPACE ws) { workspaceActive(ws); }));
    // A window that changes workspace keeps its offset, shown only in a Drift scene (data-model WindowOffset).
    g_session->listeners.push_back(events.window.moveToWorkspace.listen([](PHLWINDOW w, PHLWORKSPACE) {
        if (!w || !g_anyDrift)
            return;
        g_session->model.windowLeftScene(idOf(w));
        syncMechanism();
    }));
    initCamera(handle);
    initWindows();
    initInput();
    initGesture(handle);
    Log::logger->log(Log::INFO, "[hyprvx] Drift policy loaded (form {}); Drift is OFF", HYPRVX_FORM_NAME);
}

void HyprVXPolicy::shutdownSession() {
    if (!g_session)
        return;
    // Unload acts as panic while the mechanism is still in place: every camera identity, every offset 0 (dev offsets
    // included, mechanism-api resetWindowOffsets).
    shutdownGesture();
    shutdownInput();
    shutdownWindows();
    shutdownCamera();
    g_session->listeners.clear();
    g_session->model.panic();
    syncMechanism();
    HyprVX::resetWindowOffsets();
    if (g_session->hyprctl)
        HyprlandAPI::unregisterHyprCtlCommand(PHANDLE, g_session->hyprctl);
    delete g_session;
    g_session  = nullptr;
    g_anyDrift = false;
    Log::logger->log(Log::INFO, "[hyprvx] Drift policy unloaded");
}
