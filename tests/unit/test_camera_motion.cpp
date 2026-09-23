#include <cmath>

#include <gtest/gtest.h>

#include "hyprvx/core/CameraMotion.hpp"

using namespace hyprvx;

namespace {

Vec2 simulateManual(
    int refreshRate,
    double seconds
) {
    Camera camera;

    CameraMotionConfig config;
    config.acceleration = 1500.0;
    config.damping = 5.0;
    config.maxSpeed = 1000000.0;

    CameraController controller{
        camera,
        config
    };

    controller.setPanInput({
        1.0,
        0.35
    });

    const int frames =
        static_cast<int>(
            refreshRate * seconds
        );

    const double dt =
        1.0 /
        static_cast<double>(
            refreshRate
        );

    for (int i = 0; i < frames; ++i) {
        controller.update(dt);
    }

    return camera.position();
}

Vec2 simulateFlight(
    int refreshRate,
    double seconds
) {
    Camera camera{
        {-5000.0, 2000.0},
        1.0
    };

    CameraMotionConfig config;
    config.flightResponse = 4.5;
    config.flightEpsilon = 1e-12;

    CameraController controller{
        camera,
        config
    };

    controller.flyTo({
        12000.0,
        -8000.0
    });

    const int frames =
        static_cast<int>(
            refreshRate * seconds
        );

    const double dt =
        1.0 /
        static_cast<double>(
            refreshRate
        );

    for (int i = 0; i < frames; ++i) {
        controller.update(dt);
    }

    return camera.position();
}

}

TEST(CameraMotionTest, ManualMotionIsFrameRateIndependent) {
    const Vec2 at60 =
        simulateManual(60, 1.0);

    const Vec2 at120 =
        simulateManual(120, 1.0);

    const Vec2 at210 =
        simulateManual(210, 1.0);

    EXPECT_NEAR(
        at60.x,
        at120.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.y,
        at120.y,
        1e-8
    );

    EXPECT_NEAR(
        at60.x,
        at210.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.y,
        at210.y,
        1e-8
    );
}

TEST(CameraMotionTest, InertiaIsFrameRateIndependent) {
    auto run =
        [](int refreshRate) {
            Camera camera;

            CameraMotionConfig config;
            config.acceleration = 0.0;
            config.damping = 6.0;
            config.maxSpeed = 1000000.0;

            CameraController controller{
                camera,
                config
            };

            controller.setVelocity({
                4000.0,
                -1200.0
            });

            const double dt =
                1.0 /
                static_cast<double>(
                    refreshRate
                );

            for (
                int i = 0;
                i < refreshRate;
                ++i
            ) {
                controller.update(dt);
            }

            return camera.position();
        };

    const Vec2 a = run(60);
    const Vec2 b = run(120);
    const Vec2 c = run(210);

    EXPECT_NEAR(a.x, b.x, 1e-8);
    EXPECT_NEAR(a.y, b.y, 1e-8);

    EXPECT_NEAR(a.x, c.x, 1e-8);
    EXPECT_NEAR(a.y, c.y, 1e-8);
}

TEST(CameraMotionTest, FlightIsFrameRateIndependent) {
    const Vec2 at60 =
        simulateFlight(60, 1.0);

    const Vec2 at120 =
        simulateFlight(120, 1.0);

    const Vec2 at210 =
        simulateFlight(210, 1.0);

    EXPECT_NEAR(
        at60.x,
        at120.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.y,
        at120.y,
        1e-8
    );

    EXPECT_NEAR(
        at60.x,
        at210.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.y,
        at210.y,
        1e-8
    );
}

TEST(CameraMotionTest, FlightDoesNotOvershoot) {
    Camera camera{
        {0.0, 0.0},
        1.0
    };

    CameraController controller{
        camera
    };

    controller.flyTo({
        1000.0,
        500.0
    });

    for (int i = 0; i < 300; ++i) {
        const Vec2 before =
            camera.position();

        controller.update(1.0 / 120.0);

        const Vec2 after =
            camera.position();

        EXPECT_GE(after.x, before.x);
        EXPECT_GE(after.y, before.y);

        EXPECT_LE(after.x, 1000.0);
        EXPECT_LE(after.y, 500.0);
    }
}

TEST(CameraMotionTest, PanInputIsNormalized) {
    Camera camera;

    CameraController controller{
        camera
    };

    controller.setPanInput({
        100.0,
        100.0
    });

    EXPECT_NEAR(
        controller.panInput().length(),
        1.0,
        1e-12
    );
}

