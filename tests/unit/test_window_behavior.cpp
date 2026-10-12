// Unit tests for hyprvx::drift WindowBehavior, written from specs/004-drift-window-behavior/contracts/window-behavior.md
// 1.1 before the module existed (T003): snapMove (attach at threshold/zoom, exact gap, alignment, hold, release,
// tie-breaking, slow back-and-forth paths without alternation), groupOf, neighbour (V1 SpatialNavigator), the
// frame/center/reveal cameras (V1 frameRectInViewport, CameraMath), bookmarks and the view history (V1 CameraHistory).
// Expected values are computed by hand from the contract (shown in the comments), never from the code under test.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "hyprvx/core/CameraMath.hpp"
#include "hyprvx/core/WindowBehavior.hpp"

namespace cm = hyprvx::cammath;
using hyprvx::drift::AxisSnap;
using hyprvx::drift::Bookmarks;
using hyprvx::drift::centerCamera;
using hyprvx::drift::Direction;
using hyprvx::drift::frameCamera;
using hyprvx::drift::groupOf;
using hyprvx::drift::historyBack;
using hyprvx::drift::historyForward;
using hyprvx::drift::historyRecord;
using hyprvx::drift::makeViewHistory;
using hyprvx::drift::neighbour;
using hyprvx::drift::revealCamera;
using hyprvx::drift::snapMove;
using hyprvx::drift::SnapResult;
using hyprvx::drift::SnapSettings;
using hyprvx::drift::SnapState;
using hyprvx::drift::validSnapSettings;
using hyprvx::drift::VIEW_HISTORY_CAPACITY;
using hyprvx::drift::WindowId;
using hyprvx::drift::WorldBox;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kEps = 1e-9;
constexpr cm::Vec kMonitor{1280.0, 800.0};

std::uint64_t testSeed() {
    if (const char* value = std::getenv("HYPRVX_TEST_SEED"))
        return std::strtoull(value, nullptr, 10);
    return 20261012;
}

WorldBox box(WindowId id, double x, double y, double w = 100.0, double h = 100.0) {
    return {id, {{x, y}, {w, h}}};
}

// The moving window A (id 1) at the drag start, 100 x 100 at the origin.
std::vector<WorldBox> movingA() {
    return {box(1, 0.0, 0.0)};
}

void expectDelta(const SnapResult& r, double x, double y) {
    EXPECT_NEAR(r.delta.x, x, kEps);
    EXPECT_NEAR(r.delta.y, y, kEps);
}

void expectCamera(const cm::CameraState& c, double x, double y, double z) {
    EXPECT_NEAR(c.origin.x, x, 1e-9);
    EXPECT_NEAR(c.origin.y, y, 1e-9);
    EXPECT_NEAR(c.zoom, z, 1e-12);
}

bool sameState(const SnapState& a, const SnapState& b) {
    auto same = [](const AxisSnap& p, const AxisSnap& q) {
        return p.attached == q.attached && p.aligned == q.aligned && p.target == q.target && p.snapped == q.snapped;
    };
    return same(a.x, b.x) && same(a.y, b.y);
}

} // namespace

// ---- settings ----

TEST(WindowBehaviorSettings, ValidityRules) {
    EXPECT_TRUE(validSnapSettings(SnapSettings{}));
    EXPECT_TRUE(validSnapSettings(SnapSettings{true, 24.0, 0.0, 24.0})); // gap 0 and release == threshold allowed
    EXPECT_FALSE(validSnapSettings(SnapSettings{true, 0.0, 12.0, 32.0}));  // threshold > 0
    EXPECT_FALSE(validSnapSettings(SnapSettings{true, 24.0, -1.0, 32.0})); // gap >= 0
    EXPECT_FALSE(validSnapSettings(SnapSettings{true, 24.0, 12.0, 23.0})); // release >= threshold
    EXPECT_FALSE(validSnapSettings(SnapSettings{true, kNaN, 12.0, 32.0}));
    EXPECT_FALSE(validSnapSettings(SnapSettings{true, 24.0, std::numeric_limits<double>::infinity(), 32.0}));
}

// ---- snapMove: attach, gap, alignment ----
//
// A (0,0 100x100) moves right towards B (id 2, at x 200). Facing distance = 200 - (U.x + 100) = 100 - U.x; the snapped
// origin x is 200 - 12 - 100 = 88 and |distance - gap| = |88 - U.x|. With y overlapping, alignment then puts the tops
// together (|U.y - 0|, top edges first on a tie).

