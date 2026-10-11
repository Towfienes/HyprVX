// Drift camera controls (contracts/drift-control.md 1.0): hyprvx:pan, hyprvx:zoom, hyprvx:reset and the config value
// plugin:hyprvx:zoom_step. All camera arithmetic goes through DriftModel and CameraMath (Constitution IV); this file
// parses, picks the monitor and pushes the result to the mechanism.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include <src/config/values/types/FloatValue.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>

#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

using namespace hyprvx;

namespace {
    constexpr double                DEFAULT_ZOOM_STEP = 1.1;
    SP<Config::Values::CFloatValue> g_zoomStep;

    SDispatchResult error(std::string text) {
        return {.success = false, .error = std::move(text)};
    }

    SDispatchResult result(drift::Status status) {
        switch (status) {
            case drift::Status::Ok:
            case drift::Status::NoOp: return {};
            case drift::Status::State: return error("err:state");
            default: return error("err:invalid");
        }
    }

    std::vector<std::string_view> words(std::string_view args) {
        std::vector<std::string_view> out;
        size_t                        i = 0;
        while (i < args.size()) {
            while (i < args.size() && args[i] == ' ')
                ++i;
            const auto START = i;
            while (i < args.size() && args[i] != ' ')
                ++i;
            if (i > START)
                out.push_back(args.substr(START, i - START));
        }
        return out;
    }

    std::optional<double> number(std::string_view word) {
        double     value = 0;
        const auto RES   = std::from_chars(word.data(), word.data() + word.size(), value);
        if (RES.ec != std::errc{} || RES.ptr != word.data() + word.size() || !std::isfinite(value))
            return std::nullopt;
        return value;
    }

    // A camera change needs only the camera pass of the sync.
    drift::Status applied(drift::Status status) {
        if (status == drift::Status::Ok)
            HyprVXPolicy::sync(true);
        return status;
    }

    SDispatchResult pan(std::string args) {
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR || HyprVXPolicy::model().mode(MONITOR->m_id) != drift::Mode::Drift)
            return error("err:state");
        const auto WORDS = words(args);
        const auto DX    = WORDS.size() == 2 ? number(WORDS[0]) : std::nullopt;
        const auto DY    = WORDS.size() == 2 ? number(WORDS[1]) : std::nullopt;
        if (!DX || !DY)
            return error("err:invalid");
        return result(applied(HyprVXPolicy::model().pan(MONITOR->m_id, {*DX, *DY})));
    }

    SDispatchResult zoom(std::string args) {
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR || HyprVXPolicy::model().mode(MONITOR->m_id) != drift::Mode::Drift)
            return error("err:state");
        const auto WORDS  = words(args);
        const auto FACTOR = WORDS.size() == 1 ? number(WORDS[0]) : std::nullopt;
        if (!FACTOR || !(*FACTOR > 0.0))
            return error("err:invalid");
        return HyprVXPolicy::zoomAt(MONITOR, g_pInputManager->getMouseCoordsInternal(), *FACTOR) ? SDispatchResult{} : error("err:invalid");
    }

    SDispatchResult reset(std::string) {
        const auto MONITOR = Desktop::focusState()->monitor();
        if (!MONITOR)
            return error("err:state");
        return result(applied(HyprVXPolicy::model().reset(MONITOR->m_id, HyprVX::monitorOrigin(*MONITOR))));
    }
}

bool HyprVXPolicy::zoomAt(const PHLMONITOR& monitor, const Vector2D& anchor, double factor) {
    if (!monitor)
        return false;
    return applied(model().zoom(monitor->m_id, HyprVX::monitorOrigin(*monitor), {anchor.x, anchor.y}, factor)) == drift::Status::Ok;
}

double HyprVXPolicy::zoomStep() {
    const double STEP = g_zoomStep ? g_zoomStep->value() : DEFAULT_ZOOM_STEP;
    return std::isfinite(STEP) && STEP > 1.0 ? STEP : DEFAULT_ZOOM_STEP;
}

void HyprVXPolicy::initCamera(HANDLE handle) {
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:pan", pan);
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:zoom", zoom);
    HyprlandAPI::addDispatcherV2(handle, "hyprvx:reset", reset);
    g_zoomStep = makeShared<Config::Values::CFloatValue>("plugin:hyprvx:zoom_step", "HyprVX Drift: zoom factor per wheel detent (> 1)", DEFAULT_ZOOM_STEP);
    HyprlandAPI::addConfigValueV2(handle, g_zoomStep);
}

void HyprVXPolicy::shutdownCamera() {
    g_zoomStep.reset();
}