TEST(CameraMotionTest, VelocityIsLimited) {
    Camera camera;

    CameraMotionConfig config;
    config.maxSpeed = 500.0;

    CameraController controller{
        camera,
        config
    };

    controller.setVelocity({
        10000.0,
        10000.0
    });

    EXPECT_LE(
        controller.velocity().length(),
        500.0 + 1e-9
    );
}

TEST(CameraMotionTest, ZoomClampsAndPreservesAnchor) {
    Camera camera{
        {1000.0, -500.0},
        1.0
    };

    CameraMotionConfig config;
    config.minZoom = 0.5;
    config.maxZoom = 2.0;

    CameraController controller{
        camera,
        config
    };

    const Vec2 anchor{
        800.0,
        450.0
    };

    const Vec2 worldBefore =
        camera.screenToWorld(anchor);

    controller.zoomByFactorAroundScreenPoint(
        100.0,
        anchor
    );

    EXPECT_DOUBLE_EQ(
        camera.zoom(),
        2.0
    );

    const Vec2 worldAfter =
        camera.screenToWorld(anchor);

    EXPECT_NEAR(
        worldBefore.x,
        worldAfter.x,
        1e-9
    );

    EXPECT_NEAR(
        worldBefore.y,
        worldAfter.y,
        1e-9
    );

    controller.zoomByFactorAroundScreenPoint(
        0.0001,
        anchor
    );

    EXPECT_DOUBLE_EQ(
        camera.zoom(),
        0.5
    );
}

TEST(CameraMotionTest, InvalidDeltaDoesNothing) {
    Camera camera{
        {50.0, 60.0},
        1.0
    };

    CameraController controller{
        camera
    };

    controller.setVelocity({
        1000.0,
        1000.0
    });

    controller.update(0.0);
    controller.update(-1.0);

    EXPECT_DOUBLE_EQ(
        camera.position().x,
        50.0
    );

    EXPECT_DOUBLE_EQ(
        camera.position().y,
        60.0
    );
}


TEST(CameraMotionTest, SmoothViewFlightMovesPositionAndZoom) {
    Camera camera{
        {1000.0, 500.0},
        2.0
    };

    CameraController controller{
        camera
    };

    controller.flyToView(
        {0.0, 0.0},
        1.0
    );

    controller.update(
        1.0 / 120.0
    );

    EXPECT_GT(
        camera.position().x,
        0.0
    );

    EXPECT_LT(
        camera.position().x,
        1000.0
    );

    EXPECT_GT(
        camera.zoom(),
        1.0
    );

    EXPECT_LT(
        camera.zoom(),
        2.0
    );

    for (int i = 0; i < 600; ++i) {
        controller.update(
            1.0 / 120.0
        );
    }

    EXPECT_NEAR(
        camera.position().x,
        0.0,
        1e-6
    );

    EXPECT_NEAR(
        camera.position().y,
        0.0,
        1e-6
    );

    EXPECT_NEAR(
        camera.zoom(),
        1.0,
        1e-6
    );

    EXPECT_FALSE(
        controller.isFlying()
    );
}

TEST(CameraMotionTest, ViewFlightIsFrameRateIndependent) {
    auto run =
        [](int refreshRate) {
            Camera camera{
                {5000.0, -3000.0},
                2.2
            };

            CameraController controller{
                camera
            };

            controller.flyToView(
                {-1200.0, 900.0},
                0.75
            );

            const double dt =
                1.0 /
                static_cast<double>(
                    refreshRate
                );

            for (
                int i = 0;
                i < refreshRate;
                ++i
            ) {
                controller.update(dt);
            }

            return std::pair{
                camera.position(),
                camera.zoom()
            };
        };

    const auto at60 = run(60);
    const auto at120 = run(120);
    const auto at210 = run(210);

    EXPECT_NEAR(
        at60.first.x,
        at120.first.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.first.y,
        at120.first.y,
        1e-8
    );

    EXPECT_NEAR(
        at60.second,
        at120.second,
        1e-10
    );

    EXPECT_NEAR(
        at60.first.x,
        at210.first.x,
        1e-8
    );

    EXPECT_NEAR(
        at60.first.y,
        at210.first.y,
        1e-8
    );

    EXPECT_NEAR(
        at60.second,
        at210.second,
        1e-10
    );
}
