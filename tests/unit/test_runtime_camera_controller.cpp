#include "hyprvx/runtime/RuntimeCameraController.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"
#include "hyprvx/core/OutputTopology.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <string>

using namespace hyprvx;
using namespace hyprvx::runtime;
using namespace hyprvx::hyprland;

namespace {

struct RuntimeScene {
    RuntimeBridge bridge;
    SpatialProjector projector;
    World world;

    RuntimeScene() {
        projector.setCompositorEpoch("camera-test");
        for (const auto line : {
                 "openwindow>>a,1,one,A", "openwindow>>b,1,two,B", "openwindow>>c,2,three,C"}) {
            static_cast<void>(bridge.processLine(line));
            projector.reconcile(bridge.mirror(), world);
        }
    }
};

CameraMotionConfig quickMotion() {
    CameraMotionConfig config;
    config.flightResponse = 12.0;
    config.flightEpsilon = 0.01;
    config.stopSpeed = 0.05;
    config.maxSpeed = 5000.0;
    return config;
}

} // namespace

TEST(RuntimeCameraControllerTest, PanGestureCreatesOneHistoryTransaction) {
    Camera camera;
    World world;
    RuntimeCameraController nav(camera, world, {800.0, 600.0});
    EXPECT_TRUE(nav.panBegin());
    nav.panByWorld({100.0, 20.0});
    nav.panByWorld({50.0, -5.0});
    EXPECT_TRUE(nav.panEnd());
    EXPECT_EQ(nav.history().backSize(), 1U);
    EXPECT_DOUBLE_EQ(camera.position().x, 150.0);
    EXPECT_DOUBLE_EQ(camera.position().y, 15.0);
    EXPECT_FALSE(nav.needsFrame());
}

TEST(RuntimeCameraControllerTest, ZeroDeltaPanDoesNotPolluteHistory) {
    Camera camera;
    World world;
    RuntimeCameraController nav(camera, world, {800.0, 600.0});
    EXPECT_TRUE(nav.panBegin());
    EXPECT_FALSE(nav.panEnd());
    EXPECT_EQ(nav.history().backSize(), 0U);
}

TEST(RuntimeCameraControllerTest, ScreenPanUsesCurrentZoomAndInterruptsFlight) {
    Camera camera({2000.0, 0.0}, 2.0);
    World world;
    RuntimeCameraController nav(camera, world, {800.0, 600.0}, nullptr, quickMotion());
    nav.motion().flyTo({9000.0, 0.0});
    ASSERT_TRUE(nav.motion().isFlying());
    static_cast<void>(nav.panBegin());
    nav.panByScreen({100.0, -50.0});
    EXPECT_FALSE(nav.motion().isFlying());
    EXPECT_DOUBLE_EQ(camera.position().x, 1950.0);
    EXPECT_DOUBLE_EQ(camera.position().y, 25.0);
    static_cast<void>(nav.panEnd());
    EXPECT_FALSE(nav.needsFrame());
}

TEST(RuntimeCameraControllerTest, KeyboardPanIsDeltaTimeDrivenAndEndsTransactionAfterSettling) {
    Camera a, b;
    World worldA, worldB;
    RuntimeCameraController first(a, worldA, {800.0, 600.0}, nullptr, quickMotion());
    RuntimeCameraController second(b, worldB, {800.0, 600.0}, nullptr, quickMotion());
    first.setPanInput({1.0, 0.0});
    second.setPanInput({1.0, 0.0});
    for (int i = 0; i < 60; ++i) static_cast<void>(first.tick(1.0 / 60.0));
    for (int i = 0; i < 120; ++i) static_cast<void>(second.tick(1.0 / 120.0));
    first.setPanInput({});
    second.setPanInput({});
    for (int i = 0; i < 600 && first.needsFrame(); ++i) static_cast<void>(first.tick(1.0 / 60.0));
    for (int i = 0; i < 1200 && second.needsFrame(); ++i) static_cast<void>(second.tick(1.0 / 120.0));
    EXPECT_NEAR(a.position().x, b.position().x, 1.0);
    EXPECT_FALSE(first.needsFrame());
    EXPECT_EQ(first.history().backSize(), 1U);
}

TEST(RuntimeCameraControllerTest, GotoWindowFramesProjectedGeometryAndMissingWindowFails) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    const auto id = scene.projector.mappedId("a");
    ASSERT_TRUE(id);
    EXPECT_TRUE(nav.gotoWindow(*id));
    EXPECT_EQ(nav.selectedWindow(), id);
    EXPECT_TRUE(nav.motion().isFlying());
    EXPECT_FALSE(nav.gotoWindow(999999));
    EXPECT_TRUE(nav.motion().isFlying());
}

