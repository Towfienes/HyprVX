#include <cmath>
#include <limits>
#include <stdexcept>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMath.hpp"

namespace hyprvx {

namespace {

// V1 camera space is viewport-local: the monitor origin M is {0, 0}.
constexpr cammath::Vec kViewportOrigin{};

// V1 accepts any finite positive zoom; setZoom validates, the anchor math must not clamp.
constexpr cammath::ZoomLimits kUnboundedZoom{
    std::numeric_limits<double>::min(),
    std::numeric_limits<double>::max()
};

constexpr cammath::Vec toMath(Vec2 value) noexcept {
    return {value.x, value.y};
}

constexpr Vec2 fromMath(cammath::Vec value) noexcept {
    return {value.x, value.y};
}

}

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
    const cammath::CameraState before{
        toMath(position_),
        zoom_
    };

    setZoom(zoom);

    position_ = fromMath(
        cammath::anchorZoom(
            before,
            kViewportOrigin,
            toMath(screenAnchor),
            zoom_,
            kUnboundedZoom
        ).origin
    );
}

Vec2 Camera::worldToScreen(
    Vec2 worldPoint
) const noexcept {

    return fromMath(
        cammath::forward(
            {toMath(position_), zoom_},
            kViewportOrigin,
            toMath(worldPoint)
        )
    );
}

Vec2 Camera::screenToWorld(
    Vec2 screenPoint
) const noexcept {

    return fromMath(
        cammath::inverse(
            {toMath(position_), zoom_},
            kViewportOrigin,
            toMath(screenPoint)
        )
    );
}

Rect Camera::visibleWorldRect(
    Vec2 viewportPixels
) const noexcept {

    const cammath::Box visible =
        cammath::visibleWorldRect(
            {toMath(position_), zoom_},
            toMath(viewportPixels)
        );

    return {
        fromMath(visible.pos),
        fromMath(visible.size)
    };
}

}
