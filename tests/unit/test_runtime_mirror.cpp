#include "hyprvx/hyprland/RuntimeMirror.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace hyprvx::hyprland;

TEST(RuntimeMirrorTest, OpenThenFocusTracksWindowAndFocus) {
    RuntimeMirror mirror;
    mirror.apply(WindowOpened{"0x1", "foot", "terminal", "1", 1});
    mirror.apply(WindowFocused{"0x1"});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->className, "foot");
    EXPECT_EQ(state->title, "terminal");
    EXPECT_EQ(mirror.focusedWindow(), "1");
}

TEST(RuntimeMirrorTest, MoveUpdatesWorkspace) {
    RuntimeMirror mirror;
    mirror.apply(WindowWorkspaceChanged{"0x1", 2, "2"});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->workspaceId, 2);
    EXPECT_EQ(state->workspaceName, "2");
    EXPECT_EQ(mirror.knownWorkspaces().at(2), "2");
}

TEST(RuntimeMirrorTest, TitleUpdateCanCreatePartialWindow) {
    RuntimeMirror mirror;
    mirror.apply(WindowTitleChanged{"0x1", "renamed"});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->title, "renamed");
    EXPECT_FALSE(state->className);
    EXPECT_FALSE(state->workspaceId);
}

TEST(RuntimeMirrorTest, FloatingUpdateCanCreatePartialWindow) {
    RuntimeMirror mirror;
    mirror.apply(WindowFloatingChanged{"0x1", true});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->floating, true);
    EXPECT_FALSE(state->title);
}

TEST(RuntimeMirrorTest, CloseRemovesWindowAndFocus) {
    RuntimeMirror mirror;
    mirror.apply(WindowFocused{"0x1"});
    mirror.apply(WindowClosed{"0x1"});
    EXPECT_EQ(mirror.findWindow("0x1"), nullptr);
    EXPECT_FALSE(mirror.focusedWindow());
}

TEST(RuntimeMirrorTest, UnknownFocusCreatesOnlyAddressPartial) {
    RuntimeMirror mirror;
    mirror.apply(WindowFocused{"0xABCD"});
    const auto* state = mirror.findWindow("abcd");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->address, "abcd");
    EXPECT_FALSE(state->className);
    EXPECT_FALSE(state->title);
    EXPECT_FALSE(state->workspaceName);
    EXPECT_EQ(mirror.focusedWindow(), "abcd");
}

TEST(RuntimeMirrorTest, UnknownMoveCreatesWorkspaceOnlyPartial) {
    RuntimeMirror mirror;
    mirror.apply(WindowWorkspaceChanged{"0xunknown", 7, "workspace, seven"});
    const auto* state = mirror.findWindow("0xunknown");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->workspaceId, 7);
    EXPECT_EQ(state->workspaceName, "workspace, seven");
    EXPECT_FALSE(state->title);
}

TEST(RuntimeMirrorTest, OpenEnrichesExistingPartialState) {
    RuntimeMirror mirror;
    mirror.apply(WindowFocused{"0x1"});
    mirror.apply(WindowTitleChanged{"0x1", "partial title"});
    mirror.apply(WindowOpened{"0x1", "thunar", "home", "1", 1});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->className, "thunar");
    EXPECT_EQ(state->title, "partial title");
    EXPECT_EQ(state->workspaceId, 1);
    EXPECT_EQ(mirror.focusedWindow(), "1");
}

TEST(RuntimeMirrorTest, OpenPreservesNewerPartialWorkspaceMetadata) {
    RuntimeMirror mirror;
    mirror.apply(WindowWorkspaceChanged{"0x1", 4, "4"});
    mirror.apply(WindowOpened{"0x1", "app", "title", "special:chat", std::nullopt});
    const auto* state = mirror.findWindow("0x1");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->workspaceId, 4);
    EXPECT_EQ(state->workspaceName, "4");
}

TEST(RuntimeMirrorTest, RepeatedCloseIsSafe) {
    RuntimeMirror mirror;
    mirror.apply(WindowClosed{"0xmissing"});
    mirror.apply(WindowClosed{"0xmissing"});
    EXPECT_TRUE(mirror.windows().empty());
}

TEST(RuntimeMirrorTest, TracksWorkspaceFocusCreationAndDestruction) {
    RuntimeMirror mirror;
    mirror.apply(WorkspaceCreated{2, "2"});
    mirror.apply(WorkspaceFocused{2, "2"});
    ASSERT_EQ(mirror.knownWorkspaces().at(2), "2");
    EXPECT_EQ(mirror.focusedWorkspaceId(), 2);
    EXPECT_EQ(mirror.focusedWorkspaceName(), "2");
    mirror.apply(WorkspaceDestroyed{2, "2"});
    EXPECT_EQ(mirror.knownWorkspaces().count(2), 0U);
    EXPECT_FALSE(mirror.focusedWorkspaceId());
}

