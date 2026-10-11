// `hyprctl hyprvx-dev <command>` for the delivery spike: the dev-control contract (contracts/dev-control.md, transport
// A) of patch 0005 and its 1.2 additions of patch 0013 (offset, offsets in state, panic resets offsets; Feature 002
// test-tooling 1.4 §dev), registered through the plugin API so that the native tests run unchanged. Compiled only into the
// form S test build (HYPRVX_PLUGIN_DEV=ON -> HYPRVX_DEV); a delivered build has no dev control (FR-016).

#ifdef HYPRVX_DEV

#include "Hooks.hpp"
#include "Camera.hpp"

#include <src/debug/HyprCtl.hpp>
#include <src/debug/log/Logger.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/Window.hpp>
#include <src/helpers/MiscFunctions.hpp>
#include <src/output/Monitor.hpp>
#include <src/render/Renderer.hpp>
#include <src/state/MonitorQuery.hpp>
#include <src/state/MonitorState.hpp>

#include <charconv>

using namespace hyprvx;

namespace {
    std::vector<std::string_view> splitWords(std::string_view args) {
        std::vector<std::string_view> words;
        size_t                        pos = 0;
        while (pos < args.size()) {
            const auto START = args.find_first_not_of(" \t", pos);
            if (START == std::string_view::npos)
                break;
            const auto END = std::min(args.find_first_of(" \t", START), args.size());
            words.emplace_back(args.substr(START, END - START));
            pos = END;
        }
        return words;
    }

    std::optional<double> parseNumber(std::string_view word) {
        double     value = 0.0;
        const auto RES   = std::from_chars(word.data(), word.data() + word.size(), value);
        if (RES.ec != std::errc{} || RES.ptr != word.data() + word.size())
            return std::nullopt;
        return value;
    }

    std::string setFromWords(const PHLMONITOR& monitor, const std::vector<std::string_view>& words) {
        if (!monitor)
            return "err:monitor";
        if (words.size() != 3)
            return "err:invalid";

        const auto PX = parseNumber(words[0]);
        const auto PY = parseNumber(words[1]);
        const auto Z  = parseNumber(words[2]);
        if (!PX || !PY || !Z || !HyprVXSpike::setCamera(*monitor, {{*PX, *PY}, *Z}))
            return "err:invalid";
        return "ok";
    }

    // <address> <x> <y> (dev control 1.2): sets O(w) directly; validation is setWindowOffset's.
    std::string setOffset(const std::vector<std::string_view>& words) {
        if (words.size() != 3)
            return "err:invalid";
        auto addressWord = words[0];
        if (addressWord.starts_with("0x"))
            addressWord.remove_prefix(2);
        uintptr_t  address = 0;
        const auto RES     = std::from_chars(addressWord.data(), addressWord.data() + addressWord.size(), address, 16);
        const auto X       = parseNumber(words[1]);
        const auto Y       = parseNumber(words[2]);
        if (RES.ec != std::errc{} || RES.ptr != addressWord.data() + addressWord.size() || !X || !Y)
            return "err:invalid";
        for (auto const& w : Desktop::windowState()->windows()) {
            if (rc<uintptr_t>(w.get()) == address)
                return HyprVXSpike::setWindowOffset(w, {*X, *Y}) ? "ok" : "err:invalid";
        }
        return "err:window";
    }

    std::string offsetsJSON() {
        std::string out = "[";
        for (auto const& [window, offset] : HyprVXSpike::windowOffsets()) {
            if (out.size() > 1)
                out += ",";
            out += std::format(R"({{"address":"0x{:x}","offset":[{:.17g},{:.17g}]}})", rc<uintptr_t>(window.get()), offset.x, offset.y);
        }
        return out + "]";
    }

    std::string stateJSON() {
        const auto  OFFSETS = offsetsJSON();
        std::string out     = "[";
        for (auto const& m : State::monitorState()->monitors()) {
            const auto  CAMERA = HyprVXSpike::cameraOf(*m);
            const auto& STORED = HyprVXSpike::stored(*m);
            if (out.size() > 1)
                out += ",";
            out += std::format(R"({{"monitor":"{}","P":[{:.17g},{:.17g}],"z":{:.17g},"identity":{},"revision":{},"passElementsAdded":{},"offsets":{}}})",
                               escapeJSONStrings(m->m_name), CAMERA.origin.x, CAMERA.origin.y, CAMERA.zoom, HyprVXSpike::cameraIsIdentity(*m), STORED.revision,
                               STORED.passElementsAdded, OFFSETS);
        }
        return out + "]";
    }

    std::string hyprctlCommand(eHyprCtlOutputFormat, std::string in) {
        auto WORDS = splitWords(in);
        if (WORDS.size() < 2)
            return "err:invalid";

        const auto NAME = WORDS[1];
        WORDS.erase(WORDS.begin(), WORDS.begin() + 2);

        if (NAME == "state") {
            Log::logger->log(Log::DEBUG, "[hyprvx] state: {}", stateJSON());
            return stateJSON();
        }
        if (NAME == "camera")
            return setFromWords(Desktop::focusState()->monitor(), WORDS);
        if (NAME == "camera-mon") {
            if (WORDS.empty())
                return "err:monitor";
            const auto MONITOR = State::monitorState()->query().name(WORDS.front()).run();
            if (!MONITOR)
                return "err:monitor";
            WORDS.erase(WORDS.begin());
            return setFromWords(MONITOR, WORDS);
        }
        if (NAME == "panic") {
            for (auto const& m : State::monitorState()->monitors())
                HyprVXSpike::resetCamera(*m);
            HyprVXSpike::resetWindowOffsets();
            Log::logger->log(Log::WARN, "[hyprvx] panic: all cameras reset to identity, all window offsets to 0");
            return "ok";
        }
        if (NAME == "offset")
            return setOffset(WORDS);
        if (NAME == "full-damage") {
            for (auto const& m : State::monitorState()->monitors())
                g_pHyprRenderer->damageMonitor(m);
            return "ok";
        }
        return "err:invalid";
    }
}

void HyprVXSpike::registerDevControl() {
    HyprlandAPI::registerHyprCtlCommand(PHANDLE, SHyprCtlCommand{.name = "hyprvx-dev", .exact = false, .fn = hyprctlCommand});
    Log::logger->log(Log::WARN, "[hyprvx] dev camera control enabled (delivery spike plugin)");
}

#endif // HYPRVX_DEV
