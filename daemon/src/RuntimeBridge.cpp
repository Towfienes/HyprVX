#include "hyprvx/hyprland/RuntimeBridge.hpp"

namespace hyprvx::hyprland {

std::optional<SemanticEvent> RuntimeBridge::processLine(std::string_view line) {
    const auto event = EventParser::parse(line);
    if (!event) {
        return std::nullopt;
    }
    auto semantic = adapter_.adapt(*event);
    if (semantic && !mirror_.apply(*semantic)) {
        return std::nullopt;
    }
    return semantic;
}

} // namespace hyprvx::hyprland
