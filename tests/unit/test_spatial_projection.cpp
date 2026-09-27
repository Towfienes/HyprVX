#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/core/Camera.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace hyprvx;
using namespace hyprvx::runtime;
using namespace hyprvx::hyprland;

namespace {
void apply(RuntimeMirror& mirror, const SemanticEvent& event, SpatialProjector& projector, World& world) {
    mirror.apply(event);
    projector.reconcile(mirror, world);
}
}

TEST(SpatialProjectionTest, DefersMetadataOnlyPartialUntilWorkspaceIsKnown) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    apply(mirror, WindowTitleChanged{"0xABC", "partial"}, projector, world);
    const auto* record = projector.findWindow("abc");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->placementStatus, PlacementStatus::DeferredMissingWorkspace);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    apply(mirror, WindowOpened{"abc", "app", "stale", "1", std::nullopt}, projector, world);
    ASSERT_EQ(world.freeWindowCount(), 1U);
    EXPECT_EQ(projector.findWindow("0xabc")->title, "partial");
}

TEST(SpatialProjectionTest, CanonicalAddressesMapOnceAndDifferentAddressesRemainDistinct) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"0xABC", "a", "one", "1", 1});
    mirror.apply(WindowOpened{"def", "b", "two", "1", 1});
    projector.reconcile(mirror, world);
    const auto first = projector.mappedId("abc");
    ASSERT_TRUE(first);
    EXPECT_EQ(first, projector.mappedId("0xAbC"));
    EXPECT_NE(first, projector.mappedId("DEF"));
    EXPECT_EQ(world.freeWindowCount(), 2U);
}

TEST(SpatialProjectionTest, CloseRemovesMappingAndWorldWindowAndRepeatedCloseIsSafe) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"abc", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    const auto oldId = projector.mappedId("abc");
    mirror.apply(WindowClosed{"abc"});
    projector.reconcile(mirror, world);
    EXPECT_EQ(projector.mappedId("abc"), std::nullopt);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    mirror.apply(WindowClosed{"abc"});
    projector.reconcile(mirror, world);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    mirror.apply(WindowOpened{"abc", "app", "again", "1", 1});
    projector.reconcile(mirror, world);
    EXPECT_NE(projector.mappedId("abc"), oldId);
}

TEST(SpatialProjectionTest, RepeatedOpenDoesNotDuplicateWindow) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    const WindowOpened opened{"abc", "app", "title", "1", 1};
    mirror.apply(opened);
    projector.reconcile(mirror, world);
    const auto id = projector.mappedId("abc");
    mirror.apply(opened);
    projector.reconcile(mirror, world);
    EXPECT_EQ(world.freeWindowCount(), 1U);
    EXPECT_EQ(projector.mappedId("abc"), id);
}

TEST(SpatialProjectionTest, WorkspaceKindsHaveStableDistinctNonOverlappingRegions) {
    WorkspaceSpatialMap map;
    const auto numeric = map.resolve("2", 2);
    const auto named = map.resolve("dev", std::nullopt);
    const auto special = map.resolve("special:chat", std::nullopt);
    EXPECT_DOUBLE_EQ(map.resolve("2", 2).origin.x, numeric.origin.x);
    EXPECT_DOUBLE_EQ(map.resolve("2", 2).origin.y, numeric.origin.y);
    EXPECT_NE(numeric.origin.x, named.origin.x);
    EXPECT_NE(named.origin.x, special.origin.x);
    EXPECT_LE(numeric.bounds.right(), named.bounds.position.x);
    EXPECT_LE(named.bounds.right(), special.bounds.position.x);
    EXPECT_EQ(named.name, "dev");
    EXPECT_TRUE(std::isfinite(named.bounds.position.y));
    EXPECT_TRUE(std::isfinite(named.bounds.size.y));
    EXPECT_GT(named.bounds.size.y, 0.0);
    EXPECT_DOUBLE_EQ(named.bounds.size.x, 3600.0);
    EXPECT_DOUBLE_EQ(named.bounds.size.y, 2500.0);
}

