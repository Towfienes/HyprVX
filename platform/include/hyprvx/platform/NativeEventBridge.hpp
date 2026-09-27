#pragma once

#include "hyprvx/hyprland/SemanticEvent.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace hyprvx::platform {

// Actual compositor geometry is observational data. It never becomes World
// placement, which remains owned by SpatialProjector.
struct ActualCompositorGeometry {
    std::string address;
    std::array<double, 4> xywh{};
};

struct NativeObservation {
    std::optional<hyprvx::hyprland::SemanticEvent> semantic;
    std::optional<ActualCompositorGeometry> actualGeometry;
    bool cameraReset{};
    std::optional<std::array<double, 2>> cameraPan;
};

[[nodiscard]] std::optional<NativeObservation> parseNativeObservation(std::string_view datagram);

} // namespace hyprvx::platform
