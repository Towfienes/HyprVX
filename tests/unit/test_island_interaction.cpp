#include <gtest/gtest.h>

#include "hyprvx/core/IslandInteraction.hpp"

using namespace hyprvx;

namespace {

World makeWorld() {
    World world;

    world.addFreeWindow({
        1,
        {
            {900.0, 900.0},
            {300.0, 200.0}
        }
    });

    Island island{
        10,
        {1000.0, 1000.0}
    };

    island.addMember({
        2,
        {
            {0.0, 0.0},
            {600.0, 500.0}
        }
    });

    world.addIsland(
        std::move(island)
    );

    return world;
}

}

TEST(
    IslandInteractionTest,
    FreeWindowBecomesCandidate
) {
    World world =
        makeWorld();

    auto* window =
        world.freeWindow(1);

    ASSERT_NE(window, nullptr);

    window->geometry.position =
        {1500.0, 1100.0};

    IslandInteractionController interaction;

    interaction.beginFreeWindow(
        1,
        10
    );

    interaction.update(
        world,
        0.10
    );

    EXPECT_EQ(
        interaction.state().phase,
        IslandInteractionPhase::
            JoinCandidate
    );
}

TEST(
    IslandInteractionTest,
    JoinRequiresDwell
) {
    World world =
        makeWorld();

    auto* window =
        world.freeWindow(1);

    ASSERT_NE(window, nullptr);

    window->geometry.position =
        {1500.0, 1100.0};

    IslandInteractionController interaction;

    interaction.beginFreeWindow(
        1,
        10
    );

    interaction.update(
        world,
        0.10
    );

    EXPECT_FALSE(
        interaction.commit(world)
    );

    EXPECT_TRUE(
        world.hasFreeWindow(1)
    );
}

TEST(
    IslandInteractionTest,
    JoinReadyAfterDwell
) {
    World world =
        makeWorld();

    auto* window =
        world.freeWindow(1);

    ASSERT_NE(window, nullptr);

    window->geometry.position =
        {1500.0, 1100.0};

    IslandInteractionController interaction;

    interaction.beginFreeWindow(
        1,
        10
    );

    interaction.update(
        world,
        0.30
    );

    EXPECT_EQ(
        interaction.state().phase,
        IslandInteractionPhase::
            JoinReady
    );

    EXPECT_TRUE(
        interaction.commit(world)
    );

    EXPECT_FALSE(
        world.hasFreeWindow(1)
    );

    EXPECT_EQ(
        world.islandForWindow(1),
        10
    );
}

TEST(
    IslandInteractionTest,
    LeavingJoinZoneResetsDwell
) {
    World world =
        makeWorld();

    auto* window =
        world.freeWindow(1);

    ASSERT_NE(window, nullptr);

    IslandInteractionController interaction;

    interaction.beginFreeWindow(
        1,
        10
    );

    window->geometry.position =
        {1500.0, 1100.0};

    interaction.update(
        world,
        0.20
    );

    window->geometry.position =
        {-5000.0, -5000.0};

    interaction.update(
        world,
        0.01
    );

    EXPECT_EQ(
        interaction.state().phase,
        IslandInteractionPhase::Idle
    );

    EXPECT_DOUBLE_EQ(
        interaction
            .state()
            .candidateElapsed,
        0.0
    );
}

TEST(
    IslandInteractionTest,
    MemberDetachUsesReferenceBounds
) {
    World world =
        makeWorld();

    ASSERT_TRUE(
        world.joinFreeWindowToIsland(
            1,
            10
        )
    );

    Island* island =
        world.island(10);

    ASSERT_NE(island, nullptr);

    const auto bounds =
        island->worldBounds();

    ASSERT_TRUE(bounds.has_value());

    IslandInteractionController interaction;

    interaction.beginIslandMember(
        1,
        10,
        *bounds
    );

    IslandMember* member =
        island->findMember(1);

    ASSERT_NE(member, nullptr);

    member->localGeometry.position +=
        Vec2{
            3000.0,
            0.0
        };

    interaction.update(
        world,
        0.016
    );

    EXPECT_EQ(
        interaction.state().phase,
        IslandInteractionPhase::
            DetachCandidate
    );

    EXPECT_TRUE(
        interaction.commit(world)
    );

    EXPECT_TRUE(
        world.hasFreeWindow(1)
    );
}

TEST(
    IslandInteractionTest,
    InvalidPolicyRejected
) {
    IslandInteractionPolicy policy;

    policy.joinMargin = -1.0;

    EXPECT_THROW(
        IslandInteractionController{
            policy
        },
        std::invalid_argument
    );
}
