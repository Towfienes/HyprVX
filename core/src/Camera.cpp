#include <cmath>
#include <stdexcept>

#include "hyprvx/core/Camera.hpp"

namespace hyprvx {

Camera::Camera(
    Vec2 position,
    double zoom
)
    : position_(position) {

    setZoom(zoom);
}

void Camera::setZoom(double zoom) {
    if (!(zoom > 0.0) || !std::isfinite(zoom)) {
        throw std::invalid_argument(
            "camera zoom must be finite and positive"
        );
    }

    zoom_ = zoom;
}

void Camera::setZoomAroundScreenPoint(
    double zoom,
    Vec2 screenAnchor
) {
    const Vec2 worldAnchor =
        screenToWorld(screenAnchor);

    setZoom(zoom);

    position_ =
        worldAnchor -
        (screenAnchor / zoom_);
}

Vec2 Camera::worldToScreen(
    Vec2 worldPoint
) const noexcept {

    return
        (worldPoint - position_) *
        zoom_;
}

Vec2 Camera::screenToWorld(
    Vec2 screenPoint
) const noexcept {

    return
        position_ +
        (screenPoint / zoom_);
}

Rect Camera::visibleWorldRect(
    Vec2 viewportPixels
) const noexcept {

    return {
        position_,
        viewportPixels / zoom_
    };
}

}
