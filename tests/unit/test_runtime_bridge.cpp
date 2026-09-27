#include "hyprvx/hyprland/RuntimeBridge.hpp"

#include <gtest/gtest.h>

#include <variant>
#include <vector>

using namespace hyprvx::hyprland;

TEST(RuntimeBridgeTest, AppliesCapturedEventsAndSuppressesLegacyDuplicates) {
    RuntimeBridge bridge;
    const std::vector<std::string> lines = {
        "[12:00:00] openwindow>>55f003fc2bd0,1,thunar,Home",
        "activewindow>>thunar,Home",
        "[12:00:01] activewindowv2>>55f003fc2bd0",
        "movewindow>>55f003fc2bd0,2",
        "[12:00:02] movewindowv2>>55f003fc2bd0,2,2",
        "workspace>>2",
        "workspacev2>>2,2",
        "windowtitle>>55f003fc2bd0",
        "windowtitlev2>>55f003fc2bd0,Home, Documents",
        "changefloatingmode>>55f003fc2bd0,1",
        "createworkspace>>3",
        "createworkspacev2>>3,3",
        "destroyworkspace>>1",
        "destroyworkspacev2>>1,1",
        "activelayout>>kbd,English",
        "monitoraddedv2>>DP-1,mode,position"};

    std::vector<SemanticEvent> semanticEvents;
    for (const auto& line : lines) {
        if (auto event = bridge.processLine(line)) {
            semanticEvents.push_back(*event);
        }
    }

    ASSERT_EQ(semanticEvents.size(), 8U);
    EXPECT_TRUE(std::holds_alternative<WindowOpened>(semanticEvents[0]));
    EXPECT_TRUE(std::holds_alternative<WindowFocused>(semanticEvents[1]));
    EXPECT_TRUE(std::holds_alternative<WindowWorkspaceChanged>(semanticEvents[2]));
    EXPECT_TRUE(std::holds_alternative<WorkspaceFocused>(semanticEvents[3]));
    EXPECT_TRUE(std::holds_alternative<WindowTitleChanged>(semanticEvents[4]));
    EXPECT_TRUE(std::holds_alternative<WindowFloatingChanged>(semanticEvents[5]));
    EXPECT_TRUE(std::holds_alternative<WorkspaceCreated>(semanticEvents[6]));
    EXPECT_TRUE(std::holds_alternative<WorkspaceDestroyed>(semanticEvents[7]));

    const auto* window = bridge.mirror().findWindow("55f003fc2bd0");
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->className, "thunar");
    EXPECT_EQ(window->title, "Home, Documents");
    EXPECT_EQ(window->workspaceId, 2);
    EXPECT_EQ(window->workspaceName, "2");
    EXPECT_EQ(window->floating, true);
    EXPECT_EQ(bridge.mirror().focusedWindow(), "55f003fc2bd0");
    EXPECT_EQ(bridge.mirror().focusedWorkspaceId(), 2);
    EXPECT_EQ(bridge.mirror().knownWorkspaces().count(3), 1U);
    EXPECT_EQ(bridge.mirror().knownWorkspaces().count(1), 0U);
}

TEST(RuntimeBridgeTest, MalformedAndUnsupportedLinesDoNotMutateMirror) {
    RuntimeBridge bridge;
    EXPECT_FALSE(bridge.processLine("malformed without delimiter"));
    EXPECT_FALSE(bridge.processLine("activelayout>>kbd,English"));
    EXPECT_FALSE(bridge.processLine("monitorremovedv2>>DP-1"));
    EXPECT_TRUE(bridge.mirror().windows().empty());
    EXPECT_TRUE(bridge.mirror().knownWorkspaces().empty());
}

TEST(RuntimeBridgeTest, ReportsReadableSemanticDescriptions) {
    const SemanticEvent moved = WindowWorkspaceChanged{"0x1", 2, "2"};
    EXPECT_EQ(describe(moved), "WindowMoved 0x1 workspace=2");
}

TEST(RuntimeBridgeTest, EmptyV2ActiveWindowClearsFocusWithoutRemovingWindow) {
    RuntimeBridge bridge;
    ASSERT_TRUE(bridge.processLine("openwindow>>abc,1,app,title"));
    ASSERT_TRUE(bridge.processLine("activewindowv2>>abc"));
    EXPECT_EQ(bridge.mirror().focusedWindow(), "abc");
    const auto cleared = bridge.processLine("activewindowv2>>");
    ASSERT_TRUE(cleared);
    EXPECT_TRUE(std::holds_alternative<WindowFocusCleared>(*cleared));
    EXPECT_FALSE(bridge.mirror().focusedWindow());
    EXPECT_NE(bridge.mirror().findWindow("abc"), nullptr);
}

TEST(RuntimeBridgeTest, ResetProvidesCleanReconnectEpoch) {
    RuntimeBridge bridge;
    ASSERT_TRUE(bridge.processLine("closewindow>>abc"));
    EXPECT_FALSE(bridge.processLine("windowtitlev2>>abc,title"));
    EXPECT_EQ(bridge.mirror().findWindow("abc"), nullptr);
    bridge.reset();
    ASSERT_TRUE(bridge.processLine("windowtitlev2>>abc,title after reconnect"));
    EXPECT_NE(bridge.mirror().findWindow("abc"), nullptr);
}

TEST(RuntimeBridgeTest, DisconnectEndsEpochAndReconnectClearsOldTombstones) {
    RuntimeBridge bridge;
    const auto firstEpoch = bridge.connectionEpoch();
    ASSERT_TRUE(bridge.processLine("closewindow>>abc"));
    bridge.endEpoch();
    EXPECT_FALSE(bridge.epochActive());
    EXPECT_FALSE(bridge.processLine("windowtitlev2>>abc,ignored while disconnected"));
    bridge.beginEpoch();
    EXPECT_TRUE(bridge.epochActive());
    EXPECT_GT(bridge.connectionEpoch(), firstEpoch);
    ASSERT_TRUE(bridge.processLine("windowtitlev2>>abc,new epoch title"));
    EXPECT_NE(bridge.mirror().findWindow("abc"), nullptr);
}