TEST(SpatialProjectionTest, InitialPlacementPolicyRejectsInvalidInputs) {
    InitialPlacementPolicy policy;
    policy.windowSize.x = 0.0;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.windowSize.y = -1.0;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.columns = 0;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.gap = -0.1;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.firstWorkspaceOrigin.x = std::numeric_limits<double>::infinity();
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.firstWorkspaceOrigin.y = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.workspaceStride = 3399.0;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);

    policy = {};
    policy.initialRegionRows = 0;
    EXPECT_THROW(static_cast<void>(SpatialProjector{policy}), std::invalid_argument);
}

TEST(SpatialProjectionTest, WorkspaceSpatialMapRejectsEmptyOrWhitespaceIdentity) {
    WorkspaceSpatialMap map;
    EXPECT_THROW(static_cast<void>(map.resolve("")), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(map.resolve(" \t\n")), std::invalid_argument);
    const std::string embeddedNull("bad\0name", 8);
    EXPECT_THROW(static_cast<void>(map.resolve(embeddedNull)), std::invalid_argument);
}

TEST(SpatialProjectionTest, WorkspaceRegionsRemainStableAcrossUnrelatedLifecycle) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "a", "special:chat", std::nullopt});
    projector.reconcile(mirror, world);
    const auto before = projector.workspaceRegion("special:chat")->origin;
    mirror.apply(WindowOpened{"b", "app", "b", "1", 1});
    projector.reconcile(mirror, world);
    mirror.apply(WindowClosed{"b"});
    projector.reconcile(mirror, world);
    EXPECT_DOUBLE_EQ(projector.workspaceRegion("special:chat")->origin.x, before.x);
    EXPECT_DOUBLE_EQ(projector.workspaceRegion("special:chat")->origin.y, before.y);
}

TEST(SpatialProjectionTest, DefaultPlacementSeparatesWindowsAndWorkspaces) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "a", "1", 1});
    mirror.apply(WindowOpened{"b", "app", "b", "1", 1});
    mirror.apply(WindowOpened{"c", "app", "c", "2", 2});
    projector.reconcile(mirror, world);
    const Rect a = *projector.geometryForAddress("a");
    const Rect b = *projector.geometryForAddress("b");
    const Rect c = *projector.geometryForAddress("c");
    EXPECT_DOUBLE_EQ(a.size.x, 1000.0);
    EXPECT_DOUBLE_EQ(a.size.y, 700.0);
    EXPECT_NE(a.position.x, b.position.x);
    EXPECT_TRUE(a.right() <= c.left() || c.right() <= a.left());
    EXPECT_EQ(world.freeWindowCount(), 3U);
}

TEST(SpatialProjectionTest, ConfigurablePlacementControlsSizeStrideAndOrigin) {
    InitialPlacementPolicy policy;
    policy.windowSize = {800.0, 500.0};
    policy.workspaceStride = 2600.0;
    policy.columns = 2;
    policy.firstWorkspaceOrigin = {-5000.0, 250.0};
    SpatialProjector projector(policy);
    RuntimeMirror mirror;
    World world;
    mirror.apply(WindowOpened{"a", "app", "a", "named", std::nullopt});
    mirror.apply(WindowOpened{"b", "app", "b", "other", std::nullopt});
    projector.reconcile(mirror, world);
    const auto a = projector.geometryForAddress("a");
    const auto b = projector.geometryForAddress("b");
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_DOUBLE_EQ(a->size.x, 800.0);
    EXPECT_DOUBLE_EQ(a->size.y, 500.0);
    EXPECT_DOUBLE_EQ(projector.workspaceRegion("named")->origin.x, -5000.0);
    EXPECT_DOUBLE_EQ(projector.workspaceRegion("other")->origin.x, -2400.0);
}