TEST(WindowBehaviorSnap, AttachesAtThresholdOverZoomOne) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    const auto free = snapMove(movingA(), {63.0, 0.0}, others, 1.0, SnapSettings{}, SnapState{});
    expectDelta(free, 63.0, 0.0); // |88 - 63| = 25 > 24
    EXPECT_FALSE(free.state.x.attached);
    EXPECT_FALSE(free.state.y.attached);

    const auto snapped = snapMove(movingA(), {64.0, 0.0}, others, 1.0, SnapSettings{}, SnapState{});
    expectDelta(snapped, 88.0, 0.0); // |88 - 64| = 24 <= 24
    EXPECT_TRUE(snapped.state.x.attached);
    EXPECT_EQ(snapped.state.x.target, 2u);
    EXPECT_DOUBLE_EQ(snapped.state.x.snapped, 88.0);
    EXPECT_TRUE(snapped.state.y.attached); // top edges aligned with B
    EXPECT_EQ(snapped.state.y.target, 2u);
    EXPECT_DOUBLE_EQ(snapped.state.y.snapped, 0.0);
    // The gap is exact: B.left - (A.right after the move) = 200 - (88 + 100) = 12.
    EXPECT_DOUBLE_EQ(200.0 - (0.0 + snapped.delta.x + 100.0), 12.0);
}

TEST(WindowBehaviorSnap, ThresholdScalesWithZoom) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    // zoom 2: threshold 24 screen px = 12 world px -> attach for |88 - U.x| <= 12.
    expectDelta(snapMove(movingA(), {75.0, 0.0}, others, 2.0, SnapSettings{}, SnapState{}), 75.0, 0.0);
    expectDelta(snapMove(movingA(), {76.0, 0.0}, others, 2.0, SnapSettings{}, SnapState{}), 88.0, 0.0);
    // zoom 0.5: 48 world px.
    expectDelta(snapMove(movingA(), {39.0, 0.0}, others, 0.5, SnapSettings{}, SnapState{}), 39.0, 0.0);
    expectDelta(snapMove(movingA(), {40.0, 0.0}, others, 0.5, SnapSettings{}, SnapState{}), 88.0, 0.0);
    // The gap stays 12 world px at every zoom.
    expectDelta(snapMove(movingA(), {100.0, 0.0}, others, 0.5, SnapSettings{}, SnapState{}), 88.0, 0.0);
}

TEST(WindowBehaviorSnap, SnapsOnTheOtherSideAndVertically) {
    // A moving left towards C (id 3, right edge at -50): distance = U.x - (-50), snapped x = -50 + 12 = -38.
    expectDelta(snapMove(movingA(), {-20.0, 0.0}, {box(3, -150.0, 0.0)}, 1.0, SnapSettings{}, SnapState{}), -38.0, 0.0);
    // A moving down towards D (id 4 at y 200): snapped y = 200 - 12 - 100 = 88; x aligns the left edges (|5 - 0| = 5).
    expectDelta(snapMove(movingA(), {5.0, 70.0}, {box(4, 0.0, 200.0)}, 1.0, SnapSettings{}, SnapState{}), 0.0, 88.0);
}

TEST(WindowBehaviorSnap, AlignmentPicksTheSmallerEdgeError) {
    // B (id 2) at (200,30) 100x100. Candidate (70,10): x snaps to 88; tops |10 - 30| = 20, bottoms |110 - 130| = 20:
    // the tie goes to the top edges -> y = 30.
    expectDelta(snapMove(movingA(), {70.0, 10.0}, {box(2, 200.0, 30.0)}, 1.0, SnapSettings{}, SnapState{}), 88.0, 30.0);
    // B at (200,30) 100x60 (bottom 90). Candidate (70,0): tops |0 - 30| = 30 > 24, bottoms |100 - 90| = 10 -> the
    // bottoms align: y = 90 - 100 = -10.
    expectDelta(snapMove(movingA(), {70.0, 0.0}, {box(2, 200.0, 30.0, 100.0, 60.0)}, 1.0, SnapSettings{}, SnapState{}),
                88.0, -10.0);
    // Neither edge within 24: y stays free. B at (200,50): tops |0 - 50| = 50, bottoms 50.
    const auto r = snapMove(movingA(), {70.0, 0.0}, {box(2, 200.0, 50.0)}, 1.0, SnapSettings{}, SnapState{});
    expectDelta(r, 88.0, 0.0);
    EXPECT_FALSE(r.state.y.attached);
}

