#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "hyprvx/core/ViewportFraming.hpp"

namespace hyprvx {

FramedView frameRectInViewport(
    const Rect& worldRect,
    Vec2 viewportPixels,
    double currentZoom,
    ViewportFramingPolicy policy
) {
    const auto finite =
        [](double value) {
            return std::isfinite(value);
        };

    if (
        !finite(viewportPixels.x) ||
        !finite(viewportPixels.y) ||
        !(viewportPixels.x > 0.0) ||
        !(viewportPixels.y > 0.0) ||
        !finite(currentZoom) ||
        !(currentZoom > 0.0) ||
        !finite(policy.marginPixels) ||
        policy.marginPixels < 0.0 ||
        !finite(policy.minimumZoom) ||
        !finite(policy.maximumZoom) ||
        !(policy.minimumZoom > 0.0) ||
        policy.maximumZoom <
            policy.minimumZoom
    ) {
        throw std::invalid_argument(
            "invalid viewport framing input"
        );
    }

    const double availableWidth =
        std::max(
            1.0,
            viewportPixels.x -
            policy.marginPixels * 2.0
        );

    const double availableHeight =
        std::max(
            1.0,
            viewportPixels.y -
            policy.marginPixels * 2.0
        );

    double fitZoom =
        policy.maximumZoom;

    if (worldRect.size.x > 0.0) {
        fitZoom =
            std::min(
                fitZoom,
                availableWidth /
                    worldRect.size.x
            );
    }

    if (worldRect.size.y > 0.0) {
        fitZoom =
            std::min(
                fitZoom,
                availableHeight /
                    worldRect.size.y
            );
    }

    fitZoom =
        std::clamp(
            fitZoom,
            policy.minimumZoom,
            policy.maximumZoom
        );

    double targetZoom =
        fitZoom;

    if (policy.preserveCurrentZoom) {
        targetZoom =
            std::min(
                currentZoom,
                fitZoom
            );
    }

    targetZoom =
        std::clamp(
            targetZoom,
            policy.minimumZoom,
            policy.maximumZoom
        );

    const Vec2 visibleWorldSize{
        viewportPixels.x /
            targetZoom,

        viewportPixels.y /
            targetZoom
    };

    const Vec2 targetPosition =
        worldRect.center() -
        visibleWorldSize / 2.0;

    return {
        targetPosition,
        targetZoom
    };
}

}
