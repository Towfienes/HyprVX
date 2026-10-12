// Unit tests for hyprvx::drift GestureModel, written from specs/003-trackpad-native-ux/contracts/gesture-model.md 1.0
// before the model existed (T005): the decideTarget table, scroll sequence tracking, release velocity, glide damping and
// glide integration (V1 CameraController), and the pinch camera. Expected values come from the contract's rules and
// formulas computed by hand or through CameraMath (the single transform), never from the code under test.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <random>

#include <gtest/gtest.h>

#include "hyprvx/core/CameraMath.hpp"
#include "hyprvx/core/GestureModel.hpp"

namespace cm = hyprvx::cammath;
using hyprvx::drift::BeginContext;
using hyprvx::drift::decideTarget;
using hyprvx::drift::expectedGlide;
using hyprvx::drift::Glide;
using hyprvx::drift::GlideConfig;
using hyprvx::drift::glideDamping;
using hyprvx::drift::InputKind;
using hyprvx::drift::pinchCamera;
using hyprvx::drift::ReleaseVelocity;
using hyprvx::drift::ScrollStep;
using hyprvx::drift::ScrollTracker;
using hyprvx::drift::Target;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::uint64_t testSeed() {
    if (const char* value = std::getenv("HYPRVX_TEST_SEED"))
        return std::strtoull(value, nullptr, 10);
    return 20261012;
}

double length(cm::Vec v) {
    return std::hypot(v.x, v.y);
}

void expectVecNear(cm::Vec actual, cm::Vec expected, double tolerance) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
}

BeginContext canvas() {
    BeginContext ctx;
    ctx.drift = true;
    return ctx;
}

// The contract table, rule by rule (first match wins). Used as the oracle for the exhaustive enumeration; each rule
// also has its own hand-written test below.
Target tableOracle(InputKind kind, unsigned fingers, const BeginContext& c) {
    const bool gesture = kind != InputKind::Scroll;
    if (!c.drift)
        return Target::PassThrough;                                                        // 1
    if (gesture && fingers >= 4)
        return Target::PassThrough;                                                        // 2
    if (gesture && fingers < 2)
        return Target::PassThrough;                                                        // 3
    if (gesture && fingers == 3 && (c.layerUnderPointer || c.fullscreenScene || c.dragActive))
        return Target::Ignore;                                                             // 4
    if (kind == InputKind::Swipe && fingers == 3)
        return Target::Pan;                                                                // 5
    if (kind == InputKind::Pinch && fingers == 3)
        return Target::Zoom;                                                               // 6
    if (c.layerUnderPointer || c.fullscreenScene)
        return Target::PassThrough;                                                        // 7
    if (c.dragActive)
        return Target::Ignore;                                                             // 8
    if (kind == InputKind::Swipe)
        return Target::PassThrough;                                                        // 9
    if (c.superHeld)
        return kind == InputKind::Scroll ? Target::Pan : Target::Zoom;                     // 10
    if (c.windowUnderPointer)
        return Target::PassThrough;                                                        // 11
    return kind == InputKind::Scroll ? Target::Pan : Target::Zoom;                         // 12
}

// Sum of all glide steps of length dt until the glide ends (or 100000 steps).
cm::Vec runGlide(Glide& glide, double dt) {
    cm::Vec total;
    for (int i = 0; i < 100000 && glide.active(); ++i) {
        const auto step = glide.step(dt);
        total.x += step.x;
        total.y += step.y;
    }
    return total;
}

} // namespace

// ---- decideTarget ------------------------------------------------------------------------------------------------

TEST(GestureTarget, Rule1NotInDriftPassesThrough) {
    BeginContext ctx;  // drift = false
    ctx.superHeld = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Swipe, 3, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 3, ctx), Target::PassThrough);
}

TEST(GestureTarget, Rule2FourOrMoreFingersPassThrough) {
    auto ctx = canvas();
    EXPECT_EQ(decideTarget(InputKind::Swipe, 4, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 4, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Swipe, 5, ctx), Target::PassThrough);
}

