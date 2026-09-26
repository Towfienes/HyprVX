#include "hyprvx/hyprland/EventParser.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

using hyprvx::hyprland::ActiveLayoutEvent;
using hyprvx::hyprland::ActiveWindowEvent;
using hyprvx::hyprland::ActiveWindowV2Event;
using hyprvx::hyprland::ChangeFloatingModeEvent;
using hyprvx::hyprland::CloseWindowEvent;
using hyprvx::hyprland::CreateWorkspaceEvent;
using hyprvx::hyprland::CreateWorkspaceV2Event;
using hyprvx::hyprland::DestroyWorkspaceEvent;
using hyprvx::hyprland::DestroyWorkspaceV2Event;
using hyprvx::hyprland::EventParser;
using hyprvx::hyprland::MoveWindowEvent;
using hyprvx::hyprland::MoveWindowV2Event;
using hyprvx::hyprland::OpenWindowEvent;
using hyprvx::hyprland::UnknownEvent;
using hyprvx::hyprland::WindowTitleEvent;
using hyprvx::hyprland::WindowTitleV2Event;
using hyprvx::hyprland::WorkspaceEvent;
using hyprvx::hyprland::WorkspaceV2Event;

TEST(HyprlandEventParserTest, ParsesActiveWindowAndPreservesTitleCommas) {
    const auto event = EventParser::parse("activewindow>>kitty,first, second");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<ActiveWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->className, "kitty");
    EXPECT_EQ(parsed->title, "first, second");
}

TEST(HyprlandEventParserTest, ParsesTimestampPrefixedActiveWindow) {
    const auto event = EventParser::parse("[09:08:07] activewindow>>firefox,Docs");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<ActiveWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->className, "firefox");
    EXPECT_EQ(parsed->title, "Docs");
}

TEST(HyprlandEventParserTest, ParsesActiveWindowV2) {
    const auto event = EventParser::parse("activewindowv2>>0x123abc");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<ActiveWindowV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "0x123abc");
}

TEST(HyprlandEventParserTest, ParsesActiveLayoutAndPreservesLayoutCommas) {
    const auto event = EventParser::parse("activelayout>>kbd-us,English, US");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<ActiveLayoutEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->keyboardName, "kbd-us");
    EXPECT_EQ(parsed->layoutName, "English, US");
}

TEST(HyprlandEventParserTest, ParsesFloatingModeAsTypedBoolean) {
    const auto event = EventParser::parse("changefloatingmode>>1,0xabc");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<ChangeFloatingModeEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_TRUE(parsed->floating);
    EXPECT_EQ(parsed->address, "0xabc");
}

TEST(HyprlandEventParserTest, RejectsUnknownFloatingModeValue) {
    EXPECT_FALSE(EventParser::parse("changefloatingmode>>yes,0xabc"));
}

TEST(HyprlandEventParserTest, ParsesWorkspaceName) {
    const auto event = EventParser::parse("workspace>>special:chat");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<WorkspaceEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "special:chat");
}

TEST(HyprlandEventParserTest, ParsesWorkspaceV2IdAndName) {
    const auto event = EventParser::parse("workspacev2>>-2,special:chat");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<WorkspaceV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->id, -2);
    EXPECT_EQ(parsed->name, "special:chat");
}

TEST(HyprlandEventParserTest, RejectsMalformedWorkspaceV2Id) {
    EXPECT_FALSE(EventParser::parse("workspacev2>>2x,work"));
}

TEST(HyprlandEventParserTest, ParsesWindowTitleAddress) {
    const auto event = EventParser::parse("windowtitle>>0xff00");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<WindowTitleEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "0xff00");
}

TEST(HyprlandEventParserTest, ParsesWindowTitleV2AndPreservesTitleCommas) {
    const auto event = EventParser::parse("windowtitlev2>>0xff00,Report, final");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<WindowTitleV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "0xff00");
    EXPECT_EQ(parsed->title, "Report, final");
}

TEST(HyprlandEventParserTest, PreservesUnknownValidEventAndPayload) {
    const auto event = EventParser::parse("futureevent>>alpha,beta>>gamma");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "futureevent");
    EXPECT_EQ(parsed->payload, "alpha,beta>>gamma");
}

TEST(HyprlandEventParserTest, RejectsMalformedLinesAndInvalidProbePrefix) {
    EXPECT_FALSE(EventParser::parse("activewindow kitty,title"));
    EXPECT_FALSE(EventParser::parse("[yesterday] workspace>>1"));
    EXPECT_FALSE(EventParser::parse(">>payload"));
}

TEST(HyprlandEventParserLifecycleTest, ParsesBasicOpenWindow) {
    const auto event = EventParser::parse("openwindow>>0xabc,2,foot,terminal");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<OpenWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "0xabc");
    EXPECT_EQ(parsed->workspace, "2");
    EXPECT_EQ(parsed->className, "foot");
    EXPECT_EQ(parsed->title, "terminal");
}

