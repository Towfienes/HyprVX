#include "hyprvx/runtime/RuntimeNavigationInput.hpp"
#include "hyprvx/runtime/SpatialProjection.hpp"
#include "hyprvx/hyprland/RuntimeBridge.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace hyprvx;
using namespace hyprvx::runtime;
using namespace hyprvx::hyprland;

namespace {
struct Scene {
    RuntimeBridge bridge;
    SpatialProjector projector;
    World world;
    Scene() {
        projector.setCompositorEpoch("input-test");
        for (const char* line : {"openwindow>>a,1,a,A", "openwindow>>b,1,b,B", "openwindow>>c,1,c,C"}) {
            static_cast<void>(bridge.processLine(line));
            projector.reconcile(bridge.mirror(), world);
        }
    }
};

RuntimeNavigationAction key(KeyboardNavigationAdapter& adapter, KeyboardKey value, bool down,
                            KeyboardModifiers modifiers = {}, bool repeat = false) {
    RuntimeNavigationAction result;
    EXPECT_TRUE(adapter.key(value, down, modifiers, repeat, result));
    return result;
}
}

TEST(RuntimeNavigationInputTest, KeyboardMapsPanFocusHistoryHomeAndWorkspaceIntents) {
    KeyboardNavigationAdapter keyboard;
    EXPECT_EQ(key(keyboard, KeyboardKey::H, true).type, RuntimeNavigationActionType::ContinuousPanStart);
    EXPECT_EQ(key(keyboard, KeyboardKey::L, true).type, RuntimeNavigationActionType::ContinuousPanUpdate);
    EXPECT_DOUBLE_EQ(keyboard.panDirection().x, 0.0);
    EXPECT_EQ(key(keyboard, KeyboardKey::H, false).type, RuntimeNavigationActionType::ContinuousPanUpdate);
    EXPECT_EQ(key(keyboard, KeyboardKey::L, false).type, RuntimeNavigationActionType::ContinuousPanStop);
    EXPECT_EQ(key(keyboard, KeyboardKey::J, true, {.shift = true}).spatialDirection, SpatialDirection::Down);
    EXPECT_EQ(key(keyboard, KeyboardKey::Left, true, {.alt = true}).type, RuntimeNavigationActionType::HistoryBack);
    EXPECT_EQ(key(keyboard, KeyboardKey::Right, true, {.alt = true}).type, RuntimeNavigationActionType::HistoryForward);
    EXPECT_EQ(key(keyboard, KeyboardKey::Home, true).type, RuntimeNavigationActionType::Home);
    const auto workspace = key(keyboard, KeyboardKey::Digit2, true);
    EXPECT_EQ(workspace.type, RuntimeNavigationActionType::GotoWorkspace);
    EXPECT_EQ(workspace.workspace, "2");
}

TEST(RuntimeNavigationInputTest, KeyboardSimultaneousDirectionsAndResetStayCoherent) {
    KeyboardNavigationAdapter keyboard;
    static_cast<void>(key(keyboard, KeyboardKey::H, true));
    static_cast<void>(key(keyboard, KeyboardKey::K, true));
    EXPECT_NEAR(keyboard.panDirection().x, -std::sqrt(0.5), 1e-9);
    EXPECT_NEAR(keyboard.panDirection().y, -std::sqrt(0.5), 1e-9);
    keyboard.reset();
    EXPECT_FALSE(keyboard.panActive());
}

TEST(RuntimeNavigationInputTest, ShiftFocusKeyReleaseDoesNotCreatePanGesture) {
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(keyboard.key(KeyboardKey::H, true, {.shift = true}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::FocusDirection);
    // Modifier is released before the physical H key-up arrives.
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, false, {}, false, emitted));
    EXPECT_FALSE(keyboard.panActive());
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, true, {.shift = true}, true, emitted));
    EXPECT_FALSE(keyboard.panActive());
}