TEST(GestureTarget, Rule3OneFingerGesturePassesThrough) {
    auto ctx = canvas();
    EXPECT_EQ(decideTarget(InputKind::Swipe, 1, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 1, ctx), Target::PassThrough);
}

TEST(GestureTarget, Rule4ThreeFingersOverLayerFullscreenOrDragAreIgnored) {
    for (int which = 0; which < 3; ++which) {
        auto ctx = canvas();
        ctx.layerUnderPointer = which == 0;
        ctx.fullscreenScene   = which == 1;
        ctx.dragActive        = which == 2;
        EXPECT_EQ(decideTarget(InputKind::Swipe, 3, ctx), Target::Ignore) << which;
        EXPECT_EQ(decideTarget(InputKind::Pinch, 3, ctx), Target::Ignore) << which;
    }
}

TEST(GestureTarget, Rules5And6ThreeFingersPanAndZoomEvenOverAWindow) {
    auto ctx = canvas();
    ctx.windowUnderPointer = true;
    EXPECT_EQ(decideTarget(InputKind::Swipe, 3, ctx), Target::Pan);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 3, ctx), Target::Zoom);
}

TEST(GestureTarget, Rule7TwoFingerInputOverLayerOrFullscreenPassesThrough) {
    auto layer = canvas();
    layer.layerUnderPointer = true;
    layer.superHeld         = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, layer), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, layer), Target::PassThrough);
    auto full = canvas();
    full.fullscreenScene = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, full), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, full), Target::PassThrough);
}

TEST(GestureTarget, Rule8TwoFingerInputDuringADragIsIgnored) {
    auto ctx = canvas();
    ctx.dragActive = true;
    ctx.superHeld  = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, ctx), Target::Ignore);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, ctx), Target::Ignore);
}

TEST(GestureTarget, Rule9TwoFingerSwipePassesThrough) {
    auto ctx = canvas();
    ctx.superHeld = true;
    EXPECT_EQ(decideTarget(InputKind::Swipe, 2, ctx), Target::PassThrough);
}

TEST(GestureTarget, Rule10SuperPansOrZoomsOverAWindow) {
    auto ctx = canvas();
    ctx.superHeld          = true;
    ctx.windowUnderPointer = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, ctx), Target::Pan);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, ctx), Target::Zoom);
}

TEST(GestureTarget, Rule11WindowWithoutSuperPassesThrough) {
    auto ctx = canvas();
    ctx.windowUnderPointer = true;
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, ctx), Target::PassThrough);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, ctx), Target::PassThrough);
}

TEST(GestureTarget, Rule12EmptyCanvasPansOrZooms) {
    auto ctx = canvas();
    EXPECT_EQ(decideTarget(InputKind::Scroll, 2, ctx), Target::Pan);
    EXPECT_EQ(decideTarget(InputKind::Pinch, 2, ctx), Target::Zoom);
}

TEST(GestureTarget, ScrollIgnoresFingerCount) {
    auto ctx = canvas();
    for (unsigned fingers : {0u, 1u, 2u, 3u, 4u, 5u})
        EXPECT_EQ(decideTarget(InputKind::Scroll, fingers, ctx), Target::Pan) << fingers;
}

