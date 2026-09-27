#include "hyprvx/platform/NativeEventBridge.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>

namespace hyprvx::platform {
namespace {
std::optional<int> numericWorkspace(const std::string& text) {
    int result{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return result;
}
} // namespace

std::optional<NativeObservation> parseNativeObservation(std::string_view datagram) {
    try {
        const auto data = nlohmann::json::parse(datagram);
        if (!data.is_object()) return std::nullopt;
        const auto kind = data.at("kind").get<std::string>();
        NativeObservation observation;
        if (kind == "camera_reset") { observation.cameraReset = true; return observation; }
        if (kind == "camera_pan") {
            const auto dx = data.at("dx").get<double>();
            const auto dy = data.at("dy").get<double>();
            if (!std::isfinite(dx) || !std::isfinite(dy)) return std::nullopt;
            observation.cameraPan = std::array{dx, dy};
            return observation;
        }
        if (kind == "workspace_active") {
            const auto name = data.at("workspace").get<std::string>();
            if (const auto id = numericWorkspace(name)) observation.semantic = hyprland::WorkspaceFocused{*id, name};
            return observation;
        }
        const auto address = hyprland::canonicalWindowAddress(data.at("address").get<std::string>());
        if (kind == "focus" && address.empty()) {
            observation.semantic = hyprland::WindowFocusCleared{};
            return observation;
        }
        if (address.empty() || !std::all_of(address.begin(), address.end(), [](unsigned char digit) {
                return std::isxdigit(digit) != 0;
            })) return std::nullopt;
        if (kind == "focus") observation.semantic = hyprland::WindowFocused{address};
        else if (kind == "window_close") observation.semantic = hyprland::WindowClosed{address};
        else if (kind == "window_title") observation.semantic = hyprland::WindowTitleChanged{address, data.at("title").get<std::string>()};
        else if (kind == "window_floating") observation.semantic = hyprland::WindowFloatingChanged{address, data.at("floating").get<bool>()};
        else if (kind == "window_move") {
            const auto name = data.at("workspace").get<std::string>();
            if (const auto id = numericWorkspace(name)) observation.semantic = hyprland::WindowWorkspaceChanged{address, *id, name};
        } else if (kind == "window_open") {
            const auto name = data.at("workspace").get<std::string>();
            observation.semantic = hyprland::WindowOpened{address, data.at("class").get<std::string>(),
                data.at("title").get<std::string>(), name, numericWorkspace(name)};
        } else return std::nullopt;
        if (data.contains("actual_geometry")) {
            const auto& geometry = data.at("actual_geometry");
            if (geometry.is_array() && geometry.size() == 4) {
                ActualCompositorGeometry actual{address, {geometry[0].get<double>(), geometry[1].get<double>(),
                    geometry[2].get<double>(), geometry[3].get<double>()}};
                if (std::all_of(actual.xywh.begin(), actual.xywh.end(), [](double value) { return std::isfinite(value); }) &&
                    actual.xywh[2] > 0 && actual.xywh[3] > 0) observation.actualGeometry = actual;
            }
        }
        return observation;
    } catch (...) { return std::nullopt; }
}
} // namespace hyprvx::platform
