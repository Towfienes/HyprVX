#include <gtest/gtest.h>

#include "hyprvx/core/SpatialNavigation.hpp"

using namespace hyprvx;

TEST(
    SpatialNavigationTest,
    ChoosesAlignedRightCandidateOverCloserDiagonal
) {
    World world;

    world.addFreeWindow({
        1,
        {
            {0.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        2,
        {
            {400.0, 10.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        3,
        {
            {230.0, 210.0},
            {100.0, 100.0}
        }
    });

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Right
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->windowId,
        2u
    );
}

TEST(
    SpatialNavigationTest,
    IgnoresCandidatesBehindDirection
) {
    World world;

    world.addFreeWindow({
        1,
        {
            {0.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        2,
        {
            {-150.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        3,
        {
            {500.0, 0.0},
            {100.0, 100.0}
        }
    });

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Right
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->windowId,
        3u
    );
}

TEST(
    SpatialNavigationTest,
    PrefersAxisOverlap
) {
    World world;

    world.addFreeWindow({
        1,
        {
            {0.0, 0.0},
            {300.0, 300.0}
        }
    });

    world.addFreeWindow({
        2,
        {
            {500.0, 30.0},
            {300.0, 300.0}
        }
    });

    world.addFreeWindow({
        3,
        {
            {430.0, 300.0},
            {300.0, 300.0}
        }
    });

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Right
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->windowId,
        2u
    );

    EXPECT_GT(
        result->overlapRatio,
        0.8
    );
}

TEST(
    SpatialNavigationTest,
    SameIslandGetsModeratePreference
) {
    World world;

    Island island{
        10,
        {1000.0, 1000.0}
    };

    island.addMember({
        1,
        {
            {0.0, 0.0},
            {200.0, 200.0}
        }
    });

    island.addMember({
        2,
        {
            {420.0, 0.0},
            {200.0, 200.0}
        }
    });

    ASSERT_TRUE(
        world.addIsland(
            std::move(island)
        )
    );

    ASSERT_TRUE(
        world.addFreeWindow({
            3,
            {
                {1370.0, 1000.0},
                {200.0, 200.0}
            }
        })
    );

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Right
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->windowId,
        2u
    );

    ASSERT_TRUE(
        result->islandId.has_value()
    );

    EXPECT_EQ(
        *result->islandId,
        10u
    );
}

TEST(
    SpatialNavigationTest,
    WorksAcrossNegativeWorldCoordinates
) {
    World world;

    world.addFreeWindow({
        1,
        {
            {-5000.0, -4000.0},
            {300.0, 300.0}
        }
    });

    world.addFreeWindow({
        2,
        {
            {-5000.0, -5000.0},
            {300.0, 300.0}
        }
    });

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Up
        );

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->windowId,
        2u
    );
}

TEST(
    SpatialNavigationTest,
    ReturnsNothingWhenNoCandidateExists
) {
    World world;

    world.addFreeWindow({
        1,
        {
            {0.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        2,
        {
            {-500.0, 0.0},
            {100.0, 100.0}
        }
    });

    SpatialNavigator navigator;

    const auto result =
        navigator.findNeighbor(
            world,
            1,
            SpatialDirection::Right
        );

    EXPECT_FALSE(
        result.has_value()
    );
}

TEST(
    SpatialNavigationTest,
    TieBreakIsDeterministic
) {
    World world;

    world.addFreeWindow({
        10,
        {
            {0.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        5,
        {
            {500.0, 0.0},
            {100.0, 100.0}
        }
    });

    world.addFreeWindow({
        3,
        {
            {500.0, 0.0},
            {100.0, 100.0}
        }
    });

    SpatialNavigator navigator;

    for (int i = 0; i < 100; ++i) {
        const auto result =
            navigator.findNeighbor(
                world,
                10,
                SpatialDirection::Right
            );

        ASSERT_TRUE(
            result.has_value()
        );

        EXPECT_EQ(
            result->windowId,
            3u
        );
    }
}

TEST(
    SpatialNavigationTest,
    RejectsInvalidPolicy
) {
    SpatialNavigationPolicy policy;

    policy.maxAngleDegrees = 100.0;

    EXPECT_THROW(
        SpatialNavigator{
            policy
        },
        std::invalid_argument
    );
}
