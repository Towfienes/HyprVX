#pragma once

#include "hyprvx/hyprland/EventParser.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"
#include "hyprvx/hyprland/SemanticEventAdapter.hpp"

#include <optional>
#include <string_view>

namespace hyprvx::hyprland {

class RuntimeBridge {
public:
    [[nodiscard]] std::optional<SemanticEvent> processLine(std::string_view line);
    void reset() { mirror_.reset(); }
    [[nodiscard]] RuntimeMirror& mirror() { return mirror_; }
    [[nodiscard]] const RuntimeMirror& mirror() const { return mirror_; }

private:
    SemanticEventAdapter adapter_;
    RuntimeMirror mirror_;
};

} // namespace hyprvx::hyprland
