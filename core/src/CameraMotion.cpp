#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "hyprvx/core/CameraMotion.hpp"

namespace hyprvx {

namespace {

constexpr double epsilon = 1e-12;

Vec2 clampLength(
    Vec2 value,
    double maximum
) {
    const double lengthSquared =
        value.lengthSquared();

    const double maximumSquared =
        maximum * maximum;

    if (lengthSquared <= maximumSquared) {
        return value;
    }

    const double length =
        std::sqrt(lengthSquared);

    if (length <= epsilon) {
        return {};
    }

    return value * (maximum / length);
}

bool nearlyZero(
    Vec2 value,
    double threshold
) {
    return
        value.lengthSquared() <=
        threshold * threshold;
}

}

CameraController::CameraController(
    Camera& camera,
    CameraMotionConfig config
)
    : camera_(camera),
      config_(config) {

    validateConfig();

    if (
        camera_.zoom() < config_.minZoom ||
        camera_.zoom() > config_.maxZoom
    ) {
        camera_.setZoom(
            std::clamp(
                camera_.zoom(),
                config_.minZoom,
                config_.maxZoom
            )
        );
    }
}

void CameraController::validateConfig() const {
    const auto finite =
        [](double value) {
            return std::isfinite(value);
        };

    if (
        !finite(config_.acceleration) ||
        config_.acceleration < 0.0
    ) {
        throw std::invalid_argument(
            "camera acceleration must be finite and non-negative"
        );
    }

    if (
        !finite(config_.damping) ||
        config_.damping < 0.0
    ) {
        throw std::invalid_argument(
            "camera damping must be finite and non-negative"
        );
    }

    if (
        !finite(config_.maxSpeed) ||
        !(config_.maxSpeed > 0.0)
    ) {
        throw std::invalid_argument(
            "camera maximum speed must be finite and positive"
        );
    }

    if (
        !finite(config_.flightResponse) ||
        !(config_.flightResponse > 0.0)
    ) {
        throw std::invalid_argument(
            "camera flight response must be finite and positive"
        );
    }

    if (
        !finite(config_.flightEpsilon) ||
        !(config_.flightEpsilon > 0.0)
    ) {
        throw std::invalid_argument(
            "camera flight epsilon must be finite and positive"
        );
    }

    if (
        !finite(config_.stopSpeed) ||
        config_.stopSpeed < 0.0
    ) {
        throw std::invalid_argument(
            "camera stop speed must be finite and non-negative"
        );
    }

    if (
        !finite(config_.minZoom) ||
        !finite(config_.maxZoom) ||
        !(config_.minZoom > 0.0) ||
        config_.maxZoom < config_.minZoom
    ) {
        throw std::invalid_argument(
            "invalid camera zoom limits"
        );
    }
}

void CameraController::setPanInput(
    Vec2 direction
) noexcept {
    panInput_ =
        clampLength(direction, 1.0);

    if (!nearlyZero(panInput_, epsilon)) {
        cancelFlight();
    }
}

void CameraController::setVelocity(
    Vec2 velocity
) noexcept {
    velocity_ =
        clampLength(
            velocity,
            config_.maxSpeed
        );
}

void CameraController::stop() noexcept {
    velocity_ = {};
    panInput_ = {};
    flightTarget_.reset();
}

void CameraController::flyTo(
    Vec2 target
) {
    flightTarget_ = target;
    panInput_ = {};
}

void CameraController::cancelFlight() noexcept {
    flightTarget_.reset();
}

void CameraController::setZoomLimits(
    double minimum,
    double maximum
) {
    if (
        !std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        !(minimum > 0.0) ||
        maximum < minimum
    ) {
        throw std::invalid_argument(
            "invalid camera zoom limits"
        );
    }

    config_.minZoom = minimum;
    config_.maxZoom = maximum;

    const double clamped =
        std::clamp(
            camera_.zoom(),
            minimum,
            maximum
        );

    camera_.setZoom(clamped);
}

void CameraController::zoomByFactorAroundScreenPoint(
    double factor,
    Vec2 screenAnchor
) {
    if (
        !std::isfinite(factor) ||
        !(factor > 0.0)
    ) {
        throw std::invalid_argument(
            "zoom factor must be finite and positive"
        );
    }

    const double targetZoom =
        std::clamp(
            camera_.zoom() * factor,
            config_.minZoom,
            config_.maxZoom
        );

    camera_.setZoomAroundScreenPoint(
        targetZoom,
        screenAnchor
    );
}

void CameraController::update(
    double deltaSeconds
) {
    if (
        !std::isfinite(deltaSeconds) ||
        !(deltaSeconds > 0.0)
    ) {
        return;
    }

    if (flightTarget_.has_value()) {
        updateFlight(deltaSeconds);
        return;
    }

    updateManualMotion(deltaSeconds);
}

void CameraController::updateManualMotion(
    double deltaSeconds
) {
    const Vec2 acceleration =
        panInput_ * config_.acceleration;

    const Vec2 oldVelocity =
        velocity_;

    Vec2 displacement{};

    if (config_.damping > epsilon) {
        const double lambda =
            config_.damping;

        const double decay =
            std::exp(
                -lambda * deltaSeconds
            );

        const double oneMinusDecay =
            1.0 - decay;

        velocity_ =
            oldVelocity * decay +
            acceleration *
            (oneMinusDecay / lambda);

        displacement =
            oldVelocity *
            (oneMinusDecay / lambda) +
            acceleration *
            (
                deltaSeconds / lambda -
                oneMinusDecay /
                (lambda * lambda)
            );
    } else {
        velocity_ =
            oldVelocity +
            acceleration * deltaSeconds;

        displacement =
            oldVelocity * deltaSeconds +
            acceleration *
            (0.5 * deltaSeconds * deltaSeconds);
    }

    velocity_ =
        clampLength(
            velocity_,
            config_.maxSpeed
        );

    camera_.moveBy(displacement);

    if (
        nearlyZero(panInput_, epsilon) &&
        nearlyZero(
            velocity_,
            config_.stopSpeed
        )
    ) {
        velocity_ = {};
    }
}

void CameraController::updateFlight(
    double deltaSeconds
) {
    const Vec2 target =
        *flightTarget_;

    const Vec2 before =
        camera_.position();

    const Vec2 difference =
        before - target;

    if (
        nearlyZero(
            difference,
            config_.flightEpsilon
        )
    ) {
        camera_.setPosition(target);
        velocity_ = {};
        flightTarget_.reset();
        return;
    }

    const double decay =
        std::exp(
            -config_.flightResponse *
            deltaSeconds
        );

    const Vec2 after =
        target +
        difference * decay;

    camera_.setPosition(after);

    velocity_ =
        (after - before) /
        deltaSeconds;

    if (
        nearlyZero(
            after - target,
            config_.flightEpsilon
        )
    ) {
        camera_.setPosition(target);
        velocity_ = {};
        flightTarget_.reset();
    }
}

}
