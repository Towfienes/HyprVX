#pragma once

#include <algorithm>

#include "Vec2.hpp"

namespace hyprvx {

struct Rect {
    Vec2 position{};
    Vec2 size{};

    [[nodiscard]]
    constexpr double left() const {
        return position.x;
    }

    [[nodiscard]]
    constexpr double top() const {
        return position.y;
    }

    [[nodiscard]]
    constexpr double right() const {
        return position.x + size.x;
    }

    [[nodiscard]]
    constexpr double bottom() const {
        return position.y + size.y;
    }

    [[nodiscard]]
    constexpr Vec2 center() const {
        return {
            position.x + size.x / 2.0,
            position.y + size.y / 2.0
        };
    }

    [[nodiscard]]
    constexpr bool contains(const Vec2& point) const {
        return
            point.x >= left() &&
            point.x <= right() &&
            point.y >= top() &&
            point.y <= bottom();
    }

    [[nodiscard]]
    constexpr Rect translated(const Vec2& delta) const {
        return {
            position + delta,
            size
        };
    }

    [[nodiscard]]
    static Rect united(const Rect& a, const Rect& b) {
        const double l = std::min(a.left(), b.left());
        const double t = std::min(a.top(), b.top());
        const double r = std::max(a.right(), b.right());
        const double bt = std::max(a.bottom(), b.bottom());

        return {
            {l, t},
            {r - l, bt - t}
        };
    }
};

}
