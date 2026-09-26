#include "hyprvx/hyprland/EventParser.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

using hyprvx::hyprland::ActiveLayoutEvent;
using hyprvx::hyprland::ActiveWindowEvent;
using hyprvx::hyprland::ActiveWindowV2Event;
using hyprvx::hyprland::ChangeFloatingModeEvent;
using hyprvx::hyprland::EventParser;
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
