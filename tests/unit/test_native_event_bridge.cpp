#include "hyprvx/platform/NativeEventBridge.hpp"
#include <gtest/gtest.h>

using namespace hyprvx::platform;
using namespace hyprvx::hyprland;

TEST(NativeEventBridgeTest, ParsesWindowOpenWithNamedWorkspaceAndSeparateActualGeometry) {
    const auto result = parseNativeObservation(R"({"kind":"window_open","address":"0xABC","workspace":"special:chat","class":"foot","title":"Hello, world","actual_geometry":[10,20,300,200]})");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->semantic);
    const auto* opened = std::get_if<WindowOpened>(&*result->semantic);
    ASSERT_NE(opened, nullptr);
    EXPECT_EQ(opened->address, "abc");
    EXPECT_EQ(opened->workspaceName, "special:chat");
    EXPECT_FALSE(opened->workspaceId);
    EXPECT_EQ(opened->title, "Hello, world");
    ASSERT_TRUE(result->actualGeometry);
    EXPECT_EQ(result->actualGeometry->xywh[2], 300);
}

TEST(NativeEventBridgeTest, ParsesFocusLoss) {
    const auto result = parseNativeObservation(R"({"kind":"focus","address":""})");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->semantic);
    EXPECT_TRUE(std::holds_alternative<WindowFocusCleared>(*result->semantic));
}

TEST(NativeEventBridgeTest, ParsesMoveAndFloating) {
    const auto move = parseNativeObservation(R"({"kind":"window_move","address":"ABC","workspace":"4"})");
    ASSERT_TRUE(move);
    ASSERT_TRUE(move->semantic);
    const auto* moved = std::get_if<WindowWorkspaceChanged>(&*move->semantic);
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(moved->workspaceId, 4);
    const auto floating = parseNativeObservation(R"({"kind":"window_floating","address":"abc","floating":true})");
    ASSERT_TRUE(floating);
    ASSERT_TRUE(floating->semantic);
    EXPECT_TRUE(std::get<WindowFloatingChanged>(*floating->semantic).floating);
}

TEST(NativeEventBridgeTest, RejectsInvalidOrUnboundedGeometry) {
    EXPECT_FALSE(parseNativeObservation("not json"));
    EXPECT_FALSE(parseNativeObservation(R"({"kind":"window_open","address":"?","workspace":"1"})"));
    const auto result = parseNativeObservation(R"({"kind":"window_close","address":"abc","actual_geometry":[0,0,-2,4]})");
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->actualGeometry);
}

TEST(NativeEventBridgeTest, ParsesSemanticCameraControls) {
    const auto pan = parseNativeObservation(R"({"kind":"camera_pan","dx":25,"dy":-10})");
    ASSERT_TRUE(pan);
    ASSERT_TRUE(pan->cameraPan);
    EXPECT_EQ((*pan->cameraPan)[0], 25);
    EXPECT_EQ((*pan->cameraPan)[1], -10);
    const auto reset = parseNativeObservation(R"({"kind":"camera_reset"})");
    ASSERT_TRUE(reset);
    EXPECT_TRUE(reset->cameraReset);
}
