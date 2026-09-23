#include <gtest/gtest.h>

#include "hyprvx/core/Rect.hpp"
#include "hyprvx/core/Vec2.hpp"

using namespace hyprvx;

TEST(Vec2Test, Arithmetic) {
    const Vec2 a{10.0, -20.0};
    const Vec2 b{-5.0, 4.0};

    const Vec2 sum = a + b;

    EXPECT_DOUBLE_EQ(sum.x, 5.0);
    EXPECT_DOUBLE_EQ(sum.y, -16.0);

    const Vec2 scaled = a * 2.0;

    EXPECT_DOUBLE_EQ(scaled.x, 20.0);
    EXPECT_DOUBLE_EQ(scaled.y, -40.0);
}

TEST(RectTest, HandlesNegativeWorldCoordinates) {
    const Rect rect{
        {-500.0, -300.0},
        {1000.0, 600.0}
    };

    EXPECT_TRUE(
        rect.contains({0.0, 0.0})
    );

    EXPECT_FALSE(
        rect.contains({600.0, 0.0})
    );
}

TEST(RectTest, UnionProducesBoundingBox) {
    const Rect a{
        {-100.0, -100.0},
        {200.0, 200.0}
    };

    const Rect b{
        {200.0, 100.0},
        {300.0, 400.0}
    };

    const Rect result =
        Rect::united(a, b);

    EXPECT_DOUBLE_EQ(
        result.position.x,
        -100.0
    );

    EXPECT_DOUBLE_EQ(
        result.position.y,
        -100.0
    );

    EXPECT_DOUBLE_EQ(
        result.size.x,
        600.0
    );

    EXPECT_DOUBLE_EQ(
        result.size.y,
        600.0
    );
}
