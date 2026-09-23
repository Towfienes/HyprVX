#include <stdexcept>

#include <gtest/gtest.h>

#include "hyprvx/core/Camera.hpp"

using namespace hyprvx;

TEST(CameraTest, ConvertsNegativeWorldCoordinates) {
    Camera camera{
        {-1000.0, -500.0},
        2.0
    };

    const Vec2 screen =
        camera.worldToScreen({
            -750.0,
            -250.0
        });

    EXPECT_DOUBLE_EQ(screen.x, 500.0);
    EXPECT_DOUBLE_EQ(screen.y, 500.0);

    const Vec2 roundTrip =
        camera.screenToWorld(screen);

    EXPECT_DOUBLE_EQ(roundTrip.x, -750.0);
    EXPECT_DOUBLE_EQ(roundTrip.y, -250.0);
}

TEST(CameraTest, ZoomKeepsAnchorStable) {
    Camera camera{
        {100.0, 200.0},
        1.0
    };

    const Vec2 anchor{
        600.0,
        400.0
    };

    const Vec2 worldBefore =
        camera.screenToWorld(anchor);

    camera.setZoomAroundScreenPoint(
        2.5,
        anchor
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
}

TEST(CameraTest, SupportsLargeCoordinates) {
    Camera camera{
        {1.0e12, -1.0e12},
        1.75
    };

    const Vec2 world{
        1.0e12 + 512.25,
        -1.0e12 + 128.5
    };

    const Vec2 screen =
        camera.worldToScreen(world);

    const Vec2 restored =
        camera.screenToWorld(screen);

    EXPECT_NEAR(
        restored.x,
        world.x,
        1e-4
    );

    EXPECT_NEAR(
        restored.y,
        world.y,
        1e-4
    );
}

TEST(CameraTest, RejectsInvalidZoom) {
    Camera camera;

    EXPECT_THROW(
        camera.setZoom(0.0),
        std::invalid_argument
    );

    EXPECT_THROW(
        camera.setZoom(-1.0),
        std::invalid_argument
    );
}
