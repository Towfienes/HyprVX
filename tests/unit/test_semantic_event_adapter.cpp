#include "hyprvx/hyprland/SemanticEventAdapter.hpp"

#include <gtest/gtest.h>

#include <variant>

using namespace hyprvx::hyprland;

namespace {

Event parseEvent(const char* line) {
    const auto parsed = EventParser::parse(line);
    EXPECT_TRUE(parsed.has_value());
    return parsed.value_or(UnknownEvent{"test-invalid", line});
}

} // namespace

TEST(SemanticEventAdapterTest, MapsFocusedWindowV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("activewindowv2>>0xabc"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowFocused>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->address, "0xabc");
}

TEST(SemanticEventAdapterTest, MapsEmptyActiveWindowV2ToFocusCleared) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("activewindowv2>>"));
    ASSERT_TRUE(event);
    EXPECT_TRUE(std::holds_alternative<WindowFocusCleared>(*event));
}

TEST(SemanticEventAdapterTest, MapsWindowOpenWithStructuredMetadata) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("openwindow>>0xabc,2,foot,terminal"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowOpened>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->address, "0xabc");
    EXPECT_EQ(value->className, "foot");
    EXPECT_EQ(value->title, "terminal");
    EXPECT_EQ(value->workspaceName, "2");
    EXPECT_FALSE(value->workspaceId);
}

TEST(SemanticEventAdapterTest, DoesNotFabricateNumericWorkspaceForNamedWorkspace) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("openwindow>>0xabc,special:chat,app,title"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowOpened>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->workspaceName, "special:chat");
    EXPECT_FALSE(value->workspaceId);
}

TEST(SemanticEventAdapterTest, MapsWindowClose) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("closewindow>>0xabc"));
    ASSERT_TRUE(event);
    EXPECT_EQ(std::get<WindowClosed>(*event).address, "0xabc");
}

TEST(SemanticEventAdapterTest, MapsWindowMoveV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("movewindowv2>>0xabc,3,work, docs"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowWorkspaceChanged>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->address, "0xabc");
    EXPECT_EQ(value->workspaceId, 3);
    EXPECT_EQ(value->workspaceName, "work, docs");
}

TEST(SemanticEventAdapterTest, MapsWindowTitleV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("windowtitlev2>>0xabc,New, title"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowTitleChanged>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->address, "0xabc");
    EXPECT_EQ(value->title, "New, title");
}

TEST(SemanticEventAdapterTest, MapsFloatingMode) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("changefloatingmode>>55f003ec5bc0,1"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WindowFloatingChanged>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->address, "55f003ec5bc0");
    EXPECT_TRUE(value->floating);
}

TEST(SemanticEventAdapterTest, MapsFocusedWorkspaceV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("workspacev2>>-1,special:chat"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WorkspaceFocused>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->workspaceId, -1);
    EXPECT_EQ(value->workspaceName, "special:chat");
}

TEST(SemanticEventAdapterTest, MapsWorkspaceCreatedV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("createworkspacev2>>4,4"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WorkspaceCreated>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->workspaceId, 4);
    EXPECT_EQ(value->workspaceName, "4");
}

TEST(SemanticEventAdapterTest, MapsWorkspaceDestroyedV2) {
    SemanticEventAdapter adapter;
    const auto event = adapter.adapt(parseEvent("destroyworkspacev2>>4,4"));
    ASSERT_TRUE(event);
    const auto* value = std::get_if<WorkspaceDestroyed>(&*event);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->workspaceId, 4);
    EXPECT_EQ(value->workspaceName, "4");
}

TEST(SemanticEventAdapterTest, IgnoresLegacyEventsWithV2Equivalents) {
    SemanticEventAdapter adapter;
    for (const auto* line : {
             "activewindow>>app,title",
             "movewindow>>0xabc,2",
             "workspace>>2",
             "createworkspace>>2",
             "destroyworkspace>>2",
             "windowtitle>>0xabc"}) {
        const auto semantic = adapter.adapt(parseEvent(line));
        EXPECT_FALSE(semantic) << line;
    }
}

TEST(SemanticEventAdapterTest, IgnoresUnsupportedAndUnknownEvents) {
    SemanticEventAdapter adapter;
    EXPECT_FALSE(adapter.adapt(parseEvent("activelayout>>kbd,English")));
    EXPECT_FALSE(adapter.adapt(parseEvent("monitoraddedv2>>DP-1,mode,position")));
}