TEST(RuntimeNavigationInputTest, ModifiedDirectionPressesAndReleasesNeverEmitPhantomActions) {
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(keyboard.key(KeyboardKey::Left, true, {.alt = true}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::HistoryBack);
    EXPECT_FALSE(keyboard.key(KeyboardKey::Left, false, {.alt = true}, false, emitted));
    ASSERT_TRUE(keyboard.key(KeyboardKey::Right, true, {.alt = true}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::HistoryForward);
    EXPECT_FALSE(keyboard.key(KeyboardKey::Right, false, {.alt = true}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::Up, true, {.alt = true}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::Up, false, {.alt = true}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, true, {.alt = true}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, true, {.control = true}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::Left, true, {.control = true}, false, emitted));
}

TEST(RuntimeNavigationInputTest, ModifiedKeysCannotWedgeDispatcherWithDefaultPanBegin) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    if (keyboard.key(KeyboardKey::Left, true, {.alt = true}, false, emitted))
        static_cast<void>(dispatcher.submit(emitted));
    if (keyboard.key(KeyboardKey::Left, false, {.alt = true}, false, emitted))
        static_cast<void>(dispatcher.submit(emitted));
    if (keyboard.key(KeyboardKey::H, true, {.control = true}, false, emitted))
        static_cast<void>(dispatcher.submit(emitted));
    EXPECT_FALSE(dispatcher.directGestureActive());
    RuntimeNavigationAction begin; begin.type = RuntimeNavigationActionType::PanBegin;
    EXPECT_TRUE(dispatcher.submit(begin));
    RuntimeNavigationAction end; end.type = RuntimeNavigationActionType::PanEnd;
    EXPECT_FALSE(dispatcher.submit(end)); // zero movement ends cleanly
}

TEST(RuntimeNavigationInputTest, UnsolicitedDirectionKeyUpsDoNotEmitActions) {
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, false, {}, false, emitted));
    EXPECT_FALSE(keyboard.key(KeyboardKey::Left, false, {}, false, emitted));
    ASSERT_TRUE(keyboard.key(KeyboardKey::H, true, {}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ContinuousPanStart);
    ASSERT_TRUE(keyboard.key(KeyboardKey::H, false, {}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ContinuousPanStop);
    EXPECT_FALSE(keyboard.key(KeyboardKey::H, false, {}, false, emitted));
}

TEST(RuntimeNavigationInputTest, MouseMiddleDragAndClickEmitDeviceIndependentActions) {
    MouseNavigationAdapter mouse({2.0, 0.1});
    RuntimeNavigationAction emitted;
    EXPECT_TRUE(mouse.button(MouseButton::Middle, true, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::PanBegin);
    EXPECT_TRUE(mouse.motion({5.0, -3.0}, emitted));
    EXPECT_DOUBLE_EQ(emitted.delta.x, 10.0);
    EXPECT_DOUBLE_EQ(emitted.delta.y, -6.0);
    EXPECT_TRUE(mouse.button(MouseButton::Middle, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::PanEnd);
    EXPECT_TRUE(mouse.click(42, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::SelectWindow);
    EXPECT_EQ(emitted.windowId, 42U);
}

TEST(RuntimeNavigationInputTest, MouseWheelCreatesOneAnchoredZoomTransaction) {
    MouseNavigationAdapter mouse;
    RuntimeNavigationAction emitted;
    EXPECT_TRUE(mouse.wheelBegin(emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ZoomBegin);
    EXPECT_TRUE(mouse.wheel(2.0, {100.0, 200.0}, emitted));
    EXPECT_NEAR(emitted.zoomFactor, std::exp(0.28), 1e-12);
    EXPECT_EQ(emitted.anchor.y, 200.0);
    EXPECT_TRUE(mouse.wheelEnd(emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ZoomEnd);
}

TEST(RuntimeNavigationInputTest, MouseRejectsOtherButtonsAndResetClearsDragAndWheel) {
    MouseNavigationAdapter mouse;
    RuntimeNavigationAction emitted;
    EXPECT_FALSE(mouse.button(MouseButton::Left, true, emitted));
    EXPECT_FALSE(mouse.button(MouseButton::Other, true, emitted));
    EXPECT_FALSE(mouse.dragging());
    ASSERT_TRUE(mouse.button(MouseButton::Middle, true, emitted));
    ASSERT_TRUE(mouse.wheelBegin(emitted));
    mouse.reset();
    EXPECT_FALSE(mouse.dragging());
    EXPECT_FALSE(mouse.wheelActive());
    EXPECT_FALSE(mouse.button(MouseButton::Middle, false, emitted));
    EXPECT_FALSE(mouse.wheelEnd(emitted));
}

TEST(RuntimeNavigationInputTest, ThreeFingerPanSharesMousePanActions) {
    GestureNavigationAdapter gestures;
    RuntimeNavigationAction emitted;
    EXPECT_FALSE(gestures.panBegin(2, emitted));
    ASSERT_TRUE(gestures.panBegin(3, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::PanBegin);
    ASSERT_TRUE(gestures.panUpdate({0.1, -0.2}, emitted));
    EXPECT_DOUBLE_EQ(emitted.delta.x, 100.0);
    EXPECT_DOUBLE_EQ(emitted.delta.y, -200.0);
    ASSERT_TRUE(gestures.panEnd(emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::PanEnd);
}

TEST(RuntimeNavigationInputTest, PinchAndFourFingerSwipeEmitSemanticActions) {
    GestureNavigationAdapter gestures;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(gestures.pinchBegin(2, emitted));
    ASSERT_TRUE(gestures.pinchUpdate(1.2, {0.25, 0.75}, {800.0, 600.0}, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ZoomAtAnchor);
    EXPECT_DOUBLE_EQ(emitted.anchor.x, 200.0);
    EXPECT_DOUBLE_EQ(emitted.anchor.y, 450.0);
    ASSERT_TRUE(gestures.pinchEnd(emitted));
    ASSERT_TRUE(gestures.swipeBegin(4));
    gestures.swipeUpdate({-0.5, 0.1});
    ASSERT_TRUE(gestures.swipeEnd(emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::FocusDirection);
    EXPECT_EQ(emitted.spatialDirection, SpatialDirection::Right);
}

TEST(RuntimeNavigationInputTest, SwipeBelowThresholdIsIgnoredAndInvalidOrderingIsSafe) {
    GestureNavigationAdapter gestures;
    RuntimeNavigationAction emitted;
    EXPECT_FALSE(gestures.panUpdate({1.0, 0.0}, emitted));
    ASSERT_TRUE(gestures.swipeBegin(4));
    gestures.swipeUpdate({0.1, 0.0});
    EXPECT_FALSE(gestures.swipeEnd(emitted));
    EXPECT_FALSE(gestures.panEnd(emitted));
}

TEST(RuntimeNavigationInputTest, GestureResetDuringPanAndPinchRejectsStaleEvents) {
    GestureNavigationAdapter gestures;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(gestures.panBegin(3, emitted));
    gestures.reset();
    EXPECT_FALSE(gestures.active());
    EXPECT_FALSE(gestures.panUpdate({0.1, 0.1}, emitted));
    EXPECT_FALSE(gestures.panEnd(emitted));
    ASSERT_TRUE(gestures.pinchBegin(2, emitted));
    gestures.reset();
    EXPECT_FALSE(gestures.active());
    EXPECT_FALSE(gestures.pinchUpdate(1.1, {0.5, 0.5}, {800.0, 600.0}, emitted));
    EXPECT_FALSE(gestures.pinchEnd(emitted));
}

TEST(RuntimeNavigationInputTest, PanDeltasCoalesceUntilExplicitFlushAndEndCommitsOneHistory) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    auto action = [](RuntimeNavigationActionType type) { RuntimeNavigationAction a; a.type = type; return a; };
    EXPECT_FALSE(dispatcher.submit(action(RuntimeNavigationActionType::PanEnd)));
    EXPECT_TRUE(dispatcher.submit(action(RuntimeNavigationActionType::PanBegin)));
    auto delta = action(RuntimeNavigationActionType::PanDelta); delta.delta = {20.0, 10.0};
    EXPECT_TRUE(dispatcher.submit(delta));
    delta.delta = {30.0, -5.0};
    EXPECT_TRUE(dispatcher.submit(delta));
    EXPECT_DOUBLE_EQ(dispatcher.pendingPanDelta().x, 50.0);
    EXPECT_TRUE(dispatcher.flushPendingPan());
    EXPECT_DOUBLE_EQ(camera.position().x, -50.0);
    EXPECT_DOUBLE_EQ(camera.position().y, -5.0);
    EXPECT_TRUE(dispatcher.submit(action(RuntimeNavigationActionType::PanEnd)));
    EXPECT_EQ(controller.history().backSize(), 1U);
    EXPECT_FALSE(dispatcher.submit(action(RuntimeNavigationActionType::PanEnd)));
}

TEST(RuntimeNavigationInputTest, PanDeltaWithoutBeginIsIgnored) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction delta; delta.type = RuntimeNavigationActionType::PanDelta; delta.delta = {100.0, 0.0};
    EXPECT_FALSE(dispatcher.submit(delta));
    EXPECT_DOUBLE_EQ(camera.position().x, 0.0);
    EXPECT_FALSE(dispatcher.flushPendingPan());
}

TEST(RuntimeNavigationInputTest, InputFocusCancellationClearsGestureAndKeyboardStateWithoutEpochReset) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction begin; begin.type = RuntimeNavigationActionType::PanBegin;
    RuntimeNavigationAction delta; delta.type = RuntimeNavigationActionType::PanDelta; delta.delta = {80.0, 20.0};
    ASSERT_TRUE(dispatcher.submit(begin));
    ASSERT_TRUE(dispatcher.submit(delta));
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(keyboard.key(KeyboardKey::H, true, {}, false, emitted));
    ASSERT_TRUE(dispatcher.submit(emitted));
    dispatcher.cancelInput();
    keyboard.reset();
    EXPECT_FALSE(dispatcher.directGestureActive());
    EXPECT_DOUBLE_EQ(dispatcher.pendingPanDelta().x, 0.0);
    EXPECT_FALSE(keyboard.panActive());
    EXPECT_FALSE(controller.needsFrame());
    EXPECT_DOUBLE_EQ(camera.position().x, 0.0);
    RuntimeNavigationAction staleEnd; staleEnd.type = RuntimeNavigationActionType::PanEnd;
    EXPECT_FALSE(dispatcher.submit(staleEnd));
}

TEST(RuntimeNavigationInputTest, InterruptedGestureCannotBeFinishedByStalePanEnd) {
    Scene scene;
    Camera camera;
    RuntimeCameraController controller(camera, scene.world, {1000.0, 700.0}, &scene.projector);
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction begin; begin.type = RuntimeNavigationActionType::PanBegin;
    RuntimeNavigationAction delta; delta.type = RuntimeNavigationActionType::PanDelta; delta.delta = {100.0, 0.0};
    ASSERT_TRUE(dispatcher.submit(begin));
    ASSERT_TRUE(dispatcher.submit(delta));
    RuntimeNavigationAction go; go.type = RuntimeNavigationActionType::GotoWindow; go.windowId = *scene.projector.mappedId("b");
    ASSERT_TRUE(dispatcher.submit(go));
    EXPECT_FALSE(dispatcher.directGestureActive());
    RuntimeNavigationAction end; end.type = RuntimeNavigationActionType::PanEnd;
    EXPECT_FALSE(dispatcher.submit(end));
    EXPECT_TRUE(controller.motion().isFlying());
    EXPECT_EQ(controller.history().backSize(), 1U);
}

TEST(RuntimeNavigationInputTest, ClickSelectionOwnsDirectionalNavigationWithoutFraming) {
    Scene scene;
    Camera camera;
    RuntimeCameraController controller(camera, scene.world, {1000.0, 700.0}, &scene.projector);
    RuntimeNavigationActionDispatcher dispatcher(controller);
    const WindowId b = *scene.projector.mappedId("b");
    RuntimeNavigationAction click; click.type = RuntimeNavigationActionType::SelectWindow; click.windowId = b;
    EXPECT_TRUE(dispatcher.submit(click));
    EXPECT_EQ(controller.selectedWindow(), b);
    EXPECT_TRUE(controller.userOwnsSelection());
    EXPECT_FALSE(controller.motion().isFlying());
    EXPECT_TRUE(controller.focusDirection(SpatialDirection::Right));
    EXPECT_EQ(controller.selectedWindow(), *scene.projector.mappedId("c"));
}

TEST(RuntimeNavigationInputTest, ClickAfterPanOwnsSubsequentDirectionalNavigation) {
    Scene scene;
    Camera camera;
    RuntimeCameraController controller(camera, scene.world, {1000.0, 700.0}, &scene.projector);
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction begin; begin.type = RuntimeNavigationActionType::PanBegin;
    RuntimeNavigationAction delta; delta.type = RuntimeNavigationActionType::PanDelta; delta.delta = {40.0, 0.0};
    RuntimeNavigationAction end; end.type = RuntimeNavigationActionType::PanEnd;
    ASSERT_TRUE(dispatcher.submit(begin)); ASSERT_TRUE(dispatcher.submit(delta)); ASSERT_TRUE(dispatcher.submit(end));
    const WindowId b = *scene.projector.mappedId("b");
    RuntimeNavigationAction select; select.type = RuntimeNavigationActionType::SelectWindow; select.windowId = b;
    ASSERT_TRUE(dispatcher.submit(select));
    EXPECT_EQ(controller.selectedWindow(), b);
    EXPECT_EQ(controller.selectionOwnership(), SelectionOwnership::UserOwned);
    ASSERT_TRUE(controller.focusDirection(SpatialDirection::Right));
    EXPECT_EQ(controller.selectedWindow(), *scene.projector.mappedId("c"));
}

TEST(RuntimeNavigationInputTest, HomePrioritizesUserSelectionThenCompositorSelectionWhenSeeded) {
    Scene scene;
    static_cast<void>(scene.bridge.processLine("activewindowv2>>a"));
    scene.projector.reconcile(scene.bridge.mirror(), scene.world);
    Camera camera;
    RuntimeCameraController controller(camera, scene.world, {1000.0, 700.0}, &scene.projector);
    const WindowId c = *scene.projector.mappedId("c");
    const WindowId a = *scene.projector.mappedId("a");
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction click; click.type = RuntimeNavigationActionType::SelectWindow; click.windowId = c;
    ASSERT_TRUE(dispatcher.submit(click));
    EXPECT_TRUE(controller.home({}, "1"));
    EXPECT_EQ(controller.selectedWindow(), c);
    controller.resetSelectionOwnership();
    controller.observeCompositorSelection(a);
    EXPECT_TRUE(controller.home({}, "1"));
    EXPECT_EQ(controller.selectedWindow(), a);
}

TEST(RuntimeNavigationInputTest, CanonicalFocusTieBreakRemainsDeterministic) {
    World world;
    ASSERT_TRUE(world.addFreeWindow({1, {{0.0, 0.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({8, {{300.0, -50.0}, {100.0, 100.0}}}));
    ASSERT_TRUE(world.addFreeWindow({3, {{300.0, 50.0}, {100.0, 100.0}}}));
    Camera camera;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    controller.observeCompositorSelection(1);
    EXPECT_TRUE(controller.focusDirection(SpatialDirection::Right));
    EXPECT_EQ(controller.selectedWindow(), 3U);
}

TEST(RuntimeNavigationInputTest, EpochResetClearsPendingGestureAndRejectsLateEnd) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    RuntimeNavigationAction begin; begin.type = RuntimeNavigationActionType::PanBegin;
    RuntimeNavigationAction delta; delta.type = RuntimeNavigationActionType::PanDelta; delta.delta = {50.0, 0.0};
    ASSERT_TRUE(dispatcher.submit(begin));
    ASSERT_TRUE(dispatcher.submit(delta));
    dispatcher.resetForEpoch();
    EXPECT_FALSE(dispatcher.directGestureActive());
    EXPECT_DOUBLE_EQ(dispatcher.pendingPanDelta().x, 0.0);
    RuntimeNavigationAction end; end.type = RuntimeNavigationActionType::PanEnd;
    EXPECT_FALSE(dispatcher.submit(end));
    EXPECT_DOUBLE_EQ(camera.position().x, 0.0);
}

TEST(RuntimeNavigationInputTest, GestureAndMouseActionsProduceEquivalentPan) {
    Camera mouseCamera, gestureCamera;
    World mouseWorld, gestureWorld;
    RuntimeCameraController mouseController(mouseCamera, mouseWorld, {800.0, 600.0});
    RuntimeCameraController gestureController(gestureCamera, gestureWorld, {800.0, 600.0});
    RuntimeNavigationActionDispatcher mouseDispatcher(mouseController), gestureDispatcher(gestureController);
    MouseNavigationAdapter mouse;
    GestureNavigationAdapter gesture;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(mouse.button(MouseButton::Middle, true, emitted)); ASSERT_TRUE(mouseDispatcher.submit(emitted));
    ASSERT_TRUE(mouse.motion({80.0, -30.0}, emitted)); ASSERT_TRUE(mouseDispatcher.submit(emitted));
    ASSERT_TRUE(gesture.panBegin(3, emitted)); ASSERT_TRUE(gestureDispatcher.submit(emitted));
    ASSERT_TRUE(gesture.panUpdate({0.08, -0.03}, emitted)); ASSERT_TRUE(gestureDispatcher.submit(emitted));
    EXPECT_TRUE(mouseDispatcher.flushPendingPan());
    EXPECT_TRUE(gestureDispatcher.flushPendingPan());
    EXPECT_DOUBLE_EQ(mouseCamera.position().x, gestureCamera.position().x);
    EXPECT_DOUBLE_EQ(mouseCamera.position().y, gestureCamera.position().y);
}

TEST(RuntimeNavigationInputTest, KeyboardContinuousPanMovesCameraAndReleaseStopsInput) {
    Camera camera;
    World world;
    RuntimeCameraController controller(camera, world, {800.0, 600.0});
    RuntimeNavigationActionDispatcher dispatcher(controller);
    KeyboardNavigationAdapter keyboard;
    RuntimeNavigationAction emitted;
    ASSERT_TRUE(keyboard.key(KeyboardKey::L, true, {}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ContinuousPanStart);
    ASSERT_TRUE(dispatcher.submit(emitted));
    for (int i = 0; i < 60; ++i) static_cast<void>(controller.tick(1.0 / 60.0));
    EXPECT_GT(camera.position().x, 0.0);
    ASSERT_TRUE(keyboard.key(KeyboardKey::L, false, {}, false, emitted));
    EXPECT_EQ(emitted.type, RuntimeNavigationActionType::ContinuousPanStop);
    ASSERT_TRUE(dispatcher.submit(emitted));
    for (int i = 0; i < 600 && controller.needsFrame(); ++i) static_cast<void>(controller.tick(1.0 / 60.0));
    EXPECT_FALSE(controller.needsFrame());
    EXPECT_EQ(controller.history().backSize(), 1U);
}
