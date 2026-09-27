#include "hyprvx/platform/OverlayState.hpp"
#include <gtest/gtest.h>

using hyprvx::platform::OverlayState;

TEST(OverlayStateTest, CoalescesDirtyUpdatesBehindFrameCallback) {
    OverlayState state;
    state.configure(1280, 720, 1);
    EXPECT_TRUE(state.beginFrame());
    state.markDirty();
    state.markDirty();
    EXPECT_FALSE(state.beginFrame());
    state.frameDone();
    EXPECT_TRUE(state.beginFrame());
    state.frameDone();
    EXPECT_FALSE(state.beginFrame());
}

TEST(OverlayStateTest, PassiveAndInteractiveTransitionsDirtyOnce) {
    OverlayState state;
    state.configure(800, 600, 1);
    ASSERT_TRUE(state.beginFrame());
    state.frameDone();
    state.setInteractive(false);
    EXPECT_FALSE(state.beginFrame());
    state.setInteractive(true);
    EXPECT_TRUE(state.interactive());
    EXPECT_TRUE(state.beginFrame());
    state.frameDone();
    state.setInteractive(false);
    EXPECT_FALSE(state.interactive());
    EXPECT_TRUE(state.beginFrame());
}

TEST(OverlayStateTest, ScaleAndResizeUpdateBufferExtent) {
    OverlayState state;
    state.configure(1200, 700, 2);
    EXPECT_EQ(state.bufferWidth(), 2400U);
    EXPECT_EQ(state.bufferHeight(), 1400U);
    state.configure(900, 500, 1);
    EXPECT_EQ(state.bufferWidth(), 900U);
    EXPECT_EQ(state.bufferHeight(), 500U);
}

TEST(OverlayStateTest, RejectsUnboundedBuffers) {
    OverlayState state;
    EXPECT_THROW(state.configure(8000, 8000, 1), std::invalid_argument);
    EXPECT_THROW(state.configure(0, 600, 1), std::invalid_argument);
    EXPECT_THROW(state.configure(800, 600, 0), std::invalid_argument);
}

TEST(OverlayStateTest, FailedFrameRetainsDirtyState) {
    OverlayState state;
    state.configure(100, 100, 1);
    ASSERT_TRUE(state.beginFrame());
    state.frameFailed();
    EXPECT_TRUE(state.beginFrame());
}
