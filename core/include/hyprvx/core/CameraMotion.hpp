#pragma once

#include <optional>

#include "Camera.hpp"

namespace hyprvx {

struct CameraMotionConfig {
    double acceleration{2800.0};
    double damping{7.5};
    double maxSpeed{6000.0};

    double flightResponse{7.0};
    double flightEpsilon{0.01};

    double stopSpeed{0.01};

    double minZoom{0.10};
    double maxZoom{4.00};
};

class CameraController {
public:
    explicit CameraController(
        Camera& camera,
        CameraMotionConfig config = {}
    );

    void setPanInput(Vec2 direction) noexcept;

    [[nodiscard]]
    const Vec2& panInput() const noexcept {
        return panInput_;
    }

    void setVelocity(Vec2 velocity) noexcept;

    [[nodiscard]]
    const Vec2& velocity() const noexcept {
        return velocity_;
    }

    void stop() noexcept;

    void flyTo(Vec2 target);

    void cancelFlight() noexcept;

    [[nodiscard]]
    bool isFlying() const noexcept {
        return flightTarget_.has_value();
    }

    [[nodiscard]]
    const std::optional<Vec2>& flightTarget() const noexcept {
        return flightTarget_;
    }

    void setZoomLimits(
        double minimum,
        double maximum
    );

    void zoomByFactorAroundScreenPoint(
        double factor,
        Vec2 screenAnchor
    );

    void update(double deltaSeconds);

    [[nodiscard]]
    const CameraMotionConfig& config() const noexcept {
        return config_;
    }

private:
    void validateConfig() const;
    void updateManualMotion(double deltaSeconds);
    void updateFlight(double deltaSeconds);

    Camera& camera_;
    CameraMotionConfig config_{};

    Vec2 panInput_{};
    Vec2 velocity_{};

    std::optional<Vec2> flightTarget_{};
};

}
