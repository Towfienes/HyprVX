// Drift session adapter (Feature 002): DriftModel per compositor, `hyprctl hyprvx state` (bookkeeping only,
// Constitution II), and the unload path. The state machine, dispatchers and listeners are added in T030 and later.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include "hyprvx/core/DriftModel.hpp"

#include <src/debug/HyprCtl.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/helpers/MiscFunctions.hpp>
#include <src/output/Monitor.hpp>
#include <src/state/MonitorState.hpp>

#include <format>

using namespace hyprvx;

namespace {
    struct SSession {
        drift::DriftModel   model;
        SP<SHyprCtlCommand> hyprctl;
    };
    SSession* g_session = nullptr;

    const char* modeName(drift::Mode mode) {
        switch (mode) {
            case drift::Mode::Off: return "OFF";
            case drift::Mode::Entering: return "ENTERING";
            case drift::Mode::Drift: return "DRIFT";
            case drift::Mode::Exiting: return "EXITING";
        }
        return "?";
    }

    std::string stateJSON() {
        std::string monitors;
        for (auto const& m : State::monitorState()->monitors()) {
            const auto CAMERA = HyprVX::cameraOf(*m);
            const auto WS     = m->m_activeWorkspace ? m->m_activeWorkspace->m_id : WORKSPACE_INVALID;
            if (!monitors.empty())
                monitors += ",";
            monitors += std::format(R"({{"monitor":"{}","state":"{}","workspace":{},"P":[{:.17g},{:.17g}],"z":{:.17g}}})", escapeJSONStrings(m->m_name),
                                    modeName(g_session->model.mode(m->m_id)), WS, CAMERA.origin.x, CAMERA.origin.y, CAMERA.zoom);
        }
        std::string windows;
        for (auto const& [address, offset] : g_session->model.offsets()) {
            if (!windows.empty())
                windows += ",";
            windows += std::format(R"({{"address":"0x{:x}","offset":[{:.17g},{:.17g}]}})", address, offset.x, offset.y);
        }
        return std::format(R"({{"monitors":[{}],"windows":[{}],"build":{}}})", monitors, windows, HyprVXPolicy::buildJSON(HyprVXPolicy::PHANDLE));
    }

    std::string hyprctlCommand(eHyprCtlOutputFormat, std::string in) {
        if (in.find("state") == std::string::npos)
            return "err:invalid";
        return stateJSON();
    }
}

void HyprVXPolicy::initSession(HANDLE handle) {
    g_session          = new SSession();
    g_session->hyprctl = HyprlandAPI::registerHyprCtlCommand(handle, SHyprCtlCommand{.name = "hyprvx", .exact = false, .fn = hyprctlCommand});
    Log::logger->log(Log::INFO, "[hyprvx] Drift policy loaded (form {}); Drift is OFF", HYPRVX_FORM_NAME);
}

void HyprVXPolicy::shutdownSession() {
    if (!g_session)
        return;
    // Unload acts as panic: every camera back to identity while the mechanism is still in place.
    g_session->model.panic();
    for (auto const& m : State::monitorState()->monitors())
        HyprVX::resetCamera(*m);
    if (g_session->hyprctl)
        HyprlandAPI::unregisterHyprCtlCommand(PHANDLE, g_session->hyprctl);
    delete g_session;
    g_session = nullptr;
    Log::logger->log(Log::INFO, "[hyprvx] Drift policy unloaded");
}
