// Property tests for core/include/hyprvx/core/CameraMath.hpp, written from
// specs/001-native-spatial-camera/contracts/transform-contract.md v1.1 (invariants 1-7).
// Expected values come from an independent long double oracle or hand-computed dyadic values.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "hyprvx/core/Camera.hpp"
#include "hyprvx/core/CameraMath.hpp"

namespace cm = hyprvx::cammath;

namespace {

constexpr int kCases = 10000;

std::uint64_t testSeed() {
    if (const char* value = std::getenv("HYPRVX_TEST_SEED"))
        return std::strtoull(value, nullptr, 10);
    return 20261011;
}

struct Random {
    explicit Random(std::uint64_t seed) : engine(seed) {}

    double coordinate() { return std::uniform_real_distribution<double>(-1.0e5, 1.0e5)(engine); }
    double zoom() { return std::uniform_real_distribution<double>(0.1, 2.0)(engine); }
    double size() { return std::uniform_real_distribution<double>(0.0, 4000.0)(engine); }
    cm::Vec point() { return {coordinate(), coordinate()}; }

    std::mt19937_64 engine;
};

std::string describe(std::uint64_t seed, int i, const cm::CameraState& c, cm::Vec M, cm::Vec W) {
    std::ostringstream out;
    out.precision(17);
    out << "seed=" << seed << " case=" << i << " P=(" << c.origin.x << "," << c.origin.y << ") z=" << c.zoom << " M=("
        << M.x << "," << M.y << ") W=(" << W.x << "," << W.y << ")";
    return out.str();
}

// Oracle in long double, written from the contract equations.
struct LVec {
    long double x, y;
};

LVec oracleForward(const cm::CameraState& c, cm::Vec M, cm::Vec W) {
    const long double z = c.zoom;
    return {M.x + (static_cast<long double>(W.x) - c.origin.x) * z, M.y + (static_cast<long double>(W.y) - c.origin.y) * z};
}

LVec oracleInverse(const cm::CameraState& c, cm::Vec M, cm::Vec S) {
    const long double z = c.zoom;
    return {c.origin.x + (static_cast<long double>(S.x) - M.x) / z, c.origin.y + (static_cast<long double>(S.y) - M.y) / z};
}

double tolerance(double magnitude) {
    return 1e-9 * std::max(1.0, std::fabs(magnitude));
}

} // namespace

TEST(CameraMathTest, ForwardAndInverseMatchTheOracle) {
    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::CameraState c{rng.point(), rng.zoom()};
        const cm::Vec M = rng.point();
        const cm::Vec W = rng.point();
        const auto S = cm::forward(c, M, W);
        const auto expected = oracleForward(c, M, W);
        ASSERT_NEAR(S.x, static_cast<double>(expected.x), tolerance(S.x)) << describe(seed, i, c, M, W);
        ASSERT_NEAR(S.y, static_cast<double>(expected.y), tolerance(S.y)) << describe(seed, i, c, M, W);
        const auto back = cm::inverse(c, M, W);
        const auto expectedBack = oracleInverse(c, M, W);
        ASSERT_NEAR(back.x, static_cast<double>(expectedBack.x), tolerance(back.x)) << describe(seed, i, c, M, W);
        ASSERT_NEAR(back.y, static_cast<double>(expectedBack.y), tolerance(back.y)) << describe(seed, i, c, M, W);
    }
}

// Invariant 1.
TEST(CameraMathTest, InverseUndoesForward) {
    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::CameraState c{rng.point(), rng.zoom()};
        const cm::Vec M = rng.point();
        const cm::Vec W = rng.point();
        const auto round = cm::inverse(c, M, cm::forward(c, M, W));
        ASSERT_NEAR(round.x, W.x, tolerance(W.x)) << describe(seed, i, c, M, W);
        ASSERT_NEAR(round.y, W.y, tolerance(W.y)) << describe(seed, i, c, M, W);
    }
}

