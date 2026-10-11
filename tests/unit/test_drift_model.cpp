// Unit tests for hyprvx::drift::DriftModel, written from specs/002-multi-window-drift-mvp/data-model.md before the model
// existed (T007): the per-monitor state machine and its idempotence, per-workspace cameras, window offsets, drags,
// spawn placement and the mechanism offset. Expected values are computed from the data-model equations by hand or
// through CameraMath (the single transform), never by the code under test.

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "hyprvx/core/CameraMath.hpp"
#include "hyprvx/core/DriftModel.hpp"

namespace cm = hyprvx::cammath;
using hyprvx::drift::DragKind;
using hyprvx::drift::DriftModel;
using hyprvx::drift::Mode;
using hyprvx::drift::Status;

namespace {

constexpr hyprvx::drift::MonitorId kMon = 1;
constexpr hyprvx::drift::MonitorId kMon2 = 2;
constexpr hyprvx::drift::WorkspaceId kWs1 = 1;
constexpr hyprvx::drift::WorkspaceId kWs2 = 2;
constexpr hyprvx::drift::WorkspaceId kSpecial = -98;
constexpr hyprvx::drift::WindowId kWinA = 0xa0;
constexpr hyprvx::drift::WindowId kWinB = 0xb0;
constexpr cm::Vec kM{0.0, 0.0};
constexpr cm::Vec kSize{1280.0, 800.0};

void expectVec(cm::Vec actual, cm::Vec expected) {
    EXPECT_DOUBLE_EQ(actual.x, expected.x);
    EXPECT_DOUBLE_EQ(actual.y, expected.y);
}

void expectCamera(const cm::CameraState& actual, const cm::CameraState& expected) {
    EXPECT_DOUBLE_EQ(actual.origin.x, expected.origin.x);
    EXPECT_DOUBLE_EQ(actual.origin.y, expected.origin.y);
    EXPECT_DOUBLE_EQ(actual.zoom, expected.zoom);
}

// OFF -> ENTERING -> DRIFT on kMon with kWs1 as the scene.
DriftModel inDrift() {
    DriftModel model;
    EXPECT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    EXPECT_EQ(model.entered(kMon, true), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Drift);
    return model;
}

} // namespace

// ---- state machine ------------------------------------------------------------------------------------------------

TEST(DriftModelTest, StartsOffWithoutAScene) {
    const DriftModel model;
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value());
}

TEST(DriftModelTest, EveryTransitionOfTheStateMachine) {
    DriftModel model;
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Entering);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value()); // the scene exists only while DRIFT
    ASSERT_EQ(model.entered(kMon, true), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Drift);
    EXPECT_EQ(model.sceneWorkspace(kMon), kWs1);
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Exiting);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value());
    ASSERT_EQ(model.exited(kMon), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
}

TEST(DriftModelTest, InvalidEnteringResultGoesBackToOff) {
    DriftModel model;
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    EXPECT_EQ(model.entered(kMon, false), Status::Incompatible);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value());
}

TEST(DriftModelTest, EnableAndDisableAreIdempotent) {
    DriftModel model;
    // disable / panic while OFF: no-ops.
    EXPECT_EQ(model.disable(kMon), Status::NoOp);
    model.panic();
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    // enable while ENTERING: no-op.
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    EXPECT_EQ(model.enable(kMon, kWs2, false), Status::NoOp);
    EXPECT_EQ(model.mode(kMon), Mode::Entering);
    ASSERT_EQ(model.entered(kMon, true), Status::Ok);
    // enable while DRIFT: no-op; the scene stays.
    EXPECT_EQ(model.enable(kMon, kWs2, false), Status::NoOp);
    EXPECT_EQ(model.sceneWorkspace(kMon), kWs1);
    // disable while EXITING: no-op.
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    EXPECT_EQ(model.disable(kMon), Status::NoOp);
    EXPECT_EQ(model.mode(kMon), Mode::Exiting);
    // entered/exited outside their state: no-ops.
    EXPECT_EQ(model.entered(kMon, true), Status::NoOp);
    ASSERT_EQ(model.exited(kMon), Status::Ok);
    EXPECT_EQ(model.exited(kMon), Status::NoOp);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
}

TEST(DriftModelTest, SpecialWorkspaceKeepsDriftOff) {
    DriftModel model;
    EXPECT_EQ(model.enable(kMon, kSpecial, true), Status::State);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
}

