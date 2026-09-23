#include <gtest/gtest.h>

#include "hyprvx/core/OutputTopology.hpp"

using namespace hyprvx;

namespace {

OutputTopologySnapshot snapshot(
    bool laptopConnected,
    bool laptopActive,
    bool externalConnected,
    bool externalActive,
    std::optional<std::string>
        primary = std::nullopt
) {
    OutputTopologySnapshot result;

    result.outputs["eDP-2"] = {
        laptopConnected,
        laptopActive
    };

    result.outputs["DP-1"] = {
        externalConnected,
        externalActive
    };

    result.primary =
        std::move(primary);

    return result;
}

}

TEST(
    OutputTopologyPlannerTest,
    LaptopOnlyBootActivatesLaptop
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                false,
                false,
                false
            )
        );

    ASSERT_EQ(
        actions.size(),
        2u
    );

    EXPECT_EQ(
        actions[0],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    Activate,
                "eDP-2"
            }
        )
    );

    EXPECT_EQ(
        actions[1],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    SelectPrimary,
                "eDP-2"
            }
        )
    );
}

TEST(
    OutputTopologyPlannerTest,
    ExternalBootPrefersExternal
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                false,
                true,
                false
            )
        );

    ASSERT_EQ(
        actions.size(),
        2u
    );

    EXPECT_EQ(
        actions[0].kind,
        OutputTopologyActionKind::
            Activate
    );

    EXPECT_EQ(
        actions[0].output,
        "DP-1"
    );

    EXPECT_EQ(
        actions[1].kind,
        OutputTopologyActionKind::
            SelectPrimary
    );

    EXPECT_EQ(
        actions[1].output,
        "DP-1"
    );
}

TEST(
    OutputTopologyPlannerTest,
    ExternalArrivalSwitchesBeforeDisablingLaptop
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                true,
                true,
                true,
                "eDP-2"
            )
        );

    ASSERT_EQ(
        actions.size(),
        2u
    );

    EXPECT_EQ(
        actions[0],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    SelectPrimary,
                "DP-1"
            }
        )
    );

    EXPECT_EQ(
        actions[1],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    Deactivate,
                "eDP-2"
            }
        )
    );
}

TEST(
    OutputTopologyPlannerTest,
    ExternalUnplugActivatesLaptopBeforeDisablingExternal
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                false,
                false,
                true,
                "DP-1"
            )
        );

    ASSERT_EQ(
        actions.size(),
        3u
    );

    EXPECT_EQ(
        actions[0],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    Activate,
                "eDP-2"
            }
        )
    );

    EXPECT_EQ(
        actions[1],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    SelectPrimary,
                "eDP-2"
            }
        )
    );

    EXPECT_EQ(
        actions[2],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    Deactivate,
                "DP-1"
            }
        )
    );
}

TEST(
    OutputTopologyPlannerTest,
    ExternalSteadyStateIsIdempotent
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                false,
                true,
                true,
                "DP-1"
            )
        );

    EXPECT_TRUE(
        actions.empty()
    );
}

TEST(
    OutputTopologyPlannerTest,
    LaptopSteadyStateIsIdempotent
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                true,
                false,
                false,
                "eDP-2"
            )
        );

    EXPECT_TRUE(
        actions.empty()
    );
}

TEST(
    OutputTopologyPlannerTest,
    RemovesStaleDisconnectedExternal
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                true,
                true,
                false,
                true,
                "eDP-2"
            )
        );

    ASSERT_EQ(
        actions.size(),
        1u
    );

    EXPECT_EQ(
        actions[0],
        (
            OutputTopologyAction{
                OutputTopologyActionKind::
                    Deactivate,
                "DP-1"
            }
        )
    );
}

TEST(
    OutputTopologyPlannerTest,
    StaleLaptopIsRemovedWhenExternalTakesOver
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                false,
                true,
                true,
                false
            )
        );

    ASSERT_EQ(
        actions.size(),
        3u
    );

    EXPECT_EQ(
        actions[0].kind,
        OutputTopologyActionKind::
            Activate
    );

    EXPECT_EQ(
        actions[0].output,
        "DP-1"
    );

    EXPECT_EQ(
        actions[1].kind,
        OutputTopologyActionKind::
            SelectPrimary
    );

    EXPECT_EQ(
        actions[2].kind,
        OutputTopologyActionKind::
            Deactivate
    );

    EXPECT_EQ(
        actions[2].output,
        "eDP-2"
    );
}

TEST(
    OutputTopologyPlannerTest,
    NoConnectedOutputsDeactivatesStalePolicyOutputs
) {
    OutputTopologyPlanner planner;

    const auto actions =
        planner.plan(
            snapshot(
                false,
                true,
                false,
                true,
                "DP-1"
            )
        );

    ASSERT_EQ(
        actions.size(),
        2u
    );

    EXPECT_EQ(
        actions[0].output,
        "DP-1"
    );

    EXPECT_EQ(
        actions[1].output,
        "eDP-2"
    );
}

TEST(
    OutputTopologyPlannerTest,
    RejectsInvalidPolicy
) {
    LaptopExternalTopologyPolicy policy;

    policy.externalOutput =
        policy.laptopOutput;

    EXPECT_THROW(
        OutputTopologyPlanner{
            policy
        },
        std::invalid_argument
    );
}