TEST(WindowBehaviorSnap, NeedsOverlapOnTheOtherAxisOpenIntervals) {
    // B (id 2) at (200,100): A at y 0 spans [0,100], B [100,200]: touching, not overlapping -> no x snap.
    const auto touching = snapMove(movingA(), {70.0, 0.0}, {box(2, 200.0, 100.0)}, 1.0, SnapSettings{}, SnapState{});
    expectDelta(touching, 70.0, 0.0);
    EXPECT_FALSE(touching.state.x.attached);
    // Half a px of overlap is enough: B at (200,99.5); the bottoms are 99.5 apart and the tops 99.5: no alignment.
    expectDelta(snapMove(movingA(), {70.0, 0.0}, {box(2, 200.0, 99.5)}, 1.0, SnapSettings{}, SnapState{}), 88.0, 0.0);
}

TEST(WindowBehaviorSnap, MovingSetUsesItsBoundingBox) {
    // A (0,0) and A2 (0,112) move together: bounding box (0,0) 100x212. B (id 2) at (200,50) overlaps on y.
    const std::vector<WorldBox> moving{box(1, 0.0, 0.0), box(5, 0.0, 112.0)};
    const auto r = snapMove(moving, {70.0, 3.0}, {box(2, 200.0, 50.0)}, 1.0, SnapSettings{}, SnapState{});
    // x: snapped origin 88. y: tops |3 - 50| = 47, bottoms |215 - 150| = 65: no alignment.
    expectDelta(r, 88.0, 3.0);
}

TEST(WindowBehaviorSnap, NearestTargetWinsThenPreviousTargetThenLowerId) {
    // At zoom 0.25 the attach band is 96 world px. A between C (id 3, right edge -50: snapped x -38) and B (id 2,
    // snapped x 88).
    const std::vector<WorldBox> others{box(2, 200.0, 0.0), box(3, -150.0, 0.0)};
    // U.x = 20: errors |88 - 20| = 68 and |20 + 38| = 58 -> C.
    expectDelta(snapMove(movingA(), {20.0, 0.0}, others, 0.25, SnapSettings{}, SnapState{}), -38.0, 0.0);
    // U.x = 25: 63 and 63 -> tie, no previous target -> the lower id (B).
    expectDelta(snapMove(movingA(), {25.0, 0.0}, others, 0.25, SnapSettings{}, SnapState{}), 88.0, 0.0);
    // Same tie with a previous x target C whose snapped coordinate is far away (hold fails: |25 - 1000| > 128) -> C.
    SnapState previous;
    previous.x = {true, false, 3, 1000.0};
    expectDelta(snapMove(movingA(), {25.0, 0.0}, others, 0.25, SnapSettings{}, previous), -38.0, 0.0);
}

// ---- snapMove: hold and release (hysteresis) ----

TEST(WindowBehaviorSnap, HoldsUntilMoreThanReleaseAtZoomOne) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    const auto attached = snapMove(movingA(), {64.0, 0.0}, others, 1.0, SnapSettings{}, SnapState{});
    ASSERT_TRUE(attached.state.x.attached);
    // Release distance 32: hold while |U.x - 88| <= 32.
    expectDelta(snapMove(movingA(), {115.0, 0.0}, others, 1.0, SnapSettings{}, attached.state), 88.0, 0.0);
    expectDelta(snapMove(movingA(), {120.0, 0.0}, others, 1.0, SnapSettings{}, attached.state), 88.0, 0.0);
    expectDelta(snapMove(movingA(), {56.0, 0.0}, others, 1.0, SnapSettings{}, attached.state), 88.0, 0.0);
    // 33 away: released; the attach band (24) does not catch it again.
    const auto released = snapMove(movingA(), {121.0, 0.0}, others, 1.0, SnapSettings{}, attached.state);
    expectDelta(released, 121.0, 0.0);
    EXPECT_FALSE(released.state.x.attached);
    expectDelta(snapMove(movingA(), {55.0, 0.0}, others, 1.0, SnapSettings{}, attached.state), 55.0, 0.0);
}