TEST(SpatialProjectionTest, MultiRowGridPlacementIsExactDeterministicAndNonOverlapping) {
    InitialPlacementPolicy policy;
    policy.windowSize = {800.0, 500.0};
    policy.workspaceStride = 2000.0;
    policy.columns = 2;
    policy.initialRegionRows = 1;
    policy.gap = 200.0;
    SpatialProjector projector(policy);
    RuntimeMirror mirror;
    World world;
    for (const std::string address : {"a", "b", "c", "d", "e"}) {
        mirror.apply(WindowOpened{address, "app", address, "workspace", std::nullopt});
    }
    projector.reconcile(mirror, world);

    const std::array<Vec2, 5> expected{{
        {0.0, 0.0}, {1000.0, 0.0}, {0.0, 700.0}, {1000.0, 700.0}, {0.0, 1400.0}}};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto geometry = projector.geometryForAddress(std::string(1, static_cast<char>('a' + index)));
        ASSERT_TRUE(geometry);
        EXPECT_DOUBLE_EQ(geometry->position.x, expected[index].x);
        EXPECT_DOUBLE_EQ(geometry->position.y, expected[index].y);
        EXPECT_DOUBLE_EQ(geometry->size.x, 800.0);
        EXPECT_DOUBLE_EQ(geometry->size.y, 500.0);
    }

    for (char lhs = 'a'; lhs <= 'e'; ++lhs) {
        const Rect a = *projector.geometryForAddress(std::string(1, lhs));
        for (char rhs = static_cast<char>(lhs + 1); rhs <= 'e'; ++rhs) {
            const Rect b = *projector.geometryForAddress(std::string(1, rhs));
            const bool separated = a.right() <= b.left() || b.right() <= a.left() ||
                a.bottom() <= b.top() || b.bottom() <= a.top();
            EXPECT_TRUE(separated) << lhs << " overlaps " << rhs;
        }
    }
    const auto region = projector.workspaceRegion("workspace");
    ASSERT_TRUE(region);
    EXPECT_DOUBLE_EQ(region->bounds.size.y, 1900.0);
}

TEST(SpatialProjectionTest, SameEventReplayProducesDeterministicFinalProjection) {
    const auto run = [] {
        RuntimeMirror mirror;
        SpatialProjector projector;
        World world;
        mirror.apply(WindowOpened{"b", "app", "b", "2", 2});
        mirror.apply(WindowOpened{"a", "app", "a", "1", 1});
        projector.reconcile(mirror, world);
        const auto id = projector.mappedId("a");
        const auto geometry = projector.geometryForAddress("b");
        return std::pair{id, std::array<double, 4>{geometry->position.x, geometry->position.y, geometry->size.x, geometry->size.y}};
    };
    EXPECT_EQ(run(), run());
}

TEST(SpatialProjectionTest, WorkspaceMoveRelocatesGeometryAndMetadata) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    const Rect before = *projector.geometryForAddress("a");
    mirror.apply(WindowWorkspaceChanged{"a", 99, "special:chat"});
    projector.reconcile(mirror, world);
    const auto* moved = projector.findWindow("a");
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(moved->workspaceName, "special:chat");
    EXPECT_EQ(moved->workspaceId, 99);
    EXPECT_NE(moved->geometry.position.x, before.position.x);
    const auto worldGeometry = world.windowGeometry(moved->windowId);
    ASSERT_TRUE(worldGeometry);
    EXPECT_DOUBLE_EQ(worldGeometry->position.x, moved->geometry.position.x);
    EXPECT_DOUBLE_EQ(worldGeometry->position.y, moved->geometry.position.y);
}

TEST(SpatialProjectionTest, MetadataChangesDoNotAlterGeometry) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    const auto before = projector.geometryForAddress("a");
    mirror.apply(WindowTitleChanged{"a", "new,title"});
    mirror.apply(WindowFloatingChanged{"a", true});
    projector.reconcile(mirror, world);
    const auto* record = projector.findWindow("a");
    EXPECT_EQ(record->title, "new,title");
    EXPECT_EQ(record->floating, true);
    const auto after = projector.geometryForAddress("a");
    ASSERT_TRUE(before);
    ASSERT_TRUE(after);
    EXPECT_DOUBLE_EQ(after->position.x, before->position.x);
    EXPECT_DOUBLE_EQ(after->position.y, before->position.y);
    EXPECT_DOUBLE_EQ(after->size.x, before->size.x);
    EXPECT_DOUBLE_EQ(after->size.y, before->size.y);
}

TEST(SpatialProjectionTest, FocusAndFocusLossMapToProjectedWindowId) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    mirror.apply(WindowFocused{"0xA"});
    projector.reconcile(mirror, world);
    EXPECT_EQ(projector.focusedWindowId(), projector.mappedId("a"));
    mirror.apply(WindowFocusCleared{});
    projector.reconcile(mirror, world);
    EXPECT_FALSE(projector.focusedWindowId());
}