TEST(RuntimeCameraControllerTest, GotoExternalAddressCanonicalizesAddress) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    EXPECT_TRUE(nav.gotoExternalAddress("0xA"));
    EXPECT_EQ(nav.selectedWindow(), scene.projector.mappedId("a"));
}

TEST(RuntimeCameraControllerTest, GotoWindowHandlesNegativeAndLargeWorldCoordinates) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({1, {{-900000000.0, -800000000.0}, {300.0, 200.0}}}));
    ASSERT_TRUE(world.addFreeWindow({2, {{500000000.0, 400000000.0}, {300.0, 200.0}}}));
    Camera camera;
    RuntimeCameraController nav(camera, world, {1000.0, 700.0}, nullptr, quickMotion());
    EXPECT_TRUE(nav.gotoWindow(1));
    EXPECT_LT(nav.motion().flightTarget()->x, 0.0);
    EXPECT_TRUE(nav.gotoWindow(2));
    EXPECT_GT(nav.motion().flightTarget()->x, 0.0);
}

TEST(RuntimeCameraControllerTest, DirectionalFocusMovesSelectionAndCameraTarget) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({1, {{0.0, 0.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({2, {{300.0, 0.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({3, {{-300.0, 0.0}, {100.0, 100.0}}}));
    Camera camera;
    RuntimeCameraController nav(camera, world, {800.0, 600.0}, nullptr, quickMotion());
    nav.observeCompositorSelection(1);
    EXPECT_TRUE(nav.focusDirection(SpatialDirection::Right));
    EXPECT_EQ(nav.selectedWindow(), 2U);
    const auto rightGeometry = world.windowGeometry(2);
    ASSERT_TRUE(rightGeometry);
    const auto framed = frameRectInViewport(*rightGeometry, {800.0, 600.0}, camera.zoom());
    EXPECT_DOUBLE_EQ(nav.motion().flightTarget()->x, framed.cameraPosition.x);
    EXPECT_TRUE(nav.focusDirection(SpatialDirection::Left));
    EXPECT_EQ(nav.selectedWindow(), 1U);
}

TEST(RuntimeCameraControllerTest, DirectionalFocusWithoutSelectionOrCandidateIsNoOp) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({1, {{0.0, 0.0}, {100.0, 100.0}}}));
    Camera camera;
    RuntimeCameraController nav(camera, world, {800.0, 600.0});
    EXPECT_FALSE(nav.focusDirection(SpatialDirection::Left));
    nav.observeCompositorSelection(1);
    EXPECT_FALSE(nav.focusDirection(SpatialDirection::Left));
    EXPECT_FALSE(nav.motion().isFlying());
}

TEST(RuntimeCameraControllerTest, DirectionalFocusNavigatesUpAndDown) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({1, {{0.0, 0.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({2, {{0.0, -300.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({3, {{0.0, 300.0}, {100.0, 100.0}}}));
    Camera camera;
    RuntimeCameraController nav(camera, world, {800.0, 600.0});
    nav.observeCompositorSelection(1);
    EXPECT_TRUE(nav.focusDirection(SpatialDirection::Up));
    EXPECT_EQ(nav.selectedWindow(), 2U);
    EXPECT_TRUE(nav.focusDirection(SpatialDirection::Down));
    EXPECT_EQ(nav.selectedWindow(), 1U);
}

TEST(RuntimeCameraControllerTest, WorkspaceJumpSupportsNumericNamedAndSpecialRegions) {
    RuntimeScene scene;
    auto state = scene.projector.snapshot();
    state.workspaces.push_back({"dev", {7200.0, 0.0}, {{7200.0, 0.0}, {3600.0, 2500.0}}, std::nullopt});
    state.workspaces.push_back({"special:chat", {10800.0, 0.0}, {{10800.0, 0.0}, {3600.0, 2500.0}}, std::nullopt});
    state.nextWorkspaceRegion = 4;
    SpatialProjector restored;
    World restoredWorld;
    restored.restoreState(state, restoredWorld);
    Camera camera;
    RuntimeCameraController nav(camera, restoredWorld, {1000.0, 700.0}, &restored, quickMotion());
    EXPECT_TRUE(nav.gotoWorkspace("1"));
    EXPECT_TRUE(nav.gotoWorkspace("dev"));
    EXPECT_TRUE(nav.gotoWorkspace("special:chat"));
    EXPECT_FALSE(nav.gotoWorkspace("unknown"));
}

TEST(RuntimeCameraControllerTest, RestoredWorkspaceRegionCanBeNavigated) {
    RuntimeScene scene;
    auto state = scene.projector.snapshot();
    state.workspaces.push_back({"special:restore", {10800.0, 0.0}, {{10800.0, 0.0}, {3600.0, 2500.0}}, std::nullopt});
    state.nextWorkspaceRegion = 4;
    SpatialProjector restored;
    World restoredWorld;
    restored.restoreState(state, restoredWorld);
    Camera camera;
    RuntimeCameraController nav(camera, restoredWorld, {1000.0, 700.0}, &restored, quickMotion());
    EXPECT_TRUE(nav.gotoWorkspace("special:restore"));
}

TEST(RuntimeCameraControllerTest, NavigationHistoryBackForwardAndBranchingWork) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    ASSERT_TRUE(nav.gotoWorkspace("1"));
    for (int i = 0; i < 200 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    ASSERT_TRUE(nav.gotoWorkspace("2"));
    for (int i = 0; i < 200 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    EXPECT_TRUE(nav.historyBack());
    for (int i = 0; i < 200 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    EXPECT_TRUE(nav.historyForward());
    for (int i = 0; i < 200 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    EXPECT_TRUE(nav.historyBack());
    EXPECT_TRUE(nav.gotoWorkspace("1"));
    EXPECT_FALSE(nav.history().canGoForward());
}

TEST(RuntimeCameraControllerTest, NewGotoReplacesPreviousAnimation) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    ASSERT_TRUE(nav.gotoWorkspace("1"));
    ASSERT_TRUE(nav.gotoWorkspace("2"));
    const double target = nav.motion().flightTarget()->x;
    const auto expected = scene.projector.viewForWorkspace("2", {1000.0, 700.0}, camera.zoom());
    ASSERT_TRUE(expected);
    EXPECT_DOUBLE_EQ(target, expected->position.x);
    for (int i = 0; i < 300 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    EXPECT_FALSE(nav.needsFrame());
    EXPECT_NEAR(camera.position().x, target, 0.1);
}

TEST(RuntimeCameraControllerTest, CompositorSelectionSeedsWhileIdleButUserOwnsAfterNavigation) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    const auto one = scene.projector.mappedId("a");
    const auto two = scene.projector.mappedId("b");
    nav.observeCompositorSelection(one);
    EXPECT_EQ(nav.selectedWindow(), one);
    EXPECT_TRUE(nav.userOwnsSelection() == false);
    ASSERT_TRUE(nav.gotoWindow(*two));
    nav.observeCompositorSelection(one);
    EXPECT_EQ(nav.selectedWindow(), two);
    nav.resetSelectionOwnership();
    nav.observeCompositorSelection(one);
    EXPECT_EQ(nav.selectedWindow(), one);
}

TEST(RuntimeCameraControllerTest, ClosingSelectedWindowClearsSelectionAndEpochResetIsSafe) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    const auto id = scene.projector.mappedId("a");
    nav.observeCompositorSelection(id);
    static_cast<void>(scene.bridge.processLine("closewindow>>a"));
    scene.projector.reconcile(scene.bridge.mirror(), scene.world);
    nav.reconcileProjector();
    EXPECT_FALSE(nav.selectedWindow());
    nav.endEpoch();
    EXPECT_FALSE(nav.needsFrame());
    EXPECT_FALSE(nav.selectedWindow());
    EXPECT_TRUE(std::isfinite(camera.position().x));
}

TEST(RuntimeCameraControllerTest, HomeUsesFocusedContentWorkspaceThenOriginFallback) {
    RuntimeScene scene;
    Camera camera({9000.0, 0.0});
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    EXPECT_TRUE(nav.home({}, "2"));
    const auto expected = scene.projector.viewForWorkspace("2", {1000.0, 700.0}, camera.zoom());
    ASSERT_TRUE(expected);
    EXPECT_NEAR(nav.motion().flightTarget()->x, expected->position.x, 0.1);

    World empty;
    Camera fallback({1234.0, -567.0});
    RuntimeCameraController home(fallback, empty, {800.0, 600.0}, nullptr, quickMotion());
    EXPECT_TRUE(home.home());
    EXPECT_DOUBLE_EQ(home.motion().flightTarget()->x, 0.0);
    EXPECT_DOUBLE_EQ(home.motion().flightTarget()->y, 0.0);
}

TEST(RuntimeCameraControllerTest, HomePrefersObservedFocusedWindowOverWorkspace) {
    RuntimeScene scene;
    static_cast<void>(scene.bridge.processLine("activewindowv2>>b"));
    scene.projector.reconcile(scene.bridge.mirror(), scene.world);
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    EXPECT_TRUE(nav.home({}, "2"));
    EXPECT_EQ(nav.selectedWindow(), scene.projector.mappedId("b"));
}

TEST(RuntimeCameraControllerTest, ObservedExternalFocusInitializesSelectionOnlyWhileIdle) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion());
    nav.observeCompositorFocus("0xB");
    EXPECT_EQ(nav.selectedWindow(), scene.projector.mappedId("b"));
    ASSERT_TRUE(nav.gotoWindow(*scene.projector.mappedId("a")));
    nav.observeCompositorFocus("b");
    EXPECT_EQ(nav.selectedWindow(), scene.projector.mappedId("a"));
}

TEST(RuntimeCameraControllerTest, HistoryCapacityRemainsBounded) {
    RuntimeScene scene;
    Camera camera;
    RuntimeCameraController nav(camera, scene.world, {1000.0, 700.0}, &scene.projector, quickMotion(), 3);
    for (const auto workspace : {"1", "2", "1", "2", "1"}) {
        EXPECT_TRUE(nav.gotoWorkspace(workspace));
        for (int i = 0; i < 200 && nav.needsFrame(); ++i) static_cast<void>(nav.tick(1.0 / 60.0));
    }
    EXPECT_LE(nav.history().backSize(), 3U);
}

TEST(RuntimeCameraControllerTest, IdleControllerDoesNotRequestFramesAndViewportValidationIsStrict) {
    Camera camera;
    World world;
    RuntimeCameraController nav(camera, world, {800.0, 600.0});
    EXPECT_FALSE(nav.needsFrame());
    EXPECT_FALSE(nav.tick(1.0 / 60.0));
    EXPECT_THROW(nav.setViewport({0.0, 600.0}), std::invalid_argument);
}

TEST(RuntimeCameraControllerTest, SingleOutputTopologyPolicyRemainsExternalExclusive) {
    OutputTopologyPlanner planner;
    OutputTopologySnapshot snapshot;
    snapshot.outputs["eDP-2"] = {true, true};
    snapshot.primary = "eDP-2";
    auto actions = planner.plan(snapshot);
    EXPECT_TRUE(actions.empty());
    snapshot.outputs["DP-1"] = {true, false};
    actions = planner.plan(snapshot);
    EXPECT_EQ(actions.size(), 3U);
    EXPECT_EQ(actions[0].kind, OutputTopologyActionKind::Activate);
    EXPECT_EQ(actions[0].output, "DP-1");
    EXPECT_EQ(actions[2].kind, OutputTopologyActionKind::Deactivate);
    EXPECT_EQ(actions[2].output, "eDP-2");
}

TEST(RuntimeCameraControllerTest, CapturedReplayNavigationSequenceIsDeterministic) {
    const auto run = [] {
        RuntimeBridge bridge;
        SpatialProjector projector;
        World world;
        projector.setCompositorEpoch("replay");
        std::ifstream capture(std::string(HYPRVX_TEST_DATA_DIR) + "/runtime_bridge_capture.txt");
        EXPECT_TRUE(capture.good());
        std::string line;
        while (std::getline(capture, line)) {
            static_cast<void>(bridge.processLine(line));
            projector.reconcile(bridge.mirror(), world);
        }
        Camera camera;
        RuntimeCameraController navigation(camera, world, {1200.0, 800.0}, &projector, quickMotion());
        EXPECT_TRUE(navigation.gotoExternalAddress("0x55F003FC2BD0"));
        EXPECT_TRUE(navigation.gotoWorkspace("2"));
        for (int i = 0; i < 500 && navigation.needsFrame(); ++i) {
            static_cast<void>(navigation.tick(1.0 / 120.0));
        }
        return std::pair{camera.position(), camera.zoom()};
    };
    const auto first = run();
    const auto second = run();
    EXPECT_DOUBLE_EQ(first.first.x, second.first.x);
    EXPECT_DOUBLE_EQ(first.first.y, second.first.y);
    EXPECT_DOUBLE_EQ(first.second, second.second);
}
