#include "hyprvx/platform/PluginCompatibility.hpp"
#include <gtest/gtest.h>

using namespace hyprvx::platform;

TEST(PluginCompatibilityTest, AcceptsOnlyExactCompiledAndRunningCommit) {
    EXPECT_TRUE(pluginCommitCompatible(SupportedHyprlandCommit, SupportedHyprlandCommit));
    EXPECT_FALSE(pluginCommitCompatible("wrong", SupportedHyprlandCommit));
    EXPECT_FALSE(pluginCommitCompatible(SupportedHyprlandCommit, "wrong"));
}
