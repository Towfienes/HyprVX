#include <limits>

#include <gtest/gtest.h>

#include "hyprvx/core/NavigationState.hpp"

using namespace hyprvx;

TEST(CameraHistoryTest, RecordThenBack) {
    CameraHistory history;

    history.record({
        {100.0, 200.0},
        1.0
    });

    const auto target =
        history.back({
            {900.0, 700.0},
            0.8
        });

    ASSERT_TRUE(target.has_value());

    EXPECT_DOUBLE_EQ(
        target->position.x,
        100.0
    );

    EXPECT_DOUBLE_EQ(
        target->position.y,
        200.0
    );

    EXPECT_DOUBLE_EQ(
        target->zoom,
        1.0
    );
}

TEST(CameraHistoryTest, BackForwardRoundTrip) {
    CameraHistory history;

    const CameraView a{
        {100.0, 200.0},
        1.0
    };

    const CameraView b{
        {2000.0, -500.0},
        0.75
    };

    history.record(a);

    const auto back =
        history.back(b);

    ASSERT_TRUE(back.has_value());

    const auto forward =
        history.forward(*back);

    ASSERT_TRUE(forward.has_value());

    EXPECT_DOUBLE_EQ(
        forward->position.x,
        b.position.x
    );

    EXPECT_DOUBLE_EQ(
        forward->position.y,
        b.position.y
    );

    EXPECT_DOUBLE_EQ(
        forward->zoom,
        b.zoom
    );
}

TEST(CameraHistoryTest, NewRecordClearsForwardHistory) {
    CameraHistory history;

    const CameraView a{
        {0.0, 0.0},
        1.0
    };

    const CameraView b{
        {1000.0, 0.0},
        1.0
    };

    history.record(a);

    const auto back =
        history.back(b);

    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(history.canGoForward());

    history.record(*back);

    EXPECT_FALSE(
        history.canGoForward()
    );
}

TEST(CameraHistoryTest, DeduplicatesEquivalentViews) {
    CameraHistory history;

    const CameraView view{
        {100.0, 200.0},
        1.0
    };

    history.record(view);
    history.record(view);

    EXPECT_EQ(
        history.backSize(),
        1u
    );
}

TEST(CameraHistoryTest, RespectsCapacity) {
    CameraHistory history{2};

    history.record({
        {0.0, 0.0},
        1.0
    });

    history.record({
        {100.0, 0.0},
        1.0
    });

    history.record({
        {200.0, 0.0},
        1.0
    });

    const auto first =
        history.back({
            {300.0, 0.0},
            1.0
        });

    ASSERT_TRUE(first.has_value());

    EXPECT_DOUBLE_EQ(
        first->position.x,
        200.0
    );

    const auto second =
        history.back(*first);

    ASSERT_TRUE(second.has_value());

    EXPECT_DOUBLE_EQ(
        second->position.x,
        100.0
    );

    EXPECT_FALSE(
        history.back(*second)
            .has_value()
    );
}

TEST(ZoneStoreTest, StoresAndFindsZone) {
    ZoneStore zones;

    EXPECT_TRUE(
        zones.set({
            1,
            "Code",
            {
                {2500.0, -1400.0},
                0.9
            }
        })
    );

    const Zone* zone =
        zones.find(1);

    ASSERT_NE(zone, nullptr);

    EXPECT_EQ(
        zone->name,
        "Code"
    );

    EXPECT_DOUBLE_EQ(
        zone->view.position.x,
        2500.0
    );

    EXPECT_DOUBLE_EQ(
        zone->view.zoom,
        0.9
    );
}

TEST(ZoneStoreTest, UpdatesExistingZone) {
    ZoneStore zones;

    EXPECT_TRUE(
        zones.set({
            2,
            "School",
            {
                {100.0, 200.0},
                1.0
            }
        })
    );

    EXPECT_FALSE(
        zones.set({
            2,
            "School Updated",
            {
                {-800.0, 400.0},
                0.75
            }
        })
    );

    EXPECT_EQ(
        zones.size(),
        1u
    );

    const Zone* zone =
        zones.find(2);

    ASSERT_NE(zone, nullptr);

    EXPECT_EQ(
        zone->name,
        "School Updated"
    );

    EXPECT_DOUBLE_EQ(
        zone->view.position.x,
        -800.0
    );
}