TEST(WindowBehaviorSnap, ReleaseScalesWhenZoomedOut) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    // zoom 0.5: attach band 48, release max(32, 32 / 0.5) = 64.
    const auto attached = snapMove(movingA(), {40.0, 0.0}, others, 0.5, SnapSettings{}, SnapState{});
    ASSERT_TRUE(attached.state.x.attached);
    expectDelta(snapMove(movingA(), {152.0, 0.0}, others, 0.5, SnapSettings{}, attached.state), 88.0, 0.0);
    const auto released = snapMove(movingA(), {153.0, 0.0}, others, 0.5, SnapSettings{}, attached.state);
    expectDelta(released, 153.0, 0.0); // 65 > 64 and 65 > 48: free
    // zoom 2: release stays 32 world px (max(32, 16)).
    const auto z2 = snapMove(movingA(), {80.0, 0.0}, others, 2.0, SnapSettings{}, SnapState{});
    ASSERT_TRUE(z2.state.x.attached);
    expectDelta(snapMove(movingA(), {120.0, 0.0}, others, 2.0, SnapSettings{}, z2.state), 88.0, 0.0);
    expectDelta(snapMove(movingA(), {121.0, 0.0}, others, 2.0, SnapSettings{}, z2.state), 121.0, 0.0);
}

TEST(WindowBehaviorSnap, AlignmentIsHeldToo) {
    // B (id 2) at (200,0). Candidate (70,20): x snaps to 88, tops |20 - 0| = 20 -> y = 0 recorded in y.
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    const auto aligned = snapMove(movingA(), {70.0, 20.0}, others, 1.0, SnapSettings{}, SnapState{});
    expectDelta(aligned, 88.0, 0.0);
    ASSERT_TRUE(aligned.state.y.attached);
    EXPECT_EQ(aligned.state.y.target, 2u);
    // y = 30 is outside the alignment band (24) but within release (32): held at 0.
    expectDelta(snapMove(movingA(), {70.0, 30.0}, others, 1.0, SnapSettings{}, aligned.state), 88.0, 0.0);
    // y = 33: released.
    expectDelta(snapMove(movingA(), {70.0, 33.0}, others, 1.0, SnapSettings{}, aligned.state), 88.0, 33.0);
    EXPECT_FALSE(aligned.state.x.aligned); // x is the gap snap
    EXPECT_TRUE(aligned.state.y.aligned);
    // When the x gap snap releases (|121 - 88| = 33 > 32), the y alignment ends with it: y returns to the candidate.
    const auto both = snapMove(movingA(), {121.0, 20.0}, others, 1.0, SnapSettings{}, aligned.state);
    expectDelta(both, 121.0, 20.0);
    EXPECT_TRUE(sameState(both.state, SnapState{}));
    // An alignment never follows another alignment: a lone y alignment state does not hold y without a gap snap on x.
    SnapState lone;
    lone.y = {true, true, 2, 0.0};
    expectDelta(snapMove(movingA(), {400.0, 10.0}, {box(2, 200.0, 0.0)}, 1.0, SnapSettings{}, lone), 400.0, 10.0);
}

TEST(WindowBehaviorSnap, HeldTargetGoneReleasesTheAxis) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    const auto attached = snapMove(movingA(), {64.0, 0.0}, others, 1.0, SnapSettings{}, SnapState{});
    ASSERT_TRUE(attached.state.x.attached);
    const auto gone = snapMove(movingA(), {100.0, 0.0}, {}, 1.0, SnapSettings{}, attached.state);
    expectDelta(gone, 100.0, 0.0);
    EXPECT_FALSE(gone.state.x.attached);
    EXPECT_FALSE(gone.state.y.attached);
}

namespace {

// One target, one axis: the contract's state machine restated as the oracle for scripted paths. attached -> hold while
// |U - 88| <= release(z); free -> attach when |U - 88| <= threshold / z.
struct OneAxisOracle {
    double attach, release;
    bool attached = false;
    double step(double u) {
        const double err = std::abs(u - 88.0);
        attached = attached ? err <= release : err <= attach;
        return attached ? 88.0 : u;
    }
};

// Runs a path of candidate x values and checks (a) every result equals the oracle, so state changes happen only at the
// contract's snap and release points, and (b) no alternation: the state never changes on two consecutive steps (FR-004).
// A run in one direction may contain one snap and one release (in, then out of the band).
void checkPath(const std::vector<double>& path, double zoom, double attach, double release, const char* label) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    OneAxisOracle oracle{attach, release};
    SnapState state;
    bool previousAttached = false, previousChanged = false;
    int changes = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto r = snapMove(movingA(), {path[i], 0.0}, others, zoom, SnapSettings{}, state);
        state = r.state;
        const double expected = oracle.step(path[i]);
        ASSERT_NEAR(r.delta.x, expected, kEps) << label << " step " << i << " u=" << path[i];
        const bool changed = i > 0 && state.x.attached != previousAttached;
        if (changed) {
            ++changes;
            ASSERT_FALSE(previousChanged) << label << " alternation at step " << i << " u=" << path[i];
        }
        previousChanged  = changed;
        previousAttached = state.x.attached;
    }
    EXPECT_GE(changes, 10) << label;
}

