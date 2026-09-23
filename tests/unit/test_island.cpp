#include <gtest/gtest.h>

#include "hyprvx/core/Island.hpp"

using namespace hyprvx;

TEST(IslandTest, ConvertsLocalGeometryToWorld) {
    Island island{
        1,
        {5000.0, -2000.0}
    };

    ASSERT_TRUE(
        island.addMember({
            42,
            {
                {200.0, 100.0},
                {800.0, 600.0}
            }
        })
    );

    const auto geometry =
        island.memberWorldGeometry(42);

    ASSERT_TRUE(
        geometry.has_value()
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.x,
        5200.0
    );

    EXPECT_DOUBLE_EQ(
        geometry->position.y,
        -1900.0
    );
}

TEST(IslandTest, MovingIslandPreservesMemberLocalPosition) {
    Island island{
        1,
        {1000.0, 1000.0}
    };

    island.addMember({
        5,
        {
            {50.0, 75.0},
            {500.0, 300.0}
        }
    });

    const auto* before =
        island.findMember(5);

    ASSERT_NE(before, nullptr);

    const Vec2 localBefore =
        before->localGeometry.position;

    island.moveBy({
        2000.0,
        -4000.0
    });

    const auto* after =
        island.findMember(5);

    ASSERT_NE(after, nullptr);

    EXPECT_DOUBLE_EQ(
        after->localGeometry.position.x,
        localBefore.x
    );

    EXPECT_DOUBLE_EQ(
        after->localGeometry.position.y,
        localBefore.y
    );

    const auto world =
        island.memberWorldGeometry(5);

    ASSERT_TRUE(world.has_value());

    EXPECT_DOUBLE_EQ(
        world->position.x,
        3050.0
    );

    EXPECT_DOUBLE_EQ(
        world->position.y,
        -2925.0
    );
}

TEST(IslandTest, ComputesWorldBounds) {
    Island island{
        1,
        {1000.0, 2000.0}
    };

    island.addMember({
        1,
        {
            {0.0, 0.0},
            {500.0, 500.0}
        }
    });

    island.addMember({
        2,
        {
            {600.0, 100.0},
            {400.0, 600.0}
        }
    });

    const auto bounds =
        island.worldBounds();

    ASSERT_TRUE(bounds.has_value());

    EXPECT_DOUBLE_EQ(
        bounds->position.x,
        1000.0
    );

    EXPECT_DOUBLE_EQ(
        bounds->position.y,
        2000.0
    );

    EXPECT_DOUBLE_EQ(
        bounds->size.x,
        1000.0
    );

    EXPECT_DOUBLE_EQ(
        bounds->size.y,
        700.0
    );
}