TEST(GestureTarget, ExhaustiveTableAndThreeFingersNeverPassThroughInDrift) {
    int checked = 0;
    for (unsigned bits = 0; bits < 64; ++bits) {
        BeginContext ctx;
        ctx.drift              = bits & 1;
        ctx.layerUnderPointer  = bits & 2;
        ctx.fullscreenScene    = bits & 4;
        ctx.dragActive         = bits & 8;
        ctx.superHeld          = bits & 16;
        ctx.windowUnderPointer = bits & 32;
        for (auto kind : {InputKind::Scroll, InputKind::Swipe, InputKind::Pinch}) {
            for (unsigned fingers = 1; fingers <= 5; ++fingers) {
                const auto target = decideTarget(kind, fingers, ctx);
                EXPECT_EQ(target, tableOracle(kind, fingers, ctx)) << "bits=" << bits << " kind=" << static_cast<int>(kind) << " fingers=" << fingers;
                if (ctx.drift && kind != InputKind::Scroll && fingers == 3)
                    EXPECT_NE(target, Target::PassThrough) << "bits=" << bits;
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, 64 * 3 * 5);
}

// ---- ScrollTracker -----------------------------------------------------------------------------------------------

TEST(ScrollTracker, SingleAxisBeginContinueEnd) {
    ScrollTracker t;
    EXPECT_FALSE(t.open());
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    EXPECT_TRUE(t.open());
    EXPECT_EQ(t.feed(0, 3.0, 110), ScrollStep::Continue);
    EXPECT_EQ(t.feed(0, 0.0, 120), ScrollStep::End);
    EXPECT_FALSE(t.open());
}

TEST(ScrollTracker, EndsOnlyWhenEveryMovingAxisStopped) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    EXPECT_EQ(t.feed(1, -2.0, 100), ScrollStep::Continue);
    EXPECT_EQ(t.feed(0, 0.0, 120), ScrollStep::Continue);  // horizontal still moving
    EXPECT_TRUE(t.open());
    EXPECT_EQ(t.feed(1, 0.0, 120), ScrollStep::End);
    EXPECT_FALSE(t.open());
}

TEST(ScrollTracker, StopOfAnAxisThatNeverMovedDoesNotEndTheSequence) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    EXPECT_EQ(t.feed(1, 0.0, 105), ScrollStep::Continue);
    EXPECT_TRUE(t.open());
    EXPECT_EQ(t.feed(0, 0.0, 110), ScrollStep::End);
}

TEST(ScrollTracker, StopWhileClosedIsIgnored) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 0.0, 100), ScrollStep::Ignore);
    EXPECT_FALSE(t.open());
}

TEST(ScrollTracker, IdleLongerThan200msStartsANewSequence) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    EXPECT_EQ(t.feed(0, 5.0, 300), ScrollStep::Continue);  // exactly 200 ms: same sequence
    EXPECT_EQ(t.feed(0, 5.0, 501), ScrollStep::Begin);     // 201 ms: new sequence
    EXPECT_TRUE(t.open());
    EXPECT_EQ(t.feed(0, 0.0, 800), ScrollStep::Ignore);    // stop after the idle time: old sequence over, closed
    EXPECT_FALSE(t.open());
}

TEST(ScrollTracker, WrappingTimes) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 5.0, 0xFFFFFFF0u), ScrollStep::Begin);
    EXPECT_EQ(t.feed(0, 5.0, 0x00000010u), ScrollStep::Continue);  // 32 ms later across the wrap
    EXPECT_EQ(t.feed(0, 0.0, 0x00000020u), ScrollStep::End);
}

TEST(ScrollTracker, InvalidInputIsIgnored) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, kNaN, 100), ScrollStep::Ignore);
    EXPECT_EQ(t.feed(0, std::numeric_limits<double>::infinity(), 100), ScrollStep::Ignore);
    EXPECT_EQ(t.feed(2, 5.0, 100), ScrollStep::Ignore);
    EXPECT_EQ(t.feed(-1, 5.0, 100), ScrollStep::Ignore);
    EXPECT_FALSE(t.open());
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    EXPECT_EQ(t.feed(0, kNaN, 105), ScrollStep::Ignore);
    EXPECT_TRUE(t.open());
    EXPECT_EQ(t.feed(0, 0.0, 110), ScrollStep::End);
}

TEST(ScrollTracker, ResetCloses) {
    ScrollTracker t;
    EXPECT_EQ(t.feed(0, 5.0, 100), ScrollStep::Begin);
    t.reset();
    EXPECT_FALSE(t.open());
    EXPECT_EQ(t.feed(0, 5.0, 101), ScrollStep::Begin);
}

// ---- ReleaseVelocity ---------------------------------------------------------------------------------------------

TEST(ReleaseVelocity, EmptyAndSingleSampleGiveZero) {
    ReleaseVelocity v;
    expectVecNear(v.estimate(100), {0, 0}, 0);
    v.add(100, {10, 0});
    expectVecNear(v.estimate(110), {0, 0}, 0);
}