std::vector<double> backAndForth(double low, double high, int passes) {
    std::vector<double> path;
    for (int pass = 0; pass < passes; ++pass) {
        for (double u = low; u <= high; u += 2.0)
            path.push_back(u);
        for (double u = high; u >= low; u -= 2.0)
            path.push_back(u);
    }
    return path;
}

} // namespace

TEST(WindowBehaviorSnap, SlowBackAndForthPathsNeverAlternate) {
    // 2-px steps across both band edges (attach on the way in, release on the way out), 6 passes each way.
    checkPath(backAndForth(20.0, 160.0, 6), 1.0, 24.0, 32.0, "zoom 1");
    checkPath(backAndForth(20.0, 160.0, 6), 2.0, 12.0, 32.0, "zoom 2");
    checkPath(backAndForth(-40.0, 220.0, 6), 0.5, 48.0, 64.0, "zoom 0.5"); // the 1.0 rule alternated here
}

TEST(WindowBehaviorSnap, RandomSlowWalksMatchTheOracle) {
    const auto seed = testSeed();
    std::mt19937_64 engine(seed);
    std::uniform_int_distribution<int> coin(0, 1);
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    for (double zoom : {0.5, 1.0, 2.0}) {
        OneAxisOracle oracle{24.0 / zoom, std::max(32.0, 32.0 / zoom)};
        SnapState state;
        double u = 88.0;
        for (int i = 0; i < 4000; ++i) {
            u += coin(engine) ? 2.0 : -2.0;
            const auto r = snapMove(movingA(), {u, 0.0}, others, zoom, SnapSettings{}, state);
            state = r.state;
            ASSERT_NEAR(r.delta.x, oracle.step(u), kEps) << "seed=" << seed << " zoom=" << zoom << " i=" << i;
        }
    }
}

TEST(WindowBehaviorSnap, DeterministicForTheSameInput) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0), box(3, -150.0, 0.0), box(4, 0.0, 200.0)};
    const auto a = snapMove(movingA(), {30.0, 60.0}, others, 0.75, SnapSettings{}, SnapState{});
    const auto b = snapMove(movingA(), {30.0, 60.0}, others, 0.75, SnapSettings{}, SnapState{});
    EXPECT_EQ(a.delta.x, b.delta.x);
    EXPECT_EQ(a.delta.y, b.delta.y);
    EXPECT_TRUE(sameState(a.state, b.state));
}

TEST(WindowBehaviorSnap, DisabledInvalidOrNonFiniteReturnsTheCandidate) {
    const std::vector<WorldBox> others{box(2, 200.0, 0.0)};
    SnapState held;
    held.x = {true, false, 2, 88.0};
    auto unchanged = [&](const SnapResult& r, double x, double y) {
        EXPECT_EQ(r.delta.x, x);
        EXPECT_EQ(r.delta.y, y);
        EXPECT_TRUE(sameState(r.state, SnapState{}));
    };
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, 1.0, SnapSettings{false, 24.0, 12.0, 32.0}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, 1.0, SnapSettings{true, 0.0, 12.0, 32.0}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, 1.0, SnapSettings{true, 24.0, -1.0, 32.0}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, 1.0, SnapSettings{true, 24.0, 12.0, 10.0}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, 0.0, SnapSettings{}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, others, kNaN, SnapSettings{}, held), 70.0, 0.0);
    unchanged(snapMove({}, {70.0, 0.0}, others, 1.0, SnapSettings{}, held), 70.0, 0.0);
    unchanged(snapMove(movingA(), {70.0, 0.0}, {box(2, kNaN, 0.0)}, 1.0, SnapSettings{}, held), 70.0, 0.0);
    const auto nan = snapMove(movingA(), {kNaN, 0.0}, others, 1.0, SnapSettings{}, held);
    EXPECT_TRUE(std::isnan(nan.delta.x));
    EXPECT_EQ(nan.delta.y, 0.0);
    EXPECT_TRUE(sameState(nan.state, SnapState{}));
}

// ---- groupOf ----