TEST(SpatialProjectionTest, MoveBeforeOpenAndFocusBeforeOpenEnrichWithoutDuplicate) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowWorkspaceChanged{"a", 7, "dev"});
    mirror.apply(WindowFocused{"a"});
    projector.reconcile(mirror, world);
    ASSERT_EQ(world.freeWindowCount(), 1U);
    const auto id = projector.mappedId("a");
    mirror.apply(WindowOpened{"a", "code", "editor", "1", 1});
    projector.reconcile(mirror, world);
    EXPECT_EQ(world.freeWindowCount(), 1U);
    EXPECT_EQ(projector.mappedId("a"), id);
    EXPECT_EQ(projector.findWindow("a")->workspaceName, "dev");
    EXPECT_EQ(projector.findWindow("a")->className, "code");
    EXPECT_EQ(projector.focusedWindowId(), id);
}

TEST(SpatialProjectionTest, FloatingBeforeOpenRemainsDeferredUntilWorkspaceArrives) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowFloatingChanged{"a", true});
    projector.reconcile(mirror, world);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    EXPECT_EQ(projector.findWindow("a")->placementStatus, PlacementStatus::DeferredMissingWorkspace);
    mirror.apply(WindowWorkspaceChanged{"a", 2, "2"});
    projector.reconcile(mirror, world);
    EXPECT_EQ(world.freeWindowCount(), 1U);
    EXPECT_EQ(projector.findWindow("a")->floating, true);
}

TEST(SpatialProjectionTest, CloseBeforeProjectionAndTombstonedMetadataDoNotResurrect) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowClosed{"a"});
    projector.reconcile(mirror, world);
    mirror.apply(WindowTitleChanged{"a", "late"});
    projector.reconcile(mirror, world);
    EXPECT_TRUE(projector.windows().empty());
    EXPECT_EQ(world.freeWindowCount(), 0U);
}

TEST(SpatialProjectionTest, ClosingIslandMemberRemovesItWithoutBreakingOwnership) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    const WindowId id = *projector.mappedId("a");
    ASSERT_TRUE(world.addIsland(Island{8, {100.0, 200.0}}));
    ASSERT_TRUE(world.joinFreeWindowToIsland(id, 8));
    EXPECT_EQ(world.islandForWindow(id), 8);
    mirror.apply(WindowClosed{"a"});
    projector.reconcile(mirror, world);
    EXPECT_FALSE(world.windowExists(id));
    ASSERT_NE(world.island(8), nullptr);
    EXPECT_TRUE(world.island(8)->members().empty());
}

TEST(SpatialProjectionTest, WorkspaceMoveDetachesIslandMemberAndRelocatesAsFreeWindow) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    const WindowId id = *projector.mappedId("a");
    ASSERT_TRUE(world.addIsland(Island{8, {100.0, 200.0}}));
    ASSERT_TRUE(world.joinFreeWindowToIsland(id, 8));
    mirror.apply(WindowWorkspaceChanged{"a", 2, "special:chat"});
    projector.reconcile(mirror, world);
    EXPECT_TRUE(world.hasFreeWindow(id));
    EXPECT_FALSE(world.islandForWindow(id));
    EXPECT_EQ(world.windowGeometry(id)->position.x, projector.findWindow("a")->geometry.position.x);
}

TEST(SpatialProjectionTest, ReconcileIsIdempotentAndWorldRetainsSingleOwnership) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    projector.reconcile(mirror, world);
    const WindowId id = *projector.mappedId("a");
    EXPECT_TRUE(world.hasFreeWindow(id));
    EXPECT_FALSE(world.islandForWindow(id));
    EXPECT_EQ(world.freeWindowCount(), 1U);
}

TEST(SpatialProjectionTest, ReconcileRestoresMissingProjectionAndAvoidsNewWorldIdCollision) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowTitleChanged{"partial", "waiting"});
    projector.reconcile(mirror, world);
    ASSERT_EQ(projector.mappedId("partial"), 1U);
    ASSERT_TRUE(world.addFreeWindow({1, {{9000.0, 9000.0}, {10.0, 10.0}}}));
    mirror.apply(WindowWorkspaceChanged{"partial", 1, "1"});
    projector.reconcile(mirror, world);
    const auto id = projector.mappedId("partial");
    ASSERT_TRUE(id);
    EXPECT_NE(*id, 1U);
    EXPECT_EQ(world.freeWindowCount(), 2U);
    EXPECT_TRUE(world.removeWindow(*id));
    projector.reconcile(mirror, world);
    EXPECT_TRUE(world.hasFreeWindow(*id));
}

