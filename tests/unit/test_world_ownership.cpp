#include <gtest/gtest.h>

#include "hyprvx/core/World.hpp"

using namespace hyprvx;

TEST(
    WorldOwnershipTest,
    JoinPreservesWorldGeometry
) {
    World world;

    ASSERT_TRUE(
        world.addFreeWindow({
            42,
            {
                {5400.0, 2250.0},
                {900.0, 600.0}
            }
        })
    );

    ASSERT_TRUE(
        world.addIsland(
            Island{
                10,
                {5000.0, 2000.0}
            }
        )
    );

    ASSERT_TRUE(
        world.joinFreeWindowToIsland(
            42,
            10
        )
    );

    EXPECT_EQ(
        world.freeWindow(42),
        nullptr
    );

    const Island* island =
        world.island(10);

    ASSERT_NE(island, nullptr);

    const IslandMember* member =
        island->findMember(42);

    ASSERT_NE(member, nullptr);

    EXPECT_DOUBLE_EQ(
        member->localGeometry.position.x,
        400.0
    );

    EXPECT_DOUBLE_EQ(
        member->localGeometry.position.y,
        250.0
    );

    const auto geometry =
        island->memberWorldGeometry(42);

    ASSERT_TRUE(
        geometry.has_value()
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.x,
        5400.0
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.y,
        2250.0
    );
}

TEST(
    WorldOwnershipTest,
    IslandMovementMovesJoinedWindow
) {
    World world;

    world.addFreeWindow({
        5,
        {
            {1200.0, 1300.0},
            {400.0, 300.0}
        }
    });

    world.addIsland(
        Island{
            7,
            {1000.0, 1000.0}
        }
    );

    ASSERT_TRUE(
        world.joinFreeWindowToIsland(
            5,
            7
        )
    );

    Island* island =
        world.island(7);

    ASSERT_NE(island, nullptr);

    island->moveBy({
        500.0,
        -200.0
    });

    const auto geometry =
        island->memberWorldGeometry(5);

    ASSERT_TRUE(
        geometry.has_value()
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.x,
        1700.0
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.y,
        1100.0
    );
}

TEST(
    WorldOwnershipTest,
    DetachPreservesCurrentWorldGeometry
) {
    World world;

    world.addFreeWindow({
        5,
        {
            {1200.0, 1300.0},
            {400.0, 300.0}
        }
    });

    world.addIsland(
        Island{
            7,
            {1000.0, 1000.0}
        }
    );

    ASSERT_TRUE(
        world.joinFreeWindowToIsland(
            5,
            7
        )
    );

    Island* island =
        world.island(7);

    ASSERT_NE(island, nullptr);

    island->moveBy({
        500.0,
        -200.0
    });

    ASSERT_TRUE(
        world.detachWindowFromIsland(
            5,
            7
        )
    );

    const WorldWindow* window =
        world.freeWindow(5);

    ASSERT_NE(window, nullptr);

    EXPECT_DOUBLE_EQ(
        window->geometry.position.x,
        1700.0
    );

    EXPECT_DOUBLE_EQ(
        window->geometry.position.y,
        1100.0
    );

    EXPECT_EQ(
        island->findMember(5),
        nullptr
    );
}

TEST(WorldOwnershipTest, RemoveWindowRemovesFreeOrIslandOwnedWindow) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({41, {{10.0, 20.0}, {100.0, 80.0}}}));
    EXPECT_TRUE(world.removeWindow(41));
    EXPECT_FALSE(world.windowExists(41));

    ASSERT_TRUE(world.addFreeWindow({42, {{10.0, 20.0}, {100.0, 80.0}}}));
    ASSERT_TRUE(world.addIsland(Island{7, {}}));
    ASSERT_TRUE(world.joinFreeWindowToIsland(42, 7));
    EXPECT_TRUE(world.removeWindow(42));
    EXPECT_FALSE(world.windowExists(42));
    ASSERT_NE(world.island(7), nullptr);
    EXPECT_TRUE(world.island(7)->members().empty());
}

TEST(
    WorldOwnershipTest,
    WindowHasSingleOwner
) {
    World world;

    Island island{
        20,
        {0.0, 0.0}
    };

    ASSERT_TRUE(
        island.addMember({
            99,
            {
                {10.0, 20.0},
                {500.0, 500.0}
            }
        })
    );

    ASSERT_TRUE(
        world.addIsland(
            std::move(island)
        )
    );

    EXPECT_FALSE(
        world.addFreeWindow({
            99,
            {
                {0.0, 0.0},
                {1.0, 1.0}
            }
        })
    );

    EXPECT_TRUE(
        world.windowExists(99)
    );

    const auto owner =
        world.islandForWindow(99);

    ASSERT_TRUE(owner.has_value());

    EXPECT_EQ(
        *owner,
        20u
    );
}

TEST(
    WorldOwnershipTest,
    InvalidTransfersDoNotMutateWorld
) {
    World world;

    ASSERT_TRUE(
        world.addFreeWindow({
            1,
            {
                {50.0, 60.0},
                {700.0, 400.0}
            }
        })
    );

    ASSERT_TRUE(
        world.addIsland(
            Island{
                10,
                {1000.0, 1000.0}
            }
        )
    );

    EXPECT_FALSE(
        world.joinFreeWindowToIsland(
            999,
            10
        )
    );

    EXPECT_FALSE(
        world.joinFreeWindowToIsland(
            1,
            999
        )
    );

    EXPECT_TRUE(
        world.hasFreeWindow(1)
    );

    EXPECT_FALSE(
        world.detachWindowFromIsland(
            1,
            10
        )
    );

    EXPECT_TRUE(
        world.hasFreeWindow(1)
    );
}
