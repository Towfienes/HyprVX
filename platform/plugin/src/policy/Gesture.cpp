// Trackpad inputs of Drift Mode (Feature 003, contracts/drift-control.md 1.1, research R1-R8): finger-source scroll
// sequences, swipe and pinch gestures, decided once at their start by hyprvx::drift::decideTarget
// (contracts/gesture-model.md) and consumed through the cancellable event bus before keybinds, `gesture =` bindings
// and client delivery. Camera changes go only through DriftModel and CameraMath (Constitution IV).
//
// T015 skeleton: listeners, config values and shutdown; every sequence passes through unchanged.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include "hyprvx/core/GestureModel.hpp"

#include <src/config/values/types/FloatValue.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/devices/IPointer.hpp>
#include <src/event/EventBus.hpp>

#include <cmath>
#include <vector>

using namespace hyprvx;

namespace {
    constexpr double DEFAULT_PAN_FACTOR      = 1.0;
    constexpr double DEFAULT_GLIDE           = 0.5;
    constexpr double DEFAULT_GLIDE_MIN_SPEED = 100.0;

    std::vector<CHyprSignalListener> g_listeners;
    SP<Config::Values::CFloatValue>  g_panFactor;
    SP<Config::Values::CFloatValue>  g_glide;
    SP<Config::Values::CFloatValue>  g_glideMinSpeed;

    // An invalid config value falls back to its default with one log line per key (drift-control 1.1).
    struct SValidated {
        const char* key;
        bool        logged = false;
    };
    SValidated g_panFactorCheck{"plugin:hyprvx:pan_factor"};
    SValidated g_glideCheck{"plugin:hyprvx:glide"};
    SValidated g_glideMinSpeedCheck{"plugin:hyprvx:glide_min_speed"};

    template <typename F>
    double validated(const SP<Config::Values::CFloatValue>& value, double fallback, SValidated& check, F&& valid) {
        const double V = value ? value->value() : fallback;
        if (std::isfinite(V) && valid(V)) {
            check.logged = false;
            return V;
        }
        if (!check.logged) {
            Log::logger->log(Log::WARN, "[hyprvx] {} = {} is invalid; using {}", check.key, V, fallback);
            check.logged = true;
        }
        return fallback;
    }

    // Skeleton (T015): nothing is consumed yet. Every listener returns at once while no monitor is in Drift.
    void onAxis(const IPointer::SAxisEvent& event, Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift() || event.source != WL_POINTER_AXIS_SOURCE_FINGER)
            return;
    }
    void onGesture(Event::SCallbackInfo& info) {
        if (!HyprVXPolicy::anyDrift())
            return;
    }
}

double HyprVXPolicy::panFactor() {
    return validated(g_panFactor, DEFAULT_PAN_FACTOR, g_panFactorCheck, [](double v) { return v > 0.0; });
}

drift::GlideConfig HyprVXPolicy::glideConfig() {
    drift::GlideConfig cfg;
    cfg.strength = validated(g_glide, DEFAULT_GLIDE, g_glideCheck, [](double v) { return v >= 0.0 && v <= 1.0; });
    cfg.minSpeed = validated(g_glideMinSpeed, DEFAULT_GLIDE_MIN_SPEED, g_glideMinSpeedCheck, [](double v) { return v >= 0.0; });
    return cfg;
}

void HyprVXPolicy::initGesture(HANDLE handle) {
    g_panFactor     = makeShared<Config::Values::CFloatValue>("plugin:hyprvx:pan_factor", "HyprVX Drift: trackpad pan factor (> 0)", DEFAULT_PAN_FACTOR);
    g_glide         = makeShared<Config::Values::CFloatValue>("plugin:hyprvx:glide", "HyprVX Drift: glide strength after a pan, 0..1 (0 = off)", DEFAULT_GLIDE);
    g_glideMinSpeed = makeShared<Config::Values::CFloatValue>("plugin:hyprvx:glide_min_speed", "HyprVX Drift: minimum release speed for a glide (px/s)",
                                                              DEFAULT_GLIDE_MIN_SPEED);
    HyprlandAPI::addConfigValueV2(handle, g_panFactor);
    HyprlandAPI::addConfigValueV2(handle, g_glide);
    HyprlandAPI::addConfigValueV2(handle, g_glideMinSpeed);

    auto& bus = Event::bus()->m_events;
    g_listeners.push_back(bus.input.mouse.axis.listen([](IPointer::SAxisEvent e, Event::SCallbackInfo& info) { onAxis(e, info); }));
    g_listeners.push_back(bus.gesture.swipe.begin.listen([](IPointer::SSwipeBeginEvent, Event::SCallbackInfo& info) { onGesture(info); }));
    g_listeners.push_back(bus.gesture.swipe.update.listen([](IPointer::SSwipeUpdateEvent, Event::SCallbackInfo& info) { onGesture(info); }));
    g_listeners.push_back(bus.gesture.swipe.end.listen([](IPointer::SSwipeEndEvent, Event::SCallbackInfo& info) { onGesture(info); }));
    g_listeners.push_back(bus.gesture.pinch.begin.listen([](IPointer::SPinchBeginEvent, Event::SCallbackInfo& info) { onGesture(info); }));
    g_listeners.push_back(bus.gesture.pinch.update.listen([](IPointer::SPinchUpdateEvent, Event::SCallbackInfo& info) { onGesture(info); }));
    g_listeners.push_back(bus.gesture.pinch.end.listen([](IPointer::SPinchEndEvent, Event::SCallbackInfo& info) { onGesture(info); }));
}

void HyprVXPolicy::shutdownGesture() {
    g_listeners.clear();
    g_panFactor.reset();
    g_glide.reset();
    g_glideMinSpeed.reset();
}
