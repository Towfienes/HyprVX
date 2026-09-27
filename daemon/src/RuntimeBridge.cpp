#include "hyprvx/hyprland/RuntimeBridge.hpp"

#include <limits>
#include <stdexcept>

namespace hyprvx::hyprland {

std::optional<SemanticEvent> RuntimeBridge::processLine(std::string_view line) {
    if (!epochActive_) {
        return std::nullopt;
    }
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

void RuntimeBridge::beginEpoch() {
    if (connectionEpoch_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("runtime connection epoch exhausted");
    }
    mirror_.reset();
    ++connectionEpoch_;
    epochActive_ = true;
}

void RuntimeBridge::endEpoch() {
    if (!epochActive_) {
        return;
    }
    mirror_.reset();
    epochActive_ = false;
}

} // namespace hyprvx::hyprland