TEST(ReleaseVelocity, KnownSamples) {
    ReleaseVelocity v;
    for (std::uint32_t t : {100u, 110u, 120u, 130u})
        v.add(t, {10, -5});
    // window: all four; sum except the oldest = (30, -15) over 30 ms
    expectVecNear(v.estimate(140), {1000, -500}, 1e-9);
    expectVecNear(v.estimate(180), {1000, -500}, 1e-9);  // rest exactly 50 ms
    expectVecNear(v.estimate(181), {0, 0}, 0);           // rest 51 ms
}

TEST(ReleaseVelocity, SamplesOlderThanTheWindowAreExcluded) {
    ReleaseVelocity v;
    v.add(0, {1000, 0});
    v.add(100, {10, 0});
    v.add(110, {10, 0});
    v.add(120, {10, 0});
    // tn = 120: t = 0 is 120 ms old (excluded); oldest in window t = 100; (20, 0) over 20 ms
    expectVecNear(v.estimate(120), {1000, 0}, 1e-9);
}

TEST(ReleaseVelocity, EqualTimestampsAreMerged) {
    ReleaseVelocity v;
    v.add(100, {10, 0});
    v.add(100, {5, 0});
    v.add(110, {10, 0});
    // merged: (100, 15), (110, 10) -> 10 over 10 ms; unmerged would give 15 over 10 ms
    expectVecNear(v.estimate(110), {1000, 0}, 1e-9);
}

TEST(ReleaseVelocity, RingKeepsTheNewest32Samples) {
    ReleaseVelocity v;
    for (std::uint32_t i = 0; i < 40; ++i)
        v.add(i, {0.1 * i, 0});
    // kept: t = 8..39; sum except the oldest = 0.1 * (sum 9..39) = 74.4 over 31 ms = 2400 px/s
    // (all 40 kept would give 78 over 39 ms = 2000 px/s)
    expectVecNear(v.estimate(39), {2400, 0}, 1e-6);
}

TEST(ReleaseVelocity, ClampedTo6000PreservingDirection) {
    ReleaseVelocity v;
    v.add(100, {0, 0});
    v.add(101, {60, 80});  // 100 px in 1 ms
    const auto e = v.estimate(101);
    EXPECT_NEAR(length(e), 6000.0, 1e-6);
    expectVecNear(e, {3600, 4800}, 1e-6);
}

TEST(ReleaseVelocity, NonFiniteSamplesAreIgnoredAndResetClears) {
    ReleaseVelocity v;
    v.add(100, {10, 0});
    v.add(105, {kNaN, 0});
    v.add(110, {10, 0});
    expectVecNear(v.estimate(110), {1000, 0}, 1e-9);
    v.reset();
    expectVecNear(v.estimate(110), {0, 0}, 0);
}

// ---- glideDamping / expectedGlide ---------------------------------------------------------------------------------

TEST(GlideDamping, Mapping) {
    EXPECT_FALSE(glideDamping(0.0).has_value());
    EXPECT_FALSE(glideDamping(-0.5).has_value());
    EXPECT_FALSE(glideDamping(kNaN).has_value());
    EXPECT_DOUBLE_EQ(*glideDamping(0.5), 7.0);
    EXPECT_DOUBLE_EQ(*glideDamping(1.0), 2.0);
    EXPECT_DOUBLE_EQ(*glideDamping(2.0), 2.0);
    EXPECT_DOUBLE_EQ(*glideDamping(0.1), 11.0);
}

TEST(ExpectedGlide, Formula) {
    GlideConfig cfg;  // strength 0.5 -> lambda 7, minSpeed 100, stopSpeed 1, maxSpeed 6000
    expectVecNear(expectedGlide({1000, 0}, cfg), {999.0 / 7.0, 0}, 1e-9);
    expectVecNear(expectedGlide({300, 400}, cfg), {499.0 / 7.0 * 0.6, 499.0 / 7.0 * 0.8}, 1e-9);
    expectVecNear(expectedGlide({50, 0}, cfg), {0, 0}, 0);                  // below minSpeed
    expectVecNear(expectedGlide({9000, 0}, cfg), {5999.0 / 7.0, 0}, 1e-9);  // clamped to maxSpeed
    GlideConfig off = cfg;
    off.strength = 0.0;
    expectVecNear(expectedGlide({1000, 0}, off), {0, 0}, 0);
}

