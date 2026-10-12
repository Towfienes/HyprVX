// Trackpad inputs of Drift Mode (Feature 003, contracts/drift-control.md 1.1, research R1-R8): finger-source scroll
// sequences, swipe and pinch gestures, decided once at their start by hyprvx::drift::decideTarget
// (contracts/gesture-model.md) and consumed through the cancellable event bus before keybinds, `gesture =` bindings
// and client delivery. Camera changes go only through DriftModel and CameraMath (Constitution IV).
//
// Implemented: finger-source scroll sequences (T022), pinch gestures (T026). Swipes pass through until T030; glide is
// T036.

#include "policy/Plugin.hpp"
#include "policy/Mechanism.hpp"

#include "hyprvx/core/GestureModel.hpp"

#include <src/config/ConfigValue.hpp>
#include <src/config/values/types/FloatValue.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/devices/IPointer.hpp>
#include <src/desktop/Workspace.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/fullscreen/FullscreenController.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>

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

    // A scroll sequence or gesture, decided once at its start (data-model GestureSession).
    struct SSession {
        drift::Target target = drift::Target::PassThrough;
        MONITORID     monitor = MONITOR_INVALID;
        WORKSPACEID   workspace = WORKSPACE_INVALID;
        bool          live = false; // camera changes allowed: still in DRIFT on the same scene, no drag started since
    };
    drift::ScrollTracker g_tracker;
    SSession             g_scroll;

    // The BeginContext of decideTarget for the pointer's current position (research R3/R4).
    drift::BeginContext beginContext(PHLMONITOR& monitorOut) {
        drift::BeginContext ctx;
        monitorOut = HyprVXPolicy::driftMonitorUnderPointer();
        if (!monitorOut)
            return ctx;
        ctx.drift            = true;
        const auto TARGET    = HyprVXPolicy::pointerTarget(monitorOut, g_pInputManager->getMouseCoordsInternal());
        ctx.layerUnderPointer  = TARGET.layer;
        ctx.windowUnderPointer = TARGET.window != nullptr;
        ctx.fullscreenScene    = monitorOut->m_activeWorkspace && Fullscreen::controller()->hasFullscreen(monitorOut->m_activeWorkspace);
        ctx.dragActive         = HyprVXPolicy::model().drag().has_value();
        ctx.superHeld          = HyprVXPolicy::superHeld();
        return ctx;
    }

    SSession openSession(drift::InputKind kind, unsigned fingers, PHLMONITOR* monitorOut = nullptr) {
        PHLMONITOR monitor;
        const auto CTX = beginContext(monitor);
        if (monitorOut)
            *monitorOut = monitor;
        SSession   session;
        session.target = drift::decideTarget(kind, fingers, CTX);
        if (monitor) {
            session.monitor   = monitor->m_id;
            session.workspace = HyprVXPolicy::model().sceneWorkspace(monitor->m_id).value_or(WORKSPACE_INVALID);
        }
        session.live = session.target == drift::Target::Pan || session.target == drift::Target::Zoom;
        return session;
    }

    // Once false, a session never changes the camera again (FR-013, FR-014): Drift left, the scene changed, or a Super +
    // drag started.
    bool stillLive(SSession& session) {
        if (session.live) {
            auto& model  = HyprVXPolicy::model();
            const auto S = model.sceneWorkspace(session.monitor);
            session.live = model.mode(session.monitor) == drift::Mode::Drift && S && *S == session.workspace && !model.drag();
        }
        return session.live;
    }

    // Screen pan of the content: -(delta) * touchpad scroll factor * pan factor (research R3). A positive value means
    // "scroll down", which moves a document's content up; natural scrolling is applied before the event reaches us.
    void scrollPan(SSession& session, int axis, double delta) {
        static auto PTOUCHPAD = CConfigValue<Config::FLOAT>("input:touchpad:scroll_factor");
        double      touchpad  = *PTOUCHPAD;
        if (!std::isfinite(touchpad) || touchpad <= 0.0)
            touchpad = 1.0;
        const double D = -delta * touchpad * HyprVXPolicy::panFactor();
        if (!stillLive(session) || D == 0.0 || !std::isfinite(D))
            return;
        const cammath::Vec SCREEN = axis == 0 ? cammath::Vec{0.0, D} : cammath::Vec{D, 0.0};
        if (HyprVXPolicy::model().pan(session.monitor, SCREEN) == drift::Status::Ok)
            HyprVXPolicy::sync(true);
    }

    // A swipe or pinch from begin to end: the session plus what a zoom and a cancel need (research R4).
    struct SGesture {
        bool                 open = false;
        drift::InputKind     kind = drift::InputKind::Pinch;
        SSession             session;
        cammath::Vec         origin;  // monitor origin M
        cammath::CameraState start;   // scene camera at begin (cancel restores it)
        cammath::Vec         anchor;  // pointer at begin (pinch anchor)
    };
    SGesture g_gesture;

    void beginGesture(drift::InputKind kind, unsigned fingers, Event::SCallbackInfo& info) {
        g_gesture = {};
        if (!HyprVXPolicy::anyDrift())
            return;  // passes through; update/end see a closed gesture
        PHLMONITOR monitor;
        g_gesture.session = openSession(kind, fingers, &monitor);
        g_gesture.open    = true;
        g_gesture.kind    = kind;
        if (monitor) {
            g_gesture.origin = HyprVX::monitorOrigin(*monitor);
            g_gesture.start  = HyprVXPolicy::model().monitorCamera(monitor->m_id, g_gesture.origin);
        }
        const auto POS   = g_pInputManager->getMouseCoordsInternal();
        g_gesture.anchor = {POS.x, POS.y};
        if (g_gesture.session.target != drift::Target::PassThrough)
            info.cancelled = true;
    }

    // True when this update/end belongs to an open, consumed gesture of KIND (it is then cancelled).
    bool consumed(drift::InputKind kind, Event::SCallbackInfo& info) {
        if (!g_gesture.open || g_gesture.kind != kind || g_gesture.session.target == drift::Target::PassThrough)
            return false;
        info.cancelled = true;
        return true;
    }

    void setSceneCamera(const cammath::CameraState& camera) {
        if (HyprVXPolicy::model().setCamera(g_gesture.session.workspace, camera))
            HyprVXPolicy::sync(true);
    }

    void onPinchBegin(const IPointer::SPinchBeginEvent& event, Event::SCallbackInfo& info) {
        beginGesture(drift::InputKind::Pinch, event.fingers, info);
    }

    // Absolute from the begin camera (cumulative libinput scale), anchored at the begin pointer (FR-008).
    void onPinchUpdate(const IPointer::SPinchUpdateEvent& event, Event::SCallbackInfo& info) {
        if (!consumed(drift::InputKind::Pinch, info) || g_gesture.session.target != drift::Target::Zoom || !stillLive(g_gesture.session))
            return;
        setSceneCamera(drift::pinchCamera(g_gesture.start, g_gesture.origin, g_gesture.anchor, event.scale));
    }

    void onPinchEnd(const IPointer::SPinchEndEvent& event, Event::SCallbackInfo& info) {
        if (consumed(drift::InputKind::Pinch, info) && event.cancelled && g_gesture.session.target == drift::Target::Zoom &&
            stillLive(g_gesture.session))
            setSceneCamera(g_gesture.start);  // FR-012
        g_gesture = {};
    }

    // Finger-source scroll sequences (FR-002, FR-004..FR-006). Wheel, continuous and tilt sources are Feature 002's.
    void onAxis(const IPointer::SAxisEvent& event, Event::SCallbackInfo& info) {
        if (event.source != WL_POINTER_AXIS_SOURCE_FINGER || (!g_tracker.open() && !HyprVXPolicy::anyDrift()))
            return;
        const int  AXIS = event.axis == WL_POINTER_AXIS_VERTICAL_SCROLL ? 0 : 1;
        const auto STEP = g_tracker.feed(AXIS, event.delta, event.timeMs);
        if (STEP == drift::ScrollStep::Ignore)
            return;
        if (STEP == drift::ScrollStep::Begin)
            g_scroll = openSession(drift::InputKind::Scroll, 2);
        if (g_scroll.target == drift::Target::PassThrough)
            return;
        info.cancelled = true;
        if (g_scroll.target == drift::Target::Pan)
            scrollPan(g_scroll, AXIS, event.delta);
        if (STEP == drift::ScrollStep::End)
            g_scroll = {};
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
    g_listeners.push_back(bus.gesture.pinch.begin.listen([](IPointer::SPinchBeginEvent e, Event::SCallbackInfo& info) { onPinchBegin(e, info); }));
    g_listeners.push_back(bus.gesture.pinch.update.listen([](IPointer::SPinchUpdateEvent e, Event::SCallbackInfo& info) { onPinchUpdate(e, info); }));
    g_listeners.push_back(bus.gesture.pinch.end.listen([](IPointer::SPinchEndEvent e, Event::SCallbackInfo& info) { onPinchEnd(e, info); }));
}

void HyprVXPolicy::shutdownGesture() {
    g_listeners.clear();
    g_tracker.reset();
    g_scroll  = {};
    g_gesture = {};
    g_panFactor.reset();
    g_glide.reset();
    g_glideMinSpeed.reset();
}
