#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "hyprvx/core/SpatialNavigation.hpp"

namespace hyprvx {

namespace {

struct SpatialNode {
    WindowId windowId{};
    Rect geometry{};
    std::optional<IslandId> islandId{};
};

std::vector<SpatialNode>
collectNodes(
    const World& world
) {
    std::vector<SpatialNode> nodes;

    nodes.reserve(
        world.freeWindowCount() + 16
    );

    for (
        const auto& [id, window] :
        world.freeWindows()
    ) {
        nodes.push_back({
            id,
            window.geometry,
            std::nullopt
        });
    }

    for (
        const auto& [islandId, island] :
        world.islands()
    ) {
        for (
            const auto& member :
            island.members()
        ) {
            const auto geometry =
                island.memberWorldGeometry(
                    member.id
                );

            if (!geometry.has_value()) {
                continue;
            }

            nodes.push_back({
                member.id,
                *geometry,
                islandId
            });
        }
    }

    return nodes;
}

Vec2 directionVector(
    SpatialDirection direction
) {
    switch (direction) {
        case SpatialDirection::Left:
            return {-1.0, 0.0};

        case SpatialDirection::Right:
            return {1.0, 0.0};

        case SpatialDirection::Up:
            return {0.0, -1.0};

        case SpatialDirection::Down:
            return {0.0, 1.0};
    }

    return {};
}

double intervalOverlap(
    double a0,
    double a1,
    double b0,
    double b1
) {
    return std::max(
        0.0,
        std::min(a1, b1) -
        std::max(a0, b0)
    );
}

double overlapRatio(
    const Rect& source,
    const Rect& candidate,
    SpatialDirection direction
) {
    double overlap{};
    double denominator{};

    if (
        direction == SpatialDirection::Left ||
        direction == SpatialDirection::Right
    ) {
        overlap =
            intervalOverlap(
                source.top(),
                source.bottom(),
                candidate.top(),
                candidate.bottom()
            );

        denominator =
            std::min(
                source.size.y,
                candidate.size.y
            );
    } else {
        overlap =
            intervalOverlap(
                source.left(),
                source.right(),
                candidate.left(),
                candidate.right()
            );

        denominator =
            std::min(
                source.size.x,
                candidate.size.x
            );
    }

    if (!(denominator > 0.0)) {
        return 0.0;
    }

    return std::clamp(
        overlap / denominator,
        0.0,
        1.0
    );
}

}

SpatialNavigator::SpatialNavigator(
    SpatialNavigationPolicy policy
)
    : policy_(policy) {

    const auto finite =
        [](double value) {
            return std::isfinite(value);
        };

    if (
        !finite(policy_.maxAngleDegrees) ||
        !(policy_.maxAngleDegrees > 0.0) ||
        policy_.maxAngleDegrees >= 90.0 ||

        !finite(policy_.angularPenalty) ||
        policy_.angularPenalty < 0.0 ||

        !finite(policy_.lateralPenalty) ||
        policy_.lateralPenalty < 0.0 ||

        !finite(policy_.overlapBonus) ||
        policy_.overlapBonus < 0.0 ||

        !finite(policy_.sameIslandBonus) ||
        policy_.sameIslandBonus < 0.0
    ) {
        throw std::invalid_argument(
            "invalid spatial navigation policy"
        );
    }
}

std::optional<SpatialNavigationResult>
SpatialNavigator::findNeighbor(
    const World& world,
    WindowId sourceWindowId,
    SpatialDirection direction
) const {
    const std::vector<SpatialNode> nodes =
        collectNodes(world);

    const auto sourceIt =
        std::find_if(
            nodes.begin(),
            nodes.end(),
            [sourceWindowId](
                const SpatialNode& node
            ) {
                return
                    node.windowId ==
                    sourceWindowId;
            }
        );

    if (sourceIt == nodes.end()) {
        return std::nullopt;
    }

    const SpatialNode& source =
        *sourceIt;

    const Vec2 sourceCenter =
        source.geometry.center();

    const Vec2 axis =
        directionVector(direction);

    const Vec2 perpendicular{
        -axis.y,
        axis.x
    };

    const double maxAngleRadians =
        policy_.maxAngleDegrees *
        std::numbers::pi /
        180.0;

    std::optional<SpatialNavigationResult>
        best{};

    constexpr double tieEpsilon =
        1e-9;

    for (
        const SpatialNode& candidate :
        nodes
    ) {
        if (
            candidate.windowId ==
            source.windowId
        ) {
            continue;
        }

        const Vec2 delta =
            candidate.geometry.center() -
            sourceCenter;

        const double forward =
            delta.x * axis.x +
            delta.y * axis.y;

        if (!(forward > 0.0)) {
            continue;
        }

        const double lateral =
            std::abs(
                delta.x * perpendicular.x +
                delta.y * perpendicular.y
            );

        const double angle =
            std::atan2(
                lateral,
                forward
            );

        if (
            angle >
            maxAngleRadians
        ) {
            continue;
        }

        const double distance =
            delta.length();

        const double overlap =
            overlapRatio(
                source.geometry,
                candidate.geometry,
                direction
            );

        const double normalizedAngle =
            angle /
            maxAngleRadians;

        double score =
            distance +
            policy_.angularPenalty *
            normalizedAngle *
            normalizedAngle +
            policy_.lateralPenalty *
            lateral -
            policy_.overlapBonus *
            overlap;

        if (
            source.islandId.has_value() &&
            candidate.islandId.has_value() &&
            source.islandId ==
                candidate.islandId
        ) {
            score -=
                policy_.sameIslandBonus;
        }

        SpatialNavigationResult result{
            candidate.windowId,
            candidate.islandId,
            score,
            distance,
            angle,
            overlap
        };

        const bool better =
            !best.has_value() ||
            result.score <
                best->score -
                tieEpsilon ||

            (
                std::abs(
                    result.score -
                    best->score
                ) <= tieEpsilon &&
                result.windowId <
                    best->windowId
            );

        if (better) {
            best = result;
        }
    }

    return best;
}

}