TEST(DriftModelTest, PanicTurnsEveryMonitorOffFromAnyState) {
    DriftModel model = inDrift();
    ASSERT_EQ(model.enable(kMon2, kWs2, false), Status::Ok); // kMon2 in ENTERING
    model.panic();
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    EXPECT_EQ(model.mode(kMon2), Mode::Off);
    // and from EXITING
    model = inDrift();
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    model.panic();
    EXPECT_EQ(model.mode(kMon), Mode::Off);
}

TEST(DriftModelTest, InvalidCameraAndMonitorRemovalLeaveDrift) {
    DriftModel model = inDrift();
    EXPECT_EQ(model.invalidCamera(kMon), Status::Ok);
    EXPECT_EQ(model.mode(kMon), Mode::Exiting);
    ASSERT_EQ(model.exited(kMon), Status::Ok);

    model = inDrift();
    model.monitorRemoved(kMon);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    // Re-added: Drift stays OFF until enabled again.
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value());
}

TEST(DriftModelTest, PanicKeepsStoredOffsetsAndCameras) {
    DriftModel model = inDrift();
    ASSERT_TRUE(model.setOffset(kWinA, {300.0, -20.0}));
    ASSERT_EQ(model.pan(kMon, {100.0, 0.0}), Status::Ok);
    const auto CAMERA = model.camera(kWs1, kM);
    model.panic();
    expectVec(model.offset(kWinA), {300.0, -20.0});
    expectCamera(model.camera(kWs1, kM), CAMERA);
    // Re-entering restores both (US1.4).
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    ASSERT_EQ(model.entered(kMon, true), Status::Ok);
    expectCamera(model.monitorCamera(kMon, kM), CAMERA);
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {300.0, -20.0});
}

// ---- per-workspace cameras ----------------------------------------------------------------------------------------

TEST(DriftModelTest, WorkspaceCameraDefaultsToIdentityOfTheMonitor) {
    const DriftModel model;
    const cm::Vec M{1280.0, 0.0};
    expectCamera(model.camera(kWs1, M), cm::identityFor(M));
    EXPECT_FALSE(model.storedCamera(kWs1).has_value());
}

TEST(DriftModelTest, SetCameraValidatesWithTheMvpLimits) {
    DriftModel model;
    EXPECT_TRUE(model.setCamera(kWs1, {{10.0, 20.0}, 0.1}));
    EXPECT_TRUE(model.setCamera(kWs1, {{10.0, 20.0}, 2.0}));
    EXPECT_FALSE(model.setCamera(kWs1, {{10.0, 20.0}, 0.09}));
    EXPECT_FALSE(model.setCamera(kWs1, {{10.0, 20.0}, 2.01}));
    EXPECT_FALSE(model.setCamera(kWs1, {{std::numeric_limits<double>::quiet_NaN(), 0.0}, 1.0}));
    EXPECT_FALSE(model.setCamera(kWs1, {{0.0, std::numeric_limits<double>::infinity()}, 1.0}));
    EXPECT_FALSE(model.setCamera(kWs1, {{0.0, 0.0}, -1.0}));
    // A rejected camera leaves the last valid one.
    expectCamera(model.camera(kWs1, kM), {{10.0, 20.0}, 2.0});
}

TEST(DriftModelTest, SpecialWorkspacesNeverGetACamera) {
    DriftModel model = inDrift();
    EXPECT_FALSE(model.setCamera(kSpecial, {{1.0, 2.0}, 0.5}));
    EXPECT_FALSE(model.storedCamera(kSpecial).has_value());
    // A special workspace shown on the monitor does not replace the scene.
    model.workspaceActivated(kMon, kSpecial, true);
    EXPECT_EQ(model.sceneWorkspace(kMon), kWs1);
}

TEST(DriftModelTest, MonitorCameraIsTheSceneCameraOnlyInDrift) {
    DriftModel model;
    ASSERT_TRUE(model.setCamera(kWs1, {{50.0, 60.0}, 0.5}));
    expectCamera(model.monitorCamera(kMon, kM), cm::identityFor(kM)); // OFF
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    expectCamera(model.monitorCamera(kMon, kM), cm::identityFor(kM)); // ENTERING
    ASSERT_EQ(model.entered(kMon, true), Status::Ok);
    expectCamera(model.monitorCamera(kMon, kM), {{50.0, 60.0}, 0.5});
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    expectCamera(model.monitorCamera(kMon, kM), cm::identityFor(kM)); // EXITING
}

