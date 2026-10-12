// Regression tests for the delivery form S build guard (host trial, 2026-10-12): the user's distribution Hyprland
// 0.56.2 at efb5099 reports GIT_DIRTY "clean", while the guard only accepted HyprVX's own "clean-upstream" label and
// refused an unmodified upstream build ("built for Hyprland efb509937800 (clean-upstream) but running ... (clean)").
// Identities below are copied from `hyprctl version -j` of the host and of the HyprVX stock prefix.

#include "hyprvx/platform/PluginCompatibility.hpp"

#include <gtest/gtest.h>

using hyprvx::platform::BuildTree;
using hyprvx::platform::classifyBuildTree;
using hyprvx::platform::stockBuildCompatible;
using hyprvx::platform::SupportedHyprlandCommit;

namespace {

constexpr const char* kAbi = "efb50993780079460b0cbed1363e2166a2de1d9f_aq_0.15_hu_0.14_hg_0.5_hc_0.1_hlg_0.6";

} // namespace

TEST(BuildIdentityTest, ClassifiesGitDirtyStrings) {
    EXPECT_EQ(classifyBuildTree("clean"), BuildTree::UnmodifiedUpstream);          // upstream CMake, distro package
    EXPECT_EQ(classifyBuildTree("clean-upstream"), BuildTree::UnmodifiedUpstream); // HyprVX stock prefix
    EXPECT_EQ(classifyBuildTree("hyprvx-d291f743b864"), BuildTree::HyprvxPatched); // HyprVX patched prefix
    EXPECT_EQ(classifyBuildTree("dirty"), BuildTree::Modified);
    EXPECT_EQ(classifyBuildTree("unknown"), BuildTree::Unknown); // built without git and without $GIT_DIRTY
    EXPECT_EQ(classifyBuildTree(""), BuildTree::Unknown);
    EXPECT_EQ(classifyBuildTree("Clean"), BuildTree::Unknown);
    EXPECT_EQ(classifyBuildTree("clean "), BuildTree::Unknown);
    EXPECT_EQ(classifyBuildTree("cleanup"), BuildTree::Unknown);
}

TEST(BuildIdentityTest, AcceptsTheSameCommitWithEitherCleanLabel) {
    EXPECT_TRUE(stockBuildCompatible(SupportedHyprlandCommit, "clean", kAbi, kAbi));          // the user's host
    EXPECT_TRUE(stockBuildCompatible(SupportedHyprlandCommit, "clean-upstream", kAbi, kAbi)); // HyprVX stock prefix
}

TEST(BuildIdentityTest, RefusesModifiedPatchedOrUnverifiableTrees) {
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "dirty", kAbi, kAbi));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "hyprvx-d291f743b864", kAbi, kAbi));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "unknown", kAbi, kAbi));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "", kAbi, kAbi));
}

TEST(BuildIdentityTest, RefusesAnotherCommit) {
    constexpr const char* other = "0123456789abcdef0123456789abcdef01234567";
    EXPECT_FALSE(stockBuildCompatible(other, "clean", kAbi, kAbi));
    EXPECT_FALSE(stockBuildCompatible("efb50993780079460b0cbed1363e2166a2de1d9", "clean", kAbi, kAbi)); // truncated
    EXPECT_FALSE(stockBuildCompatible("", "clean", kAbi, kAbi));
}

TEST(BuildIdentityTest, RefusesAnAbiHashMismatch) {
    // Same commit and clean tree, but built against other library versions than the plugin.
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "clean",
                                      "efb50993780079460b0cbed1363e2166a2de1d9f_aq_0.16_hu_0.14_hg_0.5_hc_0.1_hlg_0.6", kAbi));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "clean", kAbi,
                                      "efb50993780079460b0cbed1363e2166a2de1d9f_aq_0.15_hu_0.15_hg_0.5_hc_0.1_hlg_0.6"));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "clean", kAbi, ""));
    EXPECT_FALSE(stockBuildCompatible(SupportedHyprlandCommit, "clean", "", ""));
}
