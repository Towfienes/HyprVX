// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, the HyprVX authors.
//
// HyprVX camera transform: the single source of truth for world <-> screen mapping (Constitution IV).
// Contract: specs/001-native-spatial-camera/contracts/transform-contract.md, version 1.1.
//
// Header-only, C++20, standard library only. This file is vendored unchanged into the Hyprland patch series;
// it must not depend on hyprvx_core. Adapters to other vector/box types live at the call sites and contain no
// arithmetic beyond field copies.

#pragma once

#include <cmath>

namespace hyprvx::cammath {

struct Vec {
    double x = 0.0, y = 0.0;
};

struct Box {
    Vec pos;
    Vec size;
};

// P (world point shown at the monitor origin M) and z.
struct CameraState {
    Vec    origin;
    double zoom = 1.0;
};

// MVP range; callers may pass others.
struct ZoomLimits {
    double min = 0.1;
    double max = 2.0;
};

enum class Error {
    None,
    NonFinite,
    NonPositiveZoom,
    ZoomOutOfRange,
};

// Exact compare: identity is set exactly by identityFor() (reset/panic), never reached by arithmetic.
[[nodiscard]] constexpr bool isIdentity(const CameraState& c, Vec M) noexcept {
    return c.origin.x == M.x && c.origin.y == M.y && c.zoom == 1.0;
}

// world -> screen-logical: M + (W - P)·z. Identity returns W unchanged, because M + (W - M) is not exact.
[[nodiscard]] constexpr Vec forward(const CameraState& c, Vec M, Vec W) noexcept {
    if (isIdentity(c, M))
        return W;
    return {M.x + (W.x - c.origin.x) * c.zoom, M.y + (W.y - c.origin.y) * c.zoom};
}

// screen-logical -> world: P + (S - M)/z. Identity returns S unchanged.
[[nodiscard]] constexpr Vec inverse(const CameraState& c, Vec M, Vec S) noexcept {
    if (isIdentity(c, M))
        return S;
    return {c.origin.x + (S.x - M.x) / c.zoom, c.origin.y + (S.y - M.y) / c.zoom};
}

[[nodiscard]] constexpr Box forwardBox(const CameraState& c, Vec M, Box B) noexcept {
    if (isIdentity(c, M))
        return B;
    return {forward(c, M, B.pos), {B.size.x * c.zoom, B.size.y * c.zoom}};
}

[[nodiscard]] constexpr Box inverseBox(const CameraState& c, Vec M, Box R) noexcept {
    if (isIdentity(c, M))
        return R;
    return {inverse(c, M, R.pos), {R.size.x / c.zoom, R.size.y / c.zoom}};
}

[[nodiscard]] constexpr Box visibleWorldRect(const CameraState& c, Vec monitorSize) noexcept {
    return {c.origin, {monitorSize.x / c.zoom, monitorSize.y / c.zoom}};
}

// Unscaled surface units. Identity yields S - surfaceOrigin through the inverse short-circuit.
[[nodiscard]] constexpr Vec surfaceLocal(const CameraState& c, Vec M, Vec S, Vec surfaceOrigin) noexcept {
    const Vec W = inverse(c, M, S);
    return {W.x - surfaceOrigin.x, W.y - surfaceOrigin.y};
}

// Screen damage for a window-owned world box: roundOut(forwardBox(worldBox)) expanded by 1 px on every side.
[[nodiscard]] inline Box damageBox(const CameraState& c, Vec M, Box worldBox) noexcept {
    const Box    R  = forwardBox(c, M, worldBox);
    const double x0 = std::floor(R.pos.x) - 1.0;
    const double y0 = std::floor(R.pos.y) - 1.0;
    const double x1 = std::ceil(R.pos.x + R.size.x) + 1.0;
    const double y1 = std::ceil(R.pos.y + R.size.y) + 1.0;
    return {{x0, y0}, {x1 - x0, y1 - y0}};
}

[[nodiscard]] inline Error validate(const CameraState& c, ZoomLimits limits) noexcept {
    if (!std::isfinite(c.origin.x) || !std::isfinite(c.origin.y) || !std::isfinite(c.zoom))
        return Error::NonFinite;
    if (!(c.zoom > 0.0))
        return Error::NonPositiveZoom;
    if (c.zoom < limits.min || c.zoom > limits.max)
        return Error::ZoomOutOfRange;
    return Error::None;
}

// Keeps the world point under screen anchor a fixed: W_a = inverse(c, M, a); P' = W_a - (a - M)/z'.
// zNew is clamped to limits; non-finite a or zNew returns c unchanged.
[[nodiscard]] inline CameraState anchorZoom(const CameraState& c, Vec M, Vec a, double zNew, ZoomLimits limits) noexcept {
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(zNew))
        return c;
    const double z  = zNew < limits.min ? limits.min : (zNew > limits.max ? limits.max : zNew);
    const Vec    Wa = inverse(c, M, a);
    return {{Wa.x - (a.x - M.x) / z, Wa.y - (a.y - M.y) / z}, z};
}

// P' = P - d/z: content follows a screen-space drag of d.
[[nodiscard]] constexpr CameraState panByScreen(const CameraState& c, Vec d) noexcept {
    return {{c.origin.x - d.x / c.zoom, c.origin.y - d.y / c.zoom}, c.zoom};
}

[[nodiscard]] constexpr CameraState identityFor(Vec M) noexcept {
    return {M, 1.0};
}

} // namespace hyprvx::cammath