TEST(DriftModelTest, PanZoomResetChangeOnlyTheSceneWorkspaceCamera) {
    DriftModel model = inDrift();
    ASSERT_TRUE(model.setCamera(kWs2, {{7.0, 8.0}, 1.5}));
    ASSERT_EQ(model.pan(kMon, {40.0, -10.0}), Status::Ok);
    // panByScreen from identity: P = M - d/z.
    expectCamera(model.camera(kWs1, kM), {{-40.0, 10.0}, 1.0});
    ASSERT_EQ(model.zoom(kMon, kM, {640.0, 400.0}, 0.5), Status::Ok);
    const auto EXPECTED = cm::anchorZoom({{-40.0, 10.0}, 1.0}, kM, {640.0, 400.0}, 0.5, cm::ZoomLimits{});
    expectCamera(model.camera(kWs1, kM), EXPECTED);
    expectCamera(model.camera(kWs2, kM), {{7.0, 8.0}, 1.5}); // never leaks into another workspace
    ASSERT_EQ(model.reset(kMon, kM), Status::Ok);
    expectCamera(model.camera(kWs1, kM), cm::identityFor(kM));
}

TEST(DriftModelTest, ZoomIsClampedAndInvalidRequestsAreRejected) {
    DriftModel model = inDrift();
    ASSERT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, 100.0), Status::Ok);
    EXPECT_DOUBLE_EQ(model.camera(kWs1, kM).zoom, 2.0);
    ASSERT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, 1e-6), Status::Ok);
    EXPECT_DOUBLE_EQ(model.camera(kWs1, kM).zoom, 0.1);
    EXPECT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, 0.0), Status::Invalid);
    EXPECT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, -2.0), Status::Invalid);
    EXPECT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, std::numeric_limits<double>::quiet_NaN()), Status::Invalid);
    EXPECT_EQ(model.zoom(kMon, kM, {std::numeric_limits<double>::infinity(), 0.0}, 1.1), Status::Invalid);
    EXPECT_EQ(model.pan(kMon, {std::numeric_limits<double>::infinity(), 0.0}), Status::Invalid);
    // A finite request whose result overflows is rejected and the camera is kept.
    ASSERT_EQ(model.pan(kMon, {1e307, 0.0}), Status::Ok);
    const auto BEFORE = model.camera(kWs1, kM);
    EXPECT_EQ(model.pan(kMon, {-1.7e308, 0.0}), Status::Invalid);
    expectCamera(model.camera(kWs1, kM), BEFORE);
    EXPECT_TRUE(std::isfinite(model.camera(kWs1, kM).origin.x));
}

// Added with the implementation (T008): an unset camera is identity of the monitor's real origin for pan and drags.
TEST(DriftModelTest, PanFromAnUnsetCameraStartsAtTheMonitorOrigin) {
    DriftModel model;
    const cm::Vec M{1280.0, 0.0};
    model.setMonitorOrigin(kMon2, M);
    expectVec(model.originOf(kMon2), M);
    ASSERT_EQ(model.enable(kMon2, kWs2, false), Status::Ok);
    ASSERT_EQ(model.entered(kMon2, true), Status::Ok);
    expectVec(model.originOf(kMon2), M); // kept across transitions
    ASSERT_EQ(model.pan(kMon2, {10.0, 0.0}), Status::Ok);
    expectCamera(model.camera(kWs2, M), cm::panByScreen(cm::identityFor(M), {10.0, 0.0}));
    ASSERT_TRUE(model.beginPan(kMon2, {0.0, 0.0}));
    expectCamera(model.drag()->startCamera, cm::panByScreen(cm::identityFor(M), {10.0, 0.0}));
}

TEST(DriftModelTest, CameraCommandsNeedDrift) {
    DriftModel model;
    EXPECT_EQ(model.pan(kMon, {1.0, 1.0}), Status::State);
    EXPECT_EQ(model.zoom(kMon, kM, {0.0, 0.0}, 1.1), Status::State);
    EXPECT_EQ(model.reset(kMon, kM), Status::State);
    EXPECT_FALSE(model.storedCamera(kWs1).has_value());
}

