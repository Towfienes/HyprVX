#pragma once

#include "hyprvx/hyprland/EventParser.hpp"
#include "hyprvx/hyprland/RuntimeMirror.hpp"
#include "hyprvx/hyprland/SemanticEventAdapter.hpp"

#include <optional>
#include <cstdint>
#include <string_view>

namespace hyprvx::hyprland {

class RuntimeBridge {
public:
    [[nodiscard]] std::optional<SemanticEvent> processLine(std::string_view line);
    void beginEpoch();
    void endEpoch();
    void reset() { beginEpoch(); }
    [[nodiscard]] std::uint64_t connectionEpoch() const noexcept { return connectionEpoch_; }
    [[nodiscard]] bool epochActive() const noexcept { return epochActive_; }
    [[nodiscard]] RuntimeMirror& mirror() { return mirror_; }
    [[nodiscard]] const RuntimeMirror& mirror() const { return mirror_; }

private:
    SemanticEventAdapter adapter_;
    RuntimeMirror mirror_;
    std::uint64_t connectionEpoch_{1};
    bool epochActive_{true};
};

} // namespace hyprvx::hyprland
