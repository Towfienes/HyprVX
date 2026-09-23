#pragma once

#include <optional>

#include "World.hpp"

namespace hyprvx {

enum class SpatialDirection {
    Left,
    Right,
    Up,
    Down
};

struct SpatialNavigationPolicy {
    double maxAngleDegrees{65.0};

    double angularPenalty{900.0};
    double lateralPenalty{0.20};

    double overlapBonus{350.0};
    double sameIslandBonus{120.0};
};

struct SpatialNavigationResult {
    WindowId windowId{};

    std::optional<IslandId> islandId{};

    double score{};
    double distance{};
    double angleRadians{};
    double overlapRatio{};
};

class SpatialNavigator {
public:
    explicit SpatialNavigator(
        SpatialNavigationPolicy policy = {}
    );

    [[nodiscard]]
    const SpatialNavigationPolicy&
    policy() const noexcept {
        return policy_;
    }

    [[nodiscard]]
    std::optional<SpatialNavigationResult>
    findNeighbor(
        const World& world,
        WindowId sourceWindowId,
        SpatialDirection direction
    ) const;

private:
    SpatialNavigationPolicy policy_{};
};

}