// ---- Glide ---------------------------------------------------------------------------------------------------------

TEST(Glide, SumOfStepsMatchesTheFormulaForAnySplit) {
    GlideConfig cfg;
    for (double dt : {10.0, 1.0 / 60.0, 1.0 / 144.0}) {
        Glide glide;
        ASSERT_TRUE(glide.start({1000, -400}, cfg));
        const auto total = runGlide(glide, dt);
        EXPECT_FALSE(glide.active());
        expectVecNear(total, expectedGlide({1000, -400}, cfg), 0.5);
    }
    GlideConfig strong = cfg;
    strong.strength = 1.0;
    Glide glide;
    ASSERT_TRUE(glide.start({3000, 0}, strong));
    expectVecNear(runGlide(glide, 1.0 / 60.0), expectedGlide({3000, 0}, strong), 0.5);
}

TEST(Glide, SeededRandomSteps) {
    const auto seed = testSeed();
    std::mt19937_64 engine(seed);
    std::uniform_real_distribution<double> dtDist(0.001, 0.050);
    std::uniform_real_distribution<double> vDist(-6000.0, 6000.0);
    GlideConfig cfg;
    for (int i = 0; i < 200; ++i) {
        const cm::Vec v{vDist(engine), vDist(engine)};
        Glide glide;
        const bool started = glide.start(v, cfg);
        EXPECT_EQ(started, std::min(length(v), cfg.maxSpeed) >= cfg.minSpeed) << "seed=" << seed << " i=" << i;
        if (!started)
            continue;
        cm::Vec total;
        for (int n = 0; n < 100000 && glide.active(); ++n) {
            const auto step = glide.step(dtDist(engine));
            total.x += step.x;
            total.y += step.y;
        }
        expectVecNear(total, expectedGlide(v, cfg), 0.5);
    }
}

TEST(Glide, SpeedNeverIncreases) {
    GlideConfig cfg;
    Glide glide;
    ASSERT_TRUE(glide.start({2000, 1000}, cfg));
    double previous = std::numeric_limits<double>::infinity();
    const double dt = 1.0 / 60.0;
    while (glide.active()) {
        const double speed = length(glide.step(dt)) / dt;
        EXPECT_LE(speed, previous + 1e-9);
        previous = speed;
    }
}

TEST(Glide, StopAndNotStarted) {
    GlideConfig cfg;
    Glide idle;
    EXPECT_FALSE(idle.active());
    expectVecNear(idle.step(0.016), {0, 0}, 0);

    Glide glide;
    ASSERT_TRUE(glide.start({1000, 0}, cfg));
    EXPECT_TRUE(glide.active());
    glide.stop();
    EXPECT_FALSE(glide.active());
    expectVecNear(glide.step(0.016), {0, 0}, 0);
}

TEST(Glide, BelowMinSpeedOrStrengthZeroDoesNotStart) {
    GlideConfig cfg;
    Glide glide;
    EXPECT_FALSE(glide.start({99, 0}, cfg));
    EXPECT_FALSE(glide.active());
    cfg.strength = 0.0;
    EXPECT_FALSE(glide.start({1000, 0}, cfg));
}

TEST(Glide, InvalidConfigOrVelocityIsRefusedWithoutThrowing) {
    const auto refuses = [](GlideConfig cfg, cm::Vec v) {
        Glide glide;
        bool started = true;
        EXPECT_NO_THROW(started = glide.start(v, cfg));
        EXPECT_FALSE(started);
        EXPECT_FALSE(glide.active());
    };
    GlideConfig base;
    auto c1 = base; c1.strength = kNaN;      refuses(c1, {1000, 0});
    auto c2 = base; c2.stopSpeed = 0.0;      refuses(c2, {1000, 0});
    auto c3 = base; c3.stopSpeed = -1.0;     refuses(c3, {1000, 0});
    auto c4 = base; c4.maxSpeed = 1.0;       refuses(c4, {1000, 0});  // maxSpeed <= stopSpeed
    auto c5 = base; c5.minSpeed = -1.0;      refuses(c5, {1000, 0});
    auto c6 = base; c6.minSpeed = kNaN;      refuses(c6, {1000, 0});
    auto c7 = base; c7.maxSpeed = kNaN;      refuses(c7, {1000, 0});
    refuses(base, {kNaN, 0});
    refuses(base, {std::numeric_limits<double>::infinity(), 0});
}

