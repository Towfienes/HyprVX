#include <gtest/gtest.h>

#include "hyprvx/core/ViewportFraming.hpp"
#include "hyprvx/core/World.hpp"

using namespace hyprvx;

TEST(
    ViewportFramingTest,
    CentersWindowAtCurrentZoom
) {
    const Rect window{
        {1000.0, 500.0},
        {800.0, 600.0}
    };

    const FramedView view =
        frameRectInViewport(
            window,
            {1600.0, 1000.0},
            1.0
        );

    EXPECT_DOUBLE_EQ(
        view.zoom,
        1.0
    );

    const Vec2 visibleCenter{
        view.cameraPosition.x +
            800.0,

        view.cameraPosition.y +
            500.0
    };

    EXPECT_NEAR(
        visibleCenter.x,
        window.center().x,
        1e-9
    );

    EXPECT_NEAR(
        visibleCenter.y,
        window.center().y,
        1e-9
    );
}

TEST(
    ViewportFramingTest,
    ZoomsOutWhenWindowDoesNotFit
) {
    const Rect hugeWindow{
        {0.0, 0.0},
        {4000.0, 3000.0}
    };

    const FramedView view =
        frameRectInViewport(
            hugeWindow,
            {1400.0, 900.0},
            2.0
        );

    EXPECT_LT(
        view.zoom,
        1.0
    );

    EXPECT_GE(
        view.zoom,
        0.20
    );
}

TEST(
    ViewportFramingTest,
    DoesNotZoomInWhenPreservingZoom
) {
    const Rect tinyWindow{
        {100.0, 100.0},
        {100.0, 100.0}
    };

    const FramedView view =
        frameRectInViewport(
            tinyWindow,
            {1600.0, 1000.0},
            0.75
        );

    EXPECT_DOUBLE_EQ(
        view.zoom,
        0.75
    );
}

TEST(
    ViewportFramingTest,
    WorksWithNegativeCoordinates
) {
    const Rect window{
        {-5000.0, -3000.0},
        {900.0, 700.0}
    };

    const FramedView view =
        frameRectInViewport(
            window,
            {1800.0, 1200.0},
            1.0
        );

    EXPECT_LT(
        view.cameraPosition.x,
        0.0
    );

    EXPECT_LT(
        view.cameraPosition.y,
        0.0
    );
}

TEST(
    ViewportFramingTest,
    RejectsInvalidViewport
) {
    EXPECT_THROW(
        frameRectInViewport(
            {
                {0.0, 0.0},
                {100.0, 100.0}
            },
            {0.0, 900.0},
            1.0
        ),
        std::invalid_argument
    );
}

TEST(
    WorldGeometryTest,
    FindsGeometryAcrossOwners
) {
    World world;

    ASSERT_TRUE(
        world.addFreeWindow({
            1,
            {
                {100.0, 200.0},
                {500.0, 400.0}
            }
        })
    );

    Island island{
        10,
        {2000.0, -1000.0}
    };

    ASSERT_TRUE(
        island.addMember({
            2,
            {
                {300.0, 200.0},
                {700.0, 500.0}
            }
        })
    );

    ASSERT_TRUE(
        world.addIsland(
            std::move(island)
        )
    );

    const auto free =
        world.windowGeometry(1);

    ASSERT_TRUE(
        free.has_value()
    );

    EXPECT_DOUBLE_EQ(
        free->position.x,
        100.0
    );

    const auto member =
        world.windowGeometry(2);

    ASSERT_TRUE(
        member.has_value()
    );

    EXPECT_DOUBLE_EQ(
        member->position.x,
        2300.0
    );

    EXPECT_DOUBLE_EQ(
        member->position.y,
        -800.0
    );
}
