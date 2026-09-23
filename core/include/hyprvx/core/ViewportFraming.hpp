#pragma once

#include "Rect.hpp"

namespace hyprvx {

struct ViewportFramingPolicy {
    double marginPixels{90.0};

    double minimumZoom{0.20};
    double maximumZoom{3.00};

    bool preserveCurrentZoom{true};
};

struct FramedView {
    Vec2 cameraPosition{};
    double zoom{1.0};
};

[[nodiscard]]
FramedView frameRectInViewport(
    const Rect& worldRect,
    Vec2 viewportPixels,
    double currentZoom,
    ViewportFramingPolicy policy = {}
);

}