TEST(CameraMathTest, DyadicHandValues) {
    // P = (100, 50), z = 0.5, M = (1920, 0): forward(W = (300, 250)) = (1920 + 200*0.5, 0 + 200*0.5) = (2020, 100).
    const cm::CameraState c{{100.0, 50.0}, 0.5};
    const cm::Vec M{1920.0, 0.0};
    const auto S = cm::forward(c, M, {300.0, 250.0});
    EXPECT_EQ(S.x, 2020.0);
    EXPECT_EQ(S.y, 100.0);
    const auto W = cm::inverse(c, M, {2020.0, 100.0});
    EXPECT_EQ(W.x, 300.0);
    EXPECT_EQ(W.y, 250.0);
    // visibleWorldRect for a 1280x800 monitor at z = 0.5 is P + 2560x1600.
    const auto V = cm::visibleWorldRect(c, {1280.0, 800.0});
    EXPECT_EQ(V.pos.x, 100.0);
    EXPECT_EQ(V.size.x, 2560.0);
    EXPECT_EQ(V.size.y, 1600.0);
    // panByScreen d = (10, -4) at z = 0.5 moves P by (-20, +8).
    const auto panned = cm::panByScreen(c, {10.0, -4.0});
    EXPECT_EQ(panned.origin.x, 80.0);
    EXPECT_EQ(panned.origin.y, 58.0);
    // surfaceLocal: inverse(S) - origin, unscaled.
    const auto local = cm::surfaceLocal(c, M, {2020.0, 100.0}, {250.0, 200.0});
    EXPECT_EQ(local.x, 50.0);
    EXPECT_EQ(local.y, 50.0);
}

// Invariant 2.
TEST(CameraMathTest, AnchorZoomKeepsTheAnchoredWorldPoint) {
    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::CameraState c{rng.point(), rng.zoom()};
        const cm::Vec M = rng.point();
        const cm::Vec a{M.x + rng.size(), M.y + rng.size()};
        const double zNew = rng.zoom();
        const auto Wa = cm::inverse(c, M, a);
        const auto next = cm::anchorZoom(c, M, a, zNew, {});
        ASSERT_EQ(next.zoom, zNew) << describe(seed, i, c, M, a);
        const auto S = cm::forward(next, M, Wa);
        ASSERT_NEAR(S.x, a.x, tolerance(a.x) * 10) << describe(seed, i, c, M, a);
        ASSERT_NEAR(S.y, a.y, tolerance(a.y) * 10) << describe(seed, i, c, M, a);
    }
}

TEST(CameraMathTest, AnchorZoomClampsAndRejectsNonFiniteInput) {
    const cm::CameraState c{{5.0, 6.0}, 1.5};
    EXPECT_EQ(cm::anchorZoom(c, {}, {1.0, 1.0}, 9.0, {}).zoom, 2.0);
    EXPECT_EQ(cm::anchorZoom(c, {}, {1.0, 1.0}, 0.01, {}).zoom, 0.1);
    EXPECT_EQ(cm::anchorZoom(c, {}, {1.0, 1.0}, -3.0, {}).zoom, 0.1);
    // A clamped zoom still keeps the anchor fixed, under the clamped value.
    const cm::Vec M{100.0, 40.0};
    const cm::Vec a{612.0, 388.0};
    const auto Wa = cm::inverse(c, M, a);
    for (const double zNew : {9.0, 0.01}) {
        const auto clamped = cm::anchorZoom(c, M, a, zNew, {});
        const auto S = cm::forward(clamped, M, Wa);
        EXPECT_NEAR(S.x, a.x, 1e-9 * a.x) << "zNew=" << zNew;
        EXPECT_NEAR(S.y, a.y, 1e-9 * a.y) << "zNew=" << zNew;
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const auto& result : {cm::anchorZoom(c, {}, {nan, 1.0}, 1.0, {}), cm::anchorZoom(c, {}, {1.0, inf}, 1.0, {}),
                               cm::anchorZoom(c, {}, {1.0, 1.0}, nan, {}), cm::anchorZoom(c, {}, {1.0, 1.0}, inf, {})}) {
        EXPECT_EQ(result.origin.x, 5.0);
        EXPECT_EQ(result.origin.y, 6.0);
        EXPECT_EQ(result.zoom, 1.5);
    }
}