TEST(RuntimeMirrorTest, FocusClearRemovesOnlyFocusedWindowIdentity) {
    RuntimeMirror mirror;
    mirror.apply(WindowOpened{"0x1", "foot", "terminal", "1", 1});
    mirror.apply(WindowFocused{"0x1"});
    mirror.apply(WindowFocusCleared{});
    EXPECT_FALSE(mirror.focusedWindow());
    EXPECT_NE(mirror.findWindow("1"), nullptr);
}

TEST(RuntimeMirrorTest, TitleFloatingAndMoveBeforeOpenArePreserved) {
    RuntimeMirror mirror;
    mirror.apply(WindowTitleChanged{"0xabc", "new title"});
    mirror.apply(WindowFloatingChanged{"abc", true});
    mirror.apply(WindowWorkspaceChanged{"ABC", 7, "special:chat"});
    mirror.apply(WindowOpened{"0xAbC", "app", "stale title", "1", 1});

    ASSERT_EQ(mirror.windows().size(), 1U);
    const auto* state = mirror.findWindow("ABC");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->className, "app");
    EXPECT_EQ(state->title, "new title");
    EXPECT_EQ(state->floating, true);
    EXPECT_EQ(state->workspaceId, 7);
    EXPECT_EQ(state->workspaceName, "special:chat");
}

TEST(RuntimeMirrorTest, ClosedWindowRejectsDelayedMetadataAndFocus) {
    RuntimeMirror mirror;
    mirror.apply(WindowOpened{"55f003ec5bc0", "app", "title", "1", 1});
    mirror.apply(WindowFocused{"55f003ec5bc0"});
    mirror.apply(WindowClosed{"55f003ec5bc0"});
    mirror.apply(WindowTitleChanged{"55f003ec5bc0", "late title"});
    mirror.apply(WindowFloatingChanged{"55f003ec5bc0", true});
    mirror.apply(WindowWorkspaceChanged{"55f003ec5bc0", 2, "2"});
    mirror.apply(WindowFocused{"55f003ec5bc0"});

    EXPECT_EQ(mirror.findWindow("55f003ec5bc0"), nullptr);
    EXPECT_FALSE(mirror.focusedWindow());
}

TEST(RuntimeMirrorTest, RealOpenClearsClosedAddressTombstone) {
    RuntimeMirror mirror;
    mirror.apply(WindowClosed{"0xABC"});
    mirror.apply(WindowTitleChanged{"abc", "late title"});
    EXPECT_EQ(mirror.findWindow("ABC"), nullptr);

    mirror.apply(WindowOpened{"0xAbC", "app", "opened", "special:chat", std::nullopt});
    const auto* state = mirror.findWindow("abc");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->title, "opened");
    EXPECT_EQ(state->workspaceName, "special:chat");
    EXPECT_FALSE(state->workspaceId);
    EXPECT_EQ(mirror.recentlyClosedCount(), 0U);
}

TEST(RuntimeMirrorTest, AddressPrefixesAndHexCaseShareOneIdentity) {
    RuntimeMirror mirror;
    mirror.apply(WindowFocused{"0xABC"});
    mirror.apply(WindowTitleChanged{"abc", "same window"});
    EXPECT_EQ(mirror.windows().size(), 1U);
    EXPECT_EQ(mirror.focusedWindow(), "abc");
    EXPECT_EQ(mirror.findWindow("0xAbC")->title, "same window");
}

TEST(RuntimeMirrorTest, NamedWorkspaceDoesNotInventNumericId) {
    RuntimeMirror mirror;
    mirror.apply(WindowOpened{"0xabc", "app", "title", "special:chat", std::nullopt});
    const auto* state = mirror.findWindow("abc");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->workspaceName, "special:chat");
    EXPECT_FALSE(state->workspaceId);
}

TEST(RuntimeMirrorTest, RecentlyClosedBufferIsBounded) {
    RuntimeMirror mirror;
    for (std::size_t index = 0; index < RuntimeMirror::RecentlyClosedLimit + 32; ++index) {
        mirror.apply(WindowClosed{"dead" + std::to_string(index)});
    }
    EXPECT_EQ(mirror.recentlyClosedCount(), RuntimeMirror::RecentlyClosedLimit);
}

TEST(RuntimeMirrorTest, ResetClearsObservedStateAndReconnectTombstones) {
    RuntimeMirror mirror;
    mirror.apply(WindowClosed{"0xabc"});
    mirror.apply(WorkspaceFocused{1, "1"});
    mirror.apply(WindowOpened{"0xdef", "app", "title", "1", 1});
    mirror.reset();
    EXPECT_TRUE(mirror.windows().empty());
    EXPECT_TRUE(mirror.knownWorkspaces().empty());
    EXPECT_FALSE(mirror.focusedWindow());
    EXPECT_FALSE(mirror.focusedWorkspaceId());
    EXPECT_EQ(mirror.recentlyClosedCount(), 0U);

    mirror.apply(WindowTitleChanged{"0xabc", "new connection observation"});
    EXPECT_NE(mirror.findWindow("abc"), nullptr);
}