TEST(WindowBehaviorGroup, ChainsTolerancesAndOverlap) {
    // A(1) at 0, B(2) 12 to its right, C(3) 12 to B's right: a chain. E(5) 12.9 below A: within gap +- 1. D(4) far
    // away. F(6) 13.1 below C: outside (and 12 to B's right but not overlapping B on y). G(7) 12 to E's right on x but
    // far below it on y.
    const std::vector<WorldBox> boxes{box(1, 0.0, 0.0),     box(2, 112.0, 0.0),   box(3, 224.0, 0.0),
                                      box(4, 600.0, 600.0), box(5, 0.0, 112.9),   box(6, 224.0, 113.1),
                                      box(7, 112.0, 400.0)};
    EXPECT_EQ(groupOf(1, boxes, 12.0), (std::vector<WindowId>{1, 2, 3, 5}));
    EXPECT_EQ(groupOf(3, boxes, 12.0), (std::vector<WindowId>{1, 2, 3, 5}));
    EXPECT_EQ(groupOf(4, boxes, 12.0), (std::vector<WindowId>{4}));
    EXPECT_EQ(groupOf(6, boxes, 12.0), (std::vector<WindowId>{6}));
    EXPECT_EQ(groupOf(7, boxes, 12.0), (std::vector<WindowId>{7}));
    EXPECT_TRUE(groupOf(99, boxes, 12.0).empty());
    // 11.1 is within the tolerance on the low side; 10.9 is not.
    EXPECT_EQ(groupOf(1, {box(1, 0.0, 0.0), box(2, 111.1, 0.0)}, 12.0), (std::vector<WindowId>{1, 2}));
    EXPECT_EQ(groupOf(1, {box(1, 0.0, 0.0), box(2, 110.9, 0.0)}, 12.0), (std::vector<WindowId>{1}));
    // Facing edges at the gap but only touching on the other axis: no neighbour.
    EXPECT_EQ(groupOf(1, {box(1, 0.0, 0.0), box(2, 112.0, 100.0)}, 12.0), (std::vector<WindowId>{1}));
    // Gap 0 (flush windows), seed listed second.
    EXPECT_EQ(groupOf(2, {box(1, 0.0, 0.0), box(2, 100.0, 50.0)}, 0.0), (std::vector<WindowId>{1, 2}));
}

// ---- neighbour (V1 SpatialNavigator, default policy) ----

TEST(WindowBehaviorNavigation, DirectionalNeighbour) {
    // A(1) centre (50,50); B(2) centre (350,50) right; C(3) centre (50,350) down; D(4) centre (350,350) diagonal.
    const std::vector<WorldBox> boxes{box(1, 0.0, 0.0), box(2, 300.0, 0.0), box(3, 0.0, 300.0), box(4, 300.0, 300.0)};
    EXPECT_EQ(neighbour(1, Direction::Right, boxes), std::optional<WindowId>{2});
    EXPECT_EQ(neighbour(1, Direction::Down, boxes), std::optional<WindowId>{3});
    EXPECT_EQ(neighbour(1, Direction::Left, boxes), std::nullopt);
    EXPECT_EQ(neighbour(1, Direction::Up, boxes), std::nullopt);
    EXPECT_EQ(neighbour(2, Direction::Left, boxes), std::optional<WindowId>{1});
    EXPECT_EQ(neighbour(2, Direction::Down, boxes), std::optional<WindowId>{4});
    EXPECT_EQ(neighbour(4, Direction::Up, boxes), std::optional<WindowId>{2});
    EXPECT_EQ(neighbour(99, Direction::Right, boxes), std::nullopt);
    // Outside the 65-degree cone: E(5) centre (150,450) from A is atan(400/100) = 76 degrees off the x axis.
    EXPECT_EQ(neighbour(1, Direction::Right, {box(1, 0.0, 0.0), box(5, 100.0, 400.0)}), std::nullopt);
}

// ---- cameras ----

