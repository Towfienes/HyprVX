#include <gtest/gtest.h>

#include "hyprvx/core/World.hpp"

using namespace hyprvx;

TEST(WorldTest, StoresFreeWindows) {
    World world;

    EXPECT_TRUE(
        world.addFreeWindow({
            100,
            {
                {-500.0, 250.0},
                {800.0, 600.0}
            }
        })
    );

    EXPECT_FALSE(
        world.addFreeWindow({
            100,
            {
                {0.0, 0.0},
                {10.0, 10.0}
            }
        })
    );

    ASSERT_TRUE(
        world.hasFreeWindow(100)
    );

    const auto* window =
        world.freeWindow(100);

    ASSERT_NE(window, nullptr);

    EXPECT_DOUBLE_EQ(
        window->geometry.position.x,
        -500.0
    );
}

TEST(WorldTest, StoresIslands) {
    World world;

    EXPECT_TRUE(
        world.addIsland(
            Island{
                77,
                {-3000.0, 9000.0}
            }
        )
    );

    EXPECT_FALSE(
        world.addIsland(
            Island{
                77,
                {0.0, 0.0}
            }
        )
    );

    ASSERT_EQ(
        world.islandCount(),
        1u
    );

    const Island* island =
        world.island(77);

    ASSERT_NE(
        island,
        nullptr
    );

    EXPECT_DOUBLE_EQ(
        island->origin().x,
        -3000.0
    );
}