TEST(ZoneStoreTest, RejectsInvalidZone) {
    ZoneStore zones;

    EXPECT_THROW(
        zones.set({
            0,
            "",
            {
                {0.0, 0.0},
                1.0
            }
        }),
        std::invalid_argument
    );

    EXPECT_THROW(
        zones.set({
            1,
            "Broken",
            {
                {0.0, 0.0},
                std::numeric_limits<double>::infinity()
            }
        }),
        std::invalid_argument
    );
}


TEST(
    CameraHistoryTransactionTest,
    ChangedViewRecordsSingleHistoryEntry
) {
    CameraHistory history;

    CameraHistoryTransaction transaction{
        history
    };

    const CameraView start{
        {100.0, 200.0},
        1.0
    };

    const CameraView end{
        {900.0, -300.0},
        0.75
    };

    transaction.begin(start);

    EXPECT_TRUE(
        transaction.finish(end)
    );

    EXPECT_EQ(
        history.backSize(),
        1u
    );

    const auto back =
        history.back(end);

    ASSERT_TRUE(
        back.has_value()
    );

    EXPECT_DOUBLE_EQ(
        back->position.x,
        start.position.x
    );

    EXPECT_DOUBLE_EQ(
        back->position.y,
        start.position.y
    );

    EXPECT_DOUBLE_EQ(
        back->zoom,
        start.zoom
    );
}

TEST(
    CameraHistoryTransactionTest,
    RepeatedBeginPreservesOriginalStart
) {
    CameraHistory history;

    CameraHistoryTransaction transaction{
        history
    };

    const CameraView first{
        {0.0, 0.0},
        1.0
    };

    const CameraView middle{
        {500.0, 500.0},
        0.9
    };

    const CameraView end{
        {1200.0, 800.0},
        0.7
    };

    transaction.begin(first);
    transaction.begin(middle);

    EXPECT_TRUE(
        transaction.finish(end)
    );

    const auto back =
        history.back(end);

    ASSERT_TRUE(
        back.has_value()
    );

    EXPECT_DOUBLE_EQ(
        back->position.x,
        first.position.x
    );

    EXPECT_DOUBLE_EQ(
        back->position.y,
        first.position.y
    );

    EXPECT_DOUBLE_EQ(
        back->zoom,
        first.zoom
    );
}

TEST(
    CameraHistoryTransactionTest,
    UnchangedViewDoesNotCreateHistory
) {
    CameraHistory history;

    CameraHistoryTransaction transaction{
        history
    };

    const CameraView view{
        {100.0, 200.0},
        1.0
    };

    transaction.begin(view);

    EXPECT_FALSE(
        transaction.finish(view)
    );

    EXPECT_FALSE(
        history.canGoBack()
    );

    EXPECT_FALSE(
        transaction.active()
    );
}

TEST(
    CameraHistoryTransactionTest,
    CancelCreatesNoHistory
) {
    CameraHistory history;

    CameraHistoryTransaction transaction{
        history
    };

    transaction.begin({
        {100.0, 200.0},
        1.0
    });

    EXPECT_TRUE(
        transaction.active()
    );

    transaction.cancel();

    EXPECT_FALSE(
        transaction.active()
    );

    EXPECT_FALSE(
        history.canGoBack()
    );
}

TEST(
    CameraHistoryTransactionTest,
    MultipleMotionSamplesStillCreateOneEntry
) {
    CameraHistory history;

    CameraHistoryTransaction transaction{
        history
    };

    const CameraView start{
        {-1000.0, 500.0},
        1.25
    };

    transaction.begin(start);

    for (
        int i = 0;
        i < 500;
        ++i
    ) {
        transaction.begin({
            {
                static_cast<double>(i),
                static_cast<double>(i * 2)
            },
            1.0
        });
    }

    const CameraView end{
        {5000.0, -2000.0},
        0.6
    };

    EXPECT_TRUE(
        transaction.finish(end)
    );

    EXPECT_EQ(
        history.backSize(),
        1u
    );

    const auto back =
        history.back(end);

    ASSERT_TRUE(
        back.has_value()
    );

    EXPECT_DOUBLE_EQ(
        back->position.x,
        start.position.x
    );

    EXPECT_DOUBLE_EQ(
        back->position.y,
        start.position.y
    );

    EXPECT_DOUBLE_EQ(
        back->zoom,
        start.zoom
    );
}