// Invariant 3.
TEST(CameraMathTest, BoxesRoundTrip) {
    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::CameraState c{rng.point(), rng.zoom()};
        const cm::Vec M = rng.point();
        const cm::Box B{rng.point(), {rng.size(), rng.size()}};
        const auto R = cm::forwardBox(c, M, B);
        ASSERT_NEAR(R.size.x, B.size.x * c.zoom, tolerance(R.size.x)) << describe(seed, i, c, M, B.pos);
        const auto back = cm::inverseBox(c, M, R);
        ASSERT_NEAR(back.pos.x, B.pos.x, tolerance(B.pos.x)) << describe(seed, i, c, M, B.pos);
        ASSERT_NEAR(back.pos.y, B.pos.y, tolerance(B.pos.y)) << describe(seed, i, c, M, B.pos);
        ASSERT_NEAR(back.size.x, B.size.x, tolerance(B.size.x)) << describe(seed, i, c, M, B.pos);
        ASSERT_NEAR(back.size.y, B.size.y, tolerance(B.size.y)) << describe(seed, i, c, M, B.pos);
    }
}

// Invariant 4: exact, including non-zero M.
TEST(CameraMathTest, IdentityIsExact) {
    const cm::Vec M{1920.0, 0.0};
    const auto id = cm::identityFor(M);
    EXPECT_TRUE(cm::isIdentity(id, M));
    const cm::Vec W{0.1, 0.2};
    EXPECT_EQ(cm::forward(id, M, W).x, 0.1);
    EXPECT_EQ(cm::forward(id, M, W).y, 0.2);
    EXPECT_EQ(cm::inverse(id, M, W).x, 0.1);
    EXPECT_EQ(cm::inverse(id, M, W).y, 0.2);
    EXPECT_EQ(cm::surfaceLocal(id, M, W, {0.05, 0.0}).x, 0.1 - 0.05);
    const cm::Box B{{0.1, 0.3}, {800.7, 600.1}};
    EXPECT_EQ(cm::forwardBox(id, M, B).pos.x, B.pos.x);
    EXPECT_EQ(cm::forwardBox(id, M, B).size.y, B.size.y);
    EXPECT_EQ(cm::inverseBox(id, M, B).pos.y, B.pos.y);

    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::Vec Mi = rng.point();
        const cm::Vec Wi = rng.point();
        const auto c = cm::identityFor(Mi);
        ASSERT_EQ(cm::forward(c, Mi, Wi).x, Wi.x) << describe(seed, i, c, Mi, Wi);
        ASSERT_EQ(cm::forward(c, Mi, Wi).y, Wi.y) << describe(seed, i, c, Mi, Wi);
        ASSERT_EQ(cm::inverse(c, Mi, Wi).x, Wi.x) << describe(seed, i, c, Mi, Wi);
        ASSERT_EQ(cm::inverse(c, Mi, Wi).y, Wi.y) << describe(seed, i, c, Mi, Wi);
    }
}

// Invariant 5.
TEST(CameraMathTest, ValidateRejectsEveryInvalidCamera) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 1.0}, {}), cm::Error::None);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 0.1}, {}), cm::Error::None);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 2.0}, {}), cm::Error::None);
    EXPECT_EQ(cm::validate({{nan, 0.0}, 1.0}, {}), cm::Error::NonFinite);
    EXPECT_EQ(cm::validate({{0.0, inf}, 1.0}, {}), cm::Error::NonFinite);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, nan}, {}), cm::Error::NonFinite);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, -inf}, {}), cm::Error::NonFinite);
    EXPECT_EQ(cm::validate({{nan, 0.0}, -1.0}, {}), cm::Error::NonFinite); // NonFinite takes precedence
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 0.0}, {}), cm::Error::NonPositiveZoom);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, -1.0}, {}), cm::Error::NonPositiveZoom);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 0.099}, {}), cm::Error::ZoomOutOfRange);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 2.001}, {}), cm::Error::ZoomOutOfRange);
    EXPECT_EQ(cm::validate({{0.0, 0.0}, 3.0}, {0.1, 4.0}), cm::Error::None);
}

