#pragma once

#include <cmath>

namespace hyprvx {

struct Vec2 {
    double x{};
    double y{};

    constexpr Vec2() = default;

    constexpr Vec2(double xValue, double yValue)
        : x(xValue), y(yValue) {}

    constexpr Vec2 operator+(const Vec2& other) const {
        return {x + other.x, y + other.y};
    }

    constexpr Vec2 operator-(const Vec2& other) const {
        return {x - other.x, y - other.y};
    }

    constexpr Vec2 operator*(double scalar) const {
        return {x * scalar, y * scalar};
    }

    constexpr Vec2 operator/(double scalar) const {
        return {x / scalar, y / scalar};
    }

    constexpr Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2& operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    [[nodiscard]]
    double lengthSquared() const {
        return x * x + y * y;
    }

    [[nodiscard]]
    double length() const {
        return std::sqrt(lengthSquared());
    }
};

constexpr Vec2 operator*(double scalar, const Vec2& value) {
    return value * scalar;
}

}