TEST(WindowBehaviorCamera, FrameFitsWithMarginAndClamps) {
    // Monitor 1280x800, margin 5% of 800 = 40 px: available 1200 x 720.
    // Box (100,100) 600x300: fit = min(2.0, 1200/600, 720/300) = 2.0; visible 640x400; origin = (400,250)-(320,200).
    expectCamera(frameCamera({{100.0, 100.0}, {600.0, 300.0}}, kMonitor, 0.05), 80.0, 50.0, 2.0);
    // Box (0,0) 2400x600: fit = min(0.5, 1.2) = 0.5; visible 2560x1600; origin = (1200,300)-(1280,800).
    expectCamera(frameCamera({{0.0, 0.0}, {2400.0, 600.0}}, kMonitor, 0.05), -80.0, -500.0, 0.5);
    // Box 20000x100 at (0,0): 0.06 clamped to 0.1; visible 12800x8000; origin = (10000,50)-(6400,4000).
    expectCamera(frameCamera({{0.0, 0.0}, {20000.0, 100.0}}, kMonitor, 0.05), 3600.0, -3950.0, 0.1);
    // Box 10x10 at (0,0): clamped to 2.0; origin = (5,5)-(320,200).
    expectCamera(frameCamera({{0.0, 0.0}, {10.0, 10.0}}, kMonitor, 0.05), -315.0, -195.0, 2.0);
    // Margin 0: the monitor-sized box at (0,0) frames at zoom 1, origin (0,0).
    expectCamera(frameCamera({{0.0, 0.0}, {1280.0, 800.0}}, kMonitor, 0.0), 0.0, 0.0, 1.0);
    // The box centre lands on the screen centre: forward(c, M, centre) - M = monitor / 2 for any M.
    const auto c = frameCamera({{100.0, 100.0}, {600.0, 300.0}}, kMonitor, 0.05);
    const cm::Vec M{1920.0, 0.0};
    const auto s = cm::forward(c, M, {400.0, 250.0});
    EXPECT_NEAR(s.x - M.x, 640.0, 1e-9);
    EXPECT_NEAR(s.y - M.y, 400.0, 1e-9);
}

TEST(WindowBehaviorCamera, FrameInvalidInputDoesNotThrow) {
    expectCamera(frameCamera({{7.0, 9.0}, {100.0, 100.0}}, {0.0, 800.0}, 0.05), 7.0, 9.0, 1.0);
    expectCamera(frameCamera({{7.0, 9.0}, {100.0, 100.0}}, kMonitor, 0.5), 7.0, 9.0, 1.0);
    expectCamera(frameCamera({{7.0, 9.0}, {100.0, 100.0}}, kMonitor, -0.1), 7.0, 9.0, 1.0);
    EXPECT_NO_THROW((void)frameCamera({{kNaN, 9.0}, {100.0, 100.0}}, kMonitor, 0.05));
}

TEST(WindowBehaviorCamera, CenterKeepsZoom) {
    // c origin (100,50) zoom 0.5; box (1000,1000) 200x100, centre (1100,1050). Origin = centre - (640,400)/0.5.
    expectCamera(centerCamera({{100.0, 50.0}, 0.5}, kMonitor, {{1000.0, 1000.0}, {200.0, 100.0}}), -180.0, 250.0, 0.5);
    expectCamera(centerCamera({{0.0, 0.0}, 2.0}, kMonitor, {{0.0, 0.0}, {100.0, 100.0}}), -270.0, -150.0, 2.0);
    // Invalid input returns the camera unchanged.
    expectCamera(centerCamera({{3.0, 4.0}, 1.0}, {0.0, 0.0}, {{0.0, 0.0}, {100.0, 100.0}}), 3.0, 4.0, 1.0);
}

TEST(WindowBehaviorCamera, RevealPansTheLeastOrFrames) {
    const cm::CameraState id{{0.0, 0.0}, 1.0};
    // Fully inside the 40-px inset: unchanged.
    expectCamera(revealCamera(id, kMonitor, {{100.0, 100.0}, {200.0, 100.0}}, 0.05), 0.0, 0.0, 1.0);
    // Right edge at 1400 > 1240: pan the content left by 160 -> origin.x = 160.
    expectCamera(revealCamera(id, kMonitor, {{1200.0, 100.0}, {200.0, 100.0}}, 0.05), 160.0, 0.0, 1.0);
    // Left edge at -50 < 40: pan right by 90 -> origin.x = -90.
    expectCamera(revealCamera(id, kMonitor, {{-50.0, 100.0}, {200.0, 100.0}}, 0.05), -90.0, 0.0, 1.0);
    // Both axes: box (1200,780) 200x100 -> dx = 1240-1400 = -160, dy = 760-880 = -120.
    expectCamera(revealCamera(id, kMonitor, {{1200.0, 780.0}, {200.0, 100.0}}, 0.05), 160.0, 120.0, 1.0);
    // Zoom 2: box (500,100) 200x100 is (1000,200) 400x200 on screen; dx = -160 -> origin.x = 160/2.
    expectCamera(revealCamera({{0.0, 0.0}, 2.0}, kMonitor, {{500.0, 100.0}, {200.0, 100.0}}, 0.05), 80.0, 0.0, 2.0);
    // Too large at the current zoom (2000 > 1200): frame instead (fit = min(1200/2000, 720/100) = 0.6).
    const cm::Box large{{0.0, 0.0}, {2000.0, 100.0}};
    const auto framed = frameCamera(large, kMonitor, 0.05);
    EXPECT_NEAR(framed.zoom, 0.6, 1e-12);
    const auto revealed = revealCamera(id, kMonitor, large, 0.05);
    expectCamera(revealed, framed.origin.x, framed.origin.y, framed.zoom);
    // Invalid input returns the camera unchanged.
    expectCamera(revealCamera(id, kMonitor, {{1200.0, 100.0}, {200.0, 100.0}}, 0.9), 0.0, 0.0, 1.0);
}