// Invariant 6.
TEST(CameraMathTest, DamageBoxContainsTheForwardBoxOnIntegerEdges) {
    const auto seed = testSeed();
    Random rng(seed);
    for (int i = 0; i < kCases; ++i) {
        const cm::CameraState c{rng.point(), rng.zoom()};
        const cm::Vec M = rng.point();
        const cm::Box B{rng.point(), {rng.size(), rng.size()}};
        const auto R = cm::forwardBox(c, M, B);
        const auto D = cm::damageBox(c, M, B);
        ASSERT_EQ(D.pos.x, std::floor(D.pos.x)) << describe(seed, i, c, M, B.pos);
        ASSERT_EQ(D.pos.y, std::floor(D.pos.y)) << describe(seed, i, c, M, B.pos);
        ASSERT_EQ(D.size.x, std::floor(D.size.x)) << describe(seed, i, c, M, B.pos);
        ASSERT_EQ(D.size.y, std::floor(D.size.y)) << describe(seed, i, c, M, B.pos);
        ASSERT_LE(D.pos.x, R.pos.x - 1.0) << describe(seed, i, c, M, B.pos);
        ASSERT_LE(D.pos.y, R.pos.y - 1.0) << describe(seed, i, c, M, B.pos);
        ASSERT_GE(D.pos.x + D.size.x, R.pos.x + R.size.x + 1.0) << describe(seed, i, c, M, B.pos);
        ASSERT_GE(D.pos.y + D.size.y, R.pos.y + R.size.y + 1.0) << describe(seed, i, c, M, B.pos);
        ASSERT_LT(D.size.x, R.size.x + 4.0) << describe(seed, i, c, M, B.pos);
    }
    // Hand value: z = 2 maps [0.25, 10.25] x [0.5, 10.5] to [0.5, 20.5] x [1, 21] -> [-1, 22] x [0, 22].
    const auto D = cm::damageBox({{0.0, 0.0}, 2.0}, {0.0, 0.0}, {{0.25, 0.5}, {10.0, 10.0}});
    EXPECT_EQ(D.pos.x, -1.0);
    EXPECT_EQ(D.pos.y, 0.0);
    EXPECT_EQ(D.size.x, 23.0);
    EXPECT_EQ(D.size.y, 22.0);
}

// Invariant 7: V1 equivalence, compared with ==.
TEST(CameraMathTest, V1CameraIsTheContractWithZeroMonitorOrigin) {
    const auto seed = testSeed();
    Random rng(seed);
    const cm::ZoomLimits unbounded{std::numeric_limits<double>::min(), std::numeric_limits<double>::max()};
    for (int i = 0; i < kCases; ++i) {
        const cm::Vec P = rng.point();
        const double z = (i % 7 == 0) ? 1.0 : rng.zoom();
        const cm::Vec W = rng.point();
        const cm::CameraState c{P, z};
        hyprvx::Camera camera{{P.x, P.y}, z};
        const auto S = camera.worldToScreen({W.x, W.y});
        const auto expectedS = cm::forward(c, {}, W);
        ASSERT_EQ(S.x, expectedS.x) << describe(seed, i, c, {}, W);
        ASSERT_EQ(S.y, expectedS.y) << describe(seed, i, c, {}, W);
        const auto back = camera.screenToWorld({W.x, W.y});
        const auto expectedBack = cm::inverse(c, {}, W);
        ASSERT_EQ(back.x, expectedBack.x) << describe(seed, i, c, {}, W);
        ASSERT_EQ(back.y, expectedBack.y) << describe(seed, i, c, {}, W);
        const double zNew = rng.zoom();
        const cm::Vec a = rng.point();
        const auto expectedAnchor = cm::anchorZoom(c, {}, a, zNew, unbounded);
        camera.setZoomAroundScreenPoint(zNew, {a.x, a.y});
        ASSERT_EQ(camera.position().x, expectedAnchor.origin.x) << describe(seed, i, c, {}, a);
        ASSERT_EQ(camera.position().y, expectedAnchor.origin.y) << describe(seed, i, c, {}, a);
        ASSERT_EQ(camera.zoom(), expectedAnchor.zoom) << describe(seed, i, c, {}, a);
    }
}