TEST(HyprlandEventParserLifecycleTest, ParsesCapturedThunarOpenWindow) {
    const auto event = EventParser::parse(
        "openwindow>>55f003fc2bd0,1,thunar,towfienes - Thunar");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<OpenWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fc2bd0");
    EXPECT_EQ(parsed->workspace, "1");
    EXPECT_EQ(parsed->className, "thunar");
    EXPECT_EQ(parsed->title, "towfienes - Thunar");
}

TEST(HyprlandEventParserLifecycleTest, PreservesCommasInOpenWindowTitle) {
    const auto event = EventParser::parse("openwindow>>0xabc,1,app,one, two,three");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<OpenWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->title, "one, two,three");
}

TEST(HyprlandEventParserLifecycleTest, ParsesTimestampPrefixedOpenWindow) {
    const auto event = EventParser::parse(
        "[12:34:56] openwindow>>55f003fb29a0,1,foot,foot");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<OpenWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fb29a0");
    EXPECT_EQ(parsed->title, "foot");
}

TEST(HyprlandEventParserLifecycleTest, PreservesMalformedOpenWindowAsUnknown) {
    constexpr auto line = "openwindow>>0xabc,1,foot";
    const auto event = EventParser::parse(line);
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "openwindow");
    EXPECT_EQ(parsed->payload, "0xabc,1,foot");
}

TEST(HyprlandEventParserLifecycleTest, ParsesBasicCloseWindow) {
    const auto event = EventParser::parse("closewindow>>55f003fc2bd0");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<CloseWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fc2bd0");
}

TEST(HyprlandEventParserLifecycleTest, ParsesTimestampPrefixedCloseWindow) {
    const auto event = EventParser::parse("[08:07:06] closewindow>>55f003fb29a0");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<CloseWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fb29a0");
}

TEST(HyprlandEventParserLifecycleTest, KeepsUncapturedUrgentEventUnknown) {
    const auto event = EventParser::parse("urgent>>55f003fc2bd0");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "urgent");
    EXPECT_EQ(parsed->payload, "55f003fc2bd0");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicMoveWindow) {
    const auto event = EventParser::parse("movewindow>>55f003fc1510,2");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<MoveWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fc1510");
    EXPECT_EQ(parsed->workspace, "2");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesTimestampPrefixedMoveWindow) {
    const auto event = EventParser::parse("[10:11:12] movewindow>>55f003f27150,4");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<MoveWindowEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003f27150");
    EXPECT_EQ(parsed->workspace, "4");
}

TEST(HyprlandEventParserWorkspaceTest, PreservesMalformedMoveWindowAsUnknown) {
    const auto event = EventParser::parse("movewindow>>55f003fc1510");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "movewindow");
    EXPECT_EQ(parsed->payload, "55f003fc1510");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicMoveWindowV2) {
    const auto event = EventParser::parse("movewindowv2>>55f003fc1510,2,2");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<MoveWindowV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->address, "55f003fc1510");
    EXPECT_EQ(parsed->workspaceId, 2);
    EXPECT_EQ(parsed->workspaceName, "2");
}

TEST(HyprlandEventParserWorkspaceTest, MoveWindowV2PreservesRemainingWorkspaceName) {
    const auto event = EventParser::parse("movewindowv2>>0xabc,7,desk, development");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<MoveWindowV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspaceId, 7);
    EXPECT_EQ(parsed->workspaceName, "desk, development");
}

TEST(HyprlandEventParserWorkspaceTest, PreservesMalformedMoveWindowV2AsUnknown) {
    const auto event = EventParser::parse("movewindowv2>>0xabc,not-an-id,desk");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "movewindowv2");
    EXPECT_EQ(parsed->payload, "0xabc,not-an-id,desk");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicCreateWorkspace) {
    const auto event = EventParser::parse("createworkspace>>4");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<CreateWorkspaceEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspace, "4");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicCreateWorkspaceV2) {
    const auto event = EventParser::parse("createworkspacev2>>4,4");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<CreateWorkspaceV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspaceId, 4);
    EXPECT_EQ(parsed->workspaceName, "4");
}

TEST(HyprlandEventParserWorkspaceTest, CreateWorkspaceV2PreservesRemainingWorkspaceName) {
    const auto event = EventParser::parse("createworkspacev2>>8,work, docs");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<CreateWorkspaceV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspaceId, 8);
    EXPECT_EQ(parsed->workspaceName, "work, docs");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicDestroyWorkspace) {
    const auto event = EventParser::parse("destroyworkspace>>5");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<DestroyWorkspaceEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspace, "5");
}

TEST(HyprlandEventParserWorkspaceTest, ParsesBasicDestroyWorkspaceV2) {
    const auto event = EventParser::parse("destroyworkspacev2>>5,5");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<DestroyWorkspaceV2Event>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->workspaceId, 5);
    EXPECT_EQ(parsed->workspaceName, "5");
}

TEST(HyprlandEventParserWorkspaceTest, KeepsUncapturedMonitorEventUnknown) {
    const auto event = EventParser::parse("monitoraddedv2>>DP-1,2560x1440@144,0x0");
    ASSERT_TRUE(event);
    const auto* parsed = std::get_if<UnknownEvent>(&*event);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "monitoraddedv2");
    EXPECT_EQ(parsed->payload, "DP-1,2560x1440@144,0x0");
}