TEST(DriftModelTest, WorkspaceChangeInDriftLoadsThatWorkspaceCamera) {
    DriftModel model = inDrift();
    ASSERT_EQ(model.pan(kMon, {10.0, 0.0}), Status::Ok);
    model.workspaceActivated(kMon, kWs2, false);
    EXPECT_EQ(model.sceneWorkspace(kMon), kWs2);
    expectCamera(model.monitorCamera(kMon, kM), cm::identityFor(kM)); // kWs2 has no camera yet
    ASSERT_EQ(model.pan(kMon, {0.0, 20.0}), Status::Ok);
    model.workspaceActivated(kMon, kWs1, false);
    expectCamera(model.monitorCamera(kMon, kM), {{-10.0, 0.0}, 1.0});
    // A destroyed and recreated id keeps its stored camera for the session.
    model.workspaceActivated(kMon, kWs2, false);
    expectCamera(model.monitorCamera(kMon, kM), {{0.0, -20.0}, 1.0});
}

TEST(DriftModelTest, WorkspaceChangeWhileOffDoesNotStartDrift) {
    DriftModel model;
    model.workspaceActivated(kMon, kWs2, false);
    EXPECT_EQ(model.mode(kMon), Mode::Off);
    EXPECT_FALSE(model.sceneWorkspace(kMon).has_value());
}

// ---- window offsets -----------------------------------------------------------------------------------------------

TEST(DriftModelTest, OffsetsAreFiniteAndDefaultToZero) {
    DriftModel model;
    expectVec(model.offset(kWinA), {0.0, 0.0});
    EXPECT_TRUE(model.setOffset(kWinA, {12.5, -3.0}));
    expectVec(model.offset(kWinA), {12.5, -3.0});
    EXPECT_FALSE(model.setOffset(kWinA, {std::numeric_limits<double>::quiet_NaN(), 0.0}));
    EXPECT_FALSE(model.setOffset(kWinA, {0.0, -std::numeric_limits<double>::infinity()}));
    expectVec(model.offset(kWinA), {12.5, -3.0});
}

TEST(DriftModelTest, OffsetIsDroppedOnCloseAndKeptAcrossWorkspaces) {
    DriftModel model = inDrift();
    ASSERT_TRUE(model.setOffset(kWinA, {100.0, 0.0}));
    // The offset belongs to the window: a window on another workspace keeps it, but it is only shown in the scene.
    expectVec(model.mechanismOffset(kWinA, kWs2, kMon, true), {0.0, 0.0});
    expectVec(model.offset(kWinA), {100.0, 0.0});
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {100.0, 0.0});
    model.windowClosed(kWinA);
    expectVec(model.offset(kWinA), {0.0, 0.0});
    EXPECT_TRUE(model.offsets().empty());
}

TEST(DriftModelTest, MechanismOffsetIsZeroOutsideADriftScene) {
    DriftModel model;
    ASSERT_TRUE(model.setOffset(kWinA, {5.0, 6.0}));
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {0.0, 0.0}); // OFF
    ASSERT_EQ(model.enable(kMon, kWs1, false), Status::Ok);
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {0.0, 0.0}); // ENTERING
    ASSERT_EQ(model.entered(kMon, true), Status::Ok);
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {5.0, 6.0}); // DRIFT scene
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, false), {0.0, 0.0}); // unmapped
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon2, true), {0.0, 0.0}); // other monitor, not in Drift
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    expectVec(model.mechanismOffset(kWinA, kWs1, kMon, true), {0.0, 0.0}); // EXITING
}

// ---- drags --------------------------------------------------------------------------------------------------------

TEST(DriftModelTest, MoveDragChangesOnlyTheOffsetByDeltaOverZoom) {
    DriftModel model = inDrift();
    ASSERT_TRUE(model.setCamera(kWs1, {{0.0, 0.0}, 0.5}));
    ASSERT_TRUE(model.setOffset(kWinA, {10.0, 20.0}));
    ASSERT_TRUE(model.beginMove(kMon, kWinA, {100.0, 100.0}));
    ASSERT_TRUE(model.drag().has_value());
    EXPECT_EQ(model.drag()->kind, DragKind::Move);
    EXPECT_EQ(model.drag()->window, kWinA);
    ASSERT_TRUE(model.dragMotion({130.0, 90.0}));
    // O = startOffset + delta/z = (10, 20) + (30, -10)/0.5
    expectVec(model.offset(kWinA), {70.0, 0.0});
    ASSERT_TRUE(model.dragMotion({100.0, 100.0}));
    expectVec(model.offset(kWinA), {10.0, 20.0}); // absolute from the start, no accumulated error
    ASSERT_TRUE(model.dragMotion({200.0, 150.0}));
    expectCamera(model.camera(kWs1, kM), {{0.0, 0.0}, 0.5}); // the camera is untouched
    model.dragEnd();
    EXPECT_FALSE(model.drag().has_value());
    expectVec(model.offset(kWinA), {210.0, 120.0}); // the last applied value stays
    EXPECT_FALSE(model.dragMotion({0.0, 0.0}));
}

