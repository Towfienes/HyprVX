// HyprVX test-only plugin `hyprvx-testinput` (Feature 003 T010, contracts/test-tooling.md 1.5 §8). Nested-only: the
// harness loads it with `scripts/nested.sh plugin load <path>` into a verified nested session after the policy plugin;
// it is never part of a delivery form and never loaded into the host compositor.
//
// hyprctl `hvxtest` (the word does not begin with "hyprvx", so Hyprland's prefix matching never routes it to the
// policy's `hyprvx` command):
//   hvxtest gesture swipe begin <fingers> <timeMs>
//   hvxtest gesture swipe update <fingers> <dx> <dy> <timeMs>
//   hvxtest gesture swipe end <cancelled 0|1> <timeMs>
//   hvxtest gesture pinch begin <fingers> <timeMs>
//   hvxtest gesture pinch update <fingers> <dx> <dy> <scale> <rotation> <timeMs>
//   hvxtest gesture pinch end <cancelled 0|1> <timeMs>
//   hvxtest frames <monitor>          render.pre count of that monitor since load
// Gesture commands call the public CInputManager::onSwipe*/onPinch* with exactly the given fields, so the event takes
// the compositor's own path (event bus, `gesture =` bindings, pointer-gestures protocol). Replies: "ok", the count, or
// "err:<why>"; they are never pass conditions (frame counts are a measurement only).

#include <src/devices/IPointer.hpp>
#include <src/event/EventBus.hpp>
#include <src/managers/input/InputManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/plugins/PluginAPI.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
    HANDLE                           g_handle = nullptr;
    SP<SHyprCtlCommand>              g_command;
    CHyprSignalListener              g_renderPre;
    std::map<std::string, uint64_t> g_frames;

    // Whole-token parsers: the token must be consumed completely and be in range.
    bool parseU32(const std::string& token, uint32_t& out) {
        if (token.empty() || token.find_first_not_of("0123456789") != std::string::npos || token.size() > 10)
            return false;
        const auto VALUE = std::stoull(token);
        if (VALUE > std::numeric_limits<uint32_t>::max())
            return false;
        out = static_cast<uint32_t>(VALUE);
        return true;
    }

    bool parseDouble(const std::string& token, double limit, double& out) {
        size_t used = 0;
        try {
            out = std::stod(token, &used);
        } catch (...) { return false; }
        return used == token.size() && std::isfinite(out) && std::fabs(out) <= limit;
    }

    bool parseFingers(const std::string& token, uint32_t& out) {
        return parseU32(token, out) && out >= 1 && out <= 5;
    }

    bool parseCancelled(const std::string& token, bool& out) {
        if (token != "0" && token != "1")
            return false;
        out = token == "1";
        return true;
    }

    std::string gesture(const std::vector<std::string>& w) {
        // w = {"gesture", kind, phase, args...}
        if (w.size() < 3)
            return "err:usage";
        const auto& KIND  = w[1];
        const auto& PHASE = w[2];
        const auto  ARGS  = std::vector<std::string>(w.begin() + 3, w.end());
        if (KIND != "swipe" && KIND != "pinch")
            return "err:kind";

        if (PHASE == "begin") {
            uint32_t fingers = 0, timeMs = 0;
            if (ARGS.size() != 2 || !parseFingers(ARGS[0], fingers) || !parseU32(ARGS[1], timeMs))
                return "err:args";
            if (KIND == "swipe")
                g_pInputManager->onSwipeBegin(IPointer::SSwipeBeginEvent{.timeMs = timeMs, .fingers = fingers});
            else
                g_pInputManager->onPinchBegin(IPointer::SPinchBeginEvent{.timeMs = timeMs, .fingers = fingers});
            return "ok";
        }
        if (PHASE == "end") {
            bool     cancelled = false;
            uint32_t timeMs    = 0;
            if (ARGS.size() != 2 || !parseCancelled(ARGS[0], cancelled) || !parseU32(ARGS[1], timeMs))
                return "err:args";
            if (KIND == "swipe")
                g_pInputManager->onSwipeEnd(IPointer::SSwipeEndEvent{.timeMs = timeMs, .cancelled = cancelled});
            else
                g_pInputManager->onPinchEnd(IPointer::SPinchEndEvent{.timeMs = timeMs, .cancelled = cancelled});
            return "ok";
        }
        if (PHASE == "update") {
            uint32_t fingers = 0, timeMs = 0;
            double   dx = 0, dy = 0;
            if (KIND == "swipe") {
                if (ARGS.size() != 4 || !parseFingers(ARGS[0], fingers) || !parseDouble(ARGS[1], 2000, dx) || !parseDouble(ARGS[2], 2000, dy) ||
                    !parseU32(ARGS[3], timeMs))
                    return "err:args";
                g_pInputManager->onSwipeUpdate(IPointer::SSwipeUpdateEvent{.timeMs = timeMs, .fingers = fingers, .delta = {dx, dy}});
                return "ok";
            }
            double scale = 0, rotation = 0;
            if (ARGS.size() != 6 || !parseFingers(ARGS[0], fingers) || !parseDouble(ARGS[1], 2000, dx) || !parseDouble(ARGS[2], 2000, dy) ||
                !parseDouble(ARGS[3], 100, scale) || scale <= 0 || !parseDouble(ARGS[4], 360, rotation) || !parseU32(ARGS[5], timeMs))
                return "err:args";
            g_pInputManager->onPinchUpdate(
                IPointer::SPinchUpdateEvent{.timeMs = timeMs, .fingers = fingers, .delta = {dx, dy}, .scale = scale, .rotation = rotation});
            return "ok";
        }
        return "err:phase";
    }

    std::string hyprctlCommand(eHyprCtlOutputFormat, std::string in) {
        std::istringstream       stream(in);
        std::vector<std::string> words;
        for (std::string word; stream >> word;)
            words.push_back(word);
        // words[0] is the command word; Hyprland matched it by prefix, so "hvxtestX" also arrives here.
        if (words.empty() || words[0] != "hvxtest")
            return "";
        if (words.size() >= 2 && words[1] == "gesture")
            return gesture(std::vector<std::string>(words.begin() + 1, words.end()));
        if (words.size() == 3 && words[1] == "frames") {
            const auto IT = g_frames.find(words[2]);
            return std::to_string(IT == g_frames.end() ? 0 : IT->second);
        }
        return "err:usage";
    }
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_handle    = handle;
    g_command   = HyprlandAPI::registerHyprCtlCommand(handle, SHyprCtlCommand{.name = "hvxtest", .exact = false, .fn = hyprctlCommand});
    g_renderPre = Event::bus()->m_events.render.pre.listen([](PHLMONITOR monitor) {
        if (monitor)
            ++g_frames[monitor->m_name];
    });
    return {"hyprvx-testinput", "HyprVX test-only input (nested tests; never shipped)", "HyprVX", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_renderPre.reset();
    if (g_command)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_command);
    g_command.reset();
    g_frames.clear();
}