TEST(Glide, NonPositiveOrNonFiniteDtDoesNotMove) {
    GlideConfig cfg;
    Glide glide;
    ASSERT_TRUE(glide.start({1000, 0}, cfg));
    expectVecNear(glide.step(0.0), {0, 0}, 0);
    expectVecNear(glide.step(-0.1), {0, 0}, 0);
    expectVecNear(glide.step(kNaN), {0, 0}, 0);
    EXPECT_TRUE(glide.active());
}

TEST(Glide, VelocityAboveMaxSpeedIsClamped) {
    GlideConfig cfg;
    Glide glide;
    ASSERT_TRUE(glide.start({9000, 0}, cfg));
    expectVecNear(runGlide(glide, 1.0 / 60.0), {5999.0 / 7.0, 0}, 0.5);
}

// ---- pinchCamera -------------------------------------------------------------------------------------------------

TEST(PinchCamera, AnchorWorldPointStaysUnderTheAnchor) {
    const auto seed = testSeed();
    std::mt19937_64 engine(seed);
    std::uniform_real_distribution<double> coord(-1.0e4, 1.0e4);
    std::uniform_real_distribution<double> zoom(0.1, 2.0);
    std::uniform_real_distribution<double> logScale(std::log(0.01), std::log(50.0));
    const cm::ZoomLimits limits{0.1, 2.0};
    for (int i = 0; i < 10000; ++i) {
        const cm::CameraState start{{coord(engine), coord(engine)}, zoom(engine)};
        const cm::Vec M{coord(engine), coord(engine)};
        const cm::Vec anchor{M.x + std::fabs(coord(engine)) / 5.0, M.y + std::fabs(coord(engine)) / 5.0};
        const double scale = std::exp(logScale(engine));
        const auto result = pinchCamera(start, M, anchor, scale, limits);
        const double expectedZoom = std::clamp(start.zoom * scale, limits.min, limits.max);
        EXPECT_DOUBLE_EQ(result.zoom, expectedZoom) << "seed=" << seed << " i=" << i;
        const auto world = cm::inverse(start, M, anchor);
        const auto back  = cm::forward(result, M, world);
        EXPECT_NEAR(back.x, anchor.x, 1e-6) << "seed=" << seed << " i=" << i;
        EXPECT_NEAR(back.y, anchor.y, 1e-6) << "seed=" << seed << " i=" << i;
    }
}

TEST(PinchCamera, ClampsAtBothLimitsAndKeepsTheAnchor) {
    const cm::CameraState start{{100, 50}, 0.5};
    const cm::Vec M{0, 0};
    const cm::Vec anchor{640, 400};
    const auto in  = pinchCamera(start, M, anchor, 50.0);   // 25 -> 2.0
    const auto out = pinchCamera(start, M, anchor, 0.01);   // 0.005 -> 0.1
    EXPECT_DOUBLE_EQ(in.zoom, 2.0);
    EXPECT_DOUBLE_EQ(out.zoom, 0.1);
    const auto world = cm::inverse(start, M, anchor);
    expectVecNear(cm::forward(in, M, world), anchor, 1e-9);
    expectVecNear(cm::forward(out, M, world), anchor, 1e-9);
}

TEST(PinchCamera, InvalidScaleLeavesTheCameraUnchanged) {
    const cm::CameraState start{{100, 50}, 0.5};
    for (double scale : {0.0, -1.0, kNaN, std::numeric_limits<double>::infinity()}) {
        const auto result = pinchCamera(start, {0, 0}, {640, 400}, scale);
        EXPECT_EQ(result.origin.x, start.origin.x) << scale;
        EXPECT_EQ(result.origin.y, start.origin.y) << scale;
        EXPECT_EQ(result.zoom, start.zoom) << scale;
    }
}