TEST(DriftModelTest, PanDragMovesTheCameraWithThePointer) {
    DriftModel model = inDrift();
    ASSERT_TRUE(model.setCamera(kWs1, {{100.0, 50.0}, 2.0}));
    ASSERT_TRUE(model.beginPan(kMon, {10.0, 10.0}));
    EXPECT_EQ(model.drag()->kind, DragKind::Pan);
    ASSERT_TRUE(model.dragMotion({30.0, 0.0}));
    // P = startP - delta/z = (100, 50) - (20, -10)/2
    expectCamera(model.camera(kWs1, kM), {{90.0, 55.0}, 2.0});
    model.dragEnd();
    expectCamera(model.camera(kWs1, kM), {{90.0, 55.0}, 2.0});
}

TEST(DriftModelTest, DragsNeedDriftAndOnlyOneAtATime) {
    DriftModel model;
    EXPECT_FALSE(model.beginPan(kMon, {0.0, 0.0}));
    EXPECT_FALSE(model.beginMove(kMon, kWinA, {0.0, 0.0}));
    model = inDrift();
    ASSERT_TRUE(model.beginPan(kMon, {0.0, 0.0}));
    EXPECT_FALSE(model.beginMove(kMon, kWinA, {0.0, 0.0}));
    EXPECT_EQ(model.drag()->kind, DragKind::Pan);
}

TEST(DriftModelTest, DragEndsOnEveryEndCondition) {
    // leaving DRIFT
    DriftModel model = inDrift();
    ASSERT_TRUE(model.beginMove(kMon, kWinA, {0.0, 0.0}));
    ASSERT_EQ(model.disable(kMon), Status::Ok);
    EXPECT_FALSE(model.drag().has_value());
    // panic
    model = inDrift();
    ASSERT_TRUE(model.beginPan(kMon, {0.0, 0.0}));
    model.panic();
    EXPECT_FALSE(model.drag().has_value());
    // the dragged window closes (another window closing does not end it)
    model = inDrift();
    ASSERT_TRUE(model.beginMove(kMon, kWinA, {0.0, 0.0}));
    model.windowClosed(kWinB);
    EXPECT_TRUE(model.drag().has_value());
    model.windowClosed(kWinA);
    EXPECT_FALSE(model.drag().has_value());
    // the dragged window leaves the scene
    model = inDrift();
    ASSERT_TRUE(model.beginMove(kMon, kWinA, {0.0, 0.0}));
    model.windowLeftScene(kWinA);
    EXPECT_FALSE(model.drag().has_value());
    // the scene workspace changes
    model = inDrift();
    ASSERT_TRUE(model.beginPan(kMon, {0.0, 0.0}));
    model.workspaceActivated(kMon, kWs2, false);
    EXPECT_FALSE(model.drag().has_value());
    // the monitor is removed
    model = inDrift();
    ASSERT_TRUE(model.beginPan(kMon, {0.0, 0.0}));
    model.monitorRemoved(kMon);
    EXPECT_FALSE(model.drag().has_value());
}

// ---- spawn placement ----------------------------------------------------------------------------------------------

TEST(DriftModelTest, SpawnOffsetCentresTheWindowInTheViewport) {
    // centre(visibleWorldRect(C)) - centre(L): C = (P (-500, 200), z 0.5) on 1280x800 shows (-500, 200) .. (2060, 1800),
    // centre (780, 1000); L = (140, 100, 800, 600) has centre (540, 400); offset (240, 600).
    const cm::CameraState C{{-500.0, 200.0}, 0.5};
    const auto O = DriftModel::spawnOffset(C, kSize, {{140.0, 100.0}, {800.0, 600.0}});
    expectVec(O, {240.0, 600.0});
    // The window is then drawn centred on the monitor: forward(C, M, centre(L) + O) = M + size/2.
    const auto S = cm::forward(C, kM, {540.0 + O.x, 400.0 + O.y});
    expectVec(S, {640.0, 400.0});
    // Identity camera: the offset only centres the window on the monitor.
    expectVec(DriftModel::spawnOffset(cm::identityFor(kM), kSize, {{0.0, 0.0}, {1280.0, 800.0}}), {0.0, 0.0});
}
