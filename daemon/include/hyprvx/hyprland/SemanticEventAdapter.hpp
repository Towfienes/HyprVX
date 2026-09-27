#pragma once

#include "hyprvx/hyprland/EventParser.hpp"
#include "hyprvx/hyprland/SemanticEvent.hpp"

#include <optional>

namespace hyprvx::hyprland {

class SemanticEventAdapter {
public:
    // Legacy forms with verified v2 equivalents are intentionally ignored.
    [[nodiscard]] std::optional<SemanticEvent> adapt(const Event& event) const;
};

} // namespace hyprvx::hyprland