TEST(SpatialProjectionTest, CameraHelpersFrameFocusedWindowAndWorkspaceBounds) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    mirror.apply(WindowFocused{"a"});
    projector.reconcile(mirror, world);
    const auto frame = projector.frameFocusedWindow({1200.0, 900.0}, 1.0);
    ASSERT_TRUE(frame);
    const auto geometry = projector.geometryForAddress("a");
    ASSERT_TRUE(geometry);
    Camera camera(frame->position, frame->zoom);
    EXPECT_NEAR(camera.worldToScreen(geometry->center()).x, 600.0, 1e-6);
    EXPECT_NEAR(camera.worldToScreen(geometry->center()).y, 450.0, 1e-6);
    EXPECT_TRUE(projector.viewForWorkspace("1", {1200.0, 900.0}, 1.0));
}

TEST(SpatialProjectionTest, LargeCoordinatesAndNegativeOriginAreSupported) {
    InitialPlacementPolicy policy;
    policy.firstWorkspaceOrigin = {-10000000.0, 7000000.0};
    SpatialProjector projector(policy);
    RuntimeMirror mirror;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "special:chat", std::nullopt});
    mirror.apply(WindowFocused{"a"});
    projector.reconcile(mirror, world);
    const auto geometry = projector.geometryForAddress("a");
    ASSERT_TRUE(geometry);
    EXPECT_DOUBLE_EQ(geometry->position.x, -10000000.0);
    EXPECT_DOUBLE_EQ(geometry->position.y, 7000000.0);
    EXPECT_TRUE(projector.frameFocusedWindow({1600.0, 900.0}, 1.0));
}

TEST(SpatialProjectionTest, ResetClearsProjectionAndStartsNewIdentityEpoch) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowOpened{"a", "app", "title", "1", 1});
    projector.reconcile(mirror, world);
    projector.reset(world);
    EXPECT_EQ(world.freeWindowCount(), 0U);
    EXPECT_TRUE(projector.windows().empty());
    EXPECT_TRUE(projector.workspaceMap().regions().empty());
}

TEST(SpatialProjectionTest, ResetDoesNotRemoveWorldWindowForDeferredUnprojectedId) {
    RuntimeMirror mirror;
    SpatialProjector projector;
    World world;
    mirror.apply(WindowTitleChanged{"partial", "waiting"});
    projector.reconcile(mirror, world);
    ASSERT_TRUE(world.addFreeWindow({1, {{8000.0, 9000.0}, {100.0, 100.0}}}));
    projector.reset(world);
    EXPECT_TRUE(world.hasFreeWindow(1));
    EXPECT_EQ(world.freeWindowCount(), 1U);
}

TEST(SpatialProjectionTest, EndToEndReplayUsesParserAdapterMirrorAndProjectsOnce) {
    RuntimeBridge bridge;
    SpatialProjector projector;
    World world;
    const std::array<std::string_view, 7> capture{
        "[00:00:01] windowtitlev2>>55f003fc2bd0,New title",
        "openwindow>>55f003fc2bd0,1,thunar,Old title",
        "activewindow>>thunar,Old title",
        "activewindowv2>>55f003fc2bd0",
        "movewindow>>55f003fc2bd0,2",
        "movewindowv2>>55f003fc2bd0,2,2",
        "changefloatingmode>>55f003fc2bd0,1"};
    for (const auto line : capture) {
        static_cast<void>(bridge.processLine(line));
        projector.reconcile(bridge.mirror(), world);
    }
    ASSERT_EQ(bridge.mirror().windows().size(), 1U);
    ASSERT_EQ(projector.windows().size(), 1U);
    ASSERT_EQ(world.freeWindowCount(), 1U);
    const auto* record = projector.findWindow("0x55F003FC2BD0");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->title, "New title");
    EXPECT_EQ(record->workspaceName, "2");
    EXPECT_EQ(record->floating, true);
    EXPECT_EQ(projector.focusedWindowId(), record->windowId);
}