// ---- bookmarks ----

TEST(WindowBehaviorBookmarks, SlotsOneToFour) {
    Bookmarks marks;
    const cm::CameraState a{{10.0, 20.0}, 0.5}, b{{-5.0, 7.0}, 2.0};
    EXPECT_EQ(marks.recall(2), std::nullopt);
    marks.store(2, a);
    ASSERT_TRUE(marks.recall(2).has_value());
    expectCamera(*marks.recall(2), 10.0, 20.0, 0.5);
    EXPECT_EQ(marks.recall(1), std::nullopt);
    marks.store(2, b); // overwrite
    expectCamera(*marks.recall(2), -5.0, 7.0, 2.0);
    marks.store(4, a);
    expectCamera(*marks.recall(4), 10.0, 20.0, 0.5);
    marks.store(0, a);
    marks.store(5, a);
    EXPECT_EQ(marks.recall(0), std::nullopt);
    EXPECT_EQ(marks.recall(5), std::nullopt);
    EXPECT_EQ(marks.recall(-1), std::nullopt);
}

// ---- view history (V1 CameraHistory) ----

TEST(WindowBehaviorHistory, BrowserLikeWithCapacity32) {
    EXPECT_EQ(VIEW_HISTORY_CAPACITY, 32u);
    auto history = makeViewHistory();
    const cm::CameraState a{{1.0, 0.0}, 1.0}, b{{2.0, 0.0}, 0.5}, c{{3.0, 0.0}, 2.0}, d{{4.0, 0.0}, 1.0};
    EXPECT_TRUE(historyRecord(history, a)); // jump a -> b
    EXPECT_TRUE(historyRecord(history, b)); // jump b -> c
    auto back = historyBack(history, c);
    ASSERT_TRUE(back.has_value());
    expectCamera(*back, 2.0, 0.0, 0.5);
    back = historyBack(history, b);
    ASSERT_TRUE(back.has_value());
    expectCamera(*back, 1.0, 0.0, 1.0);
    EXPECT_EQ(historyBack(history, a), std::nullopt);
    auto fwd = historyForward(history, a);
    ASSERT_TRUE(fwd.has_value());
    expectCamera(*fwd, 2.0, 0.0, 0.5);
    // A new jump from b clears forward.
    EXPECT_TRUE(historyRecord(history, b));
    EXPECT_EQ(historyForward(history, d), std::nullopt);
    // Invalid cameras are refused without throwing.
    EXPECT_FALSE(historyRecord(history, {{kNaN, 0.0}, 1.0}));
    EXPECT_FALSE(historyRecord(history, {{0.0, 0.0}, 0.0}));
    EXPECT_EQ(historyBack(history, {{kNaN, 0.0}, 1.0}), std::nullopt);
}

TEST(WindowBehaviorHistory, KeepsTheNewest32) {
    auto history = makeViewHistory();
    for (int i = 0; i < 40; ++i)
        ASSERT_TRUE(historyRecord(history, {{static_cast<double>(i), 0.0}, 1.0}));
    EXPECT_EQ(history.backSize(), 32u);
    cm::CameraState current{{100.0, 0.0}, 1.0};
    for (int i = 39; i >= 8; --i) {
        const auto back = historyBack(history, current);
        ASSERT_TRUE(back.has_value()) << i;
        EXPECT_DOUBLE_EQ(back->origin.x, static_cast<double>(i));
        current = *back;
    }
    EXPECT_EQ(historyBack(history, current), std::nullopt);
    EXPECT_EQ(history.backSize() + history.forwardSize(), 32u);
}
