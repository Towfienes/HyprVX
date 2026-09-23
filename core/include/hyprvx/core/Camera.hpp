#pragma once

#include "Rect.hpp"

namespace hyprvx {

class Camera {
public:
    Camera() = default;

    explicit Camera(
        Vec2 position,
        double zoom = 1.0
    );

    [[nodiscard]]
    const Vec2& position() const noexcept {
        return position_;
    }

    [[nodiscard]]
    double zoom() const noexcept {
        return zoom_;
    }

    void setPosition(Vec2 position) noexcept {
        position_ = position;
    }

    void moveBy(Vec2 delta) noexcept {
        position_ += delta;
    }

    void setZoom(double zoom);

    void setZoomAroundScreenPoint(
        double zoom,
        Vec2 screenAnchor
    );

    [[nodiscard]]
    Vec2 worldToScreen(Vec2 worldPoint) const noexcept;

    [[nodiscard]]
    Vec2 screenToWorld(Vec2 screenPoint) const noexcept;

    [[nodiscard]]
    Rect visibleWorldRect(Vec2 viewportPixels) const noexcept;

private:
    Vec2 position_{};
    double zoom_{1.0};
};

}
